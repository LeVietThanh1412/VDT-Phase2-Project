#include "NavalBattlefieldSubsystem.h"
#include "SeaNavigationMap.h"
#include "Private/NavalWaterQueries.h"
#include "NavalSectorSubsystem.h"
#include "NavalMatchSettings.h"
#include "NavalBattleUI.h"
#include "CustomPlayerController.h"
#include "ShipCombatComponent.h"
#include "ShipEnemyNavigationComponent.h"
#include "ShipMovementComponent.h"
#include "SeaTerrainCollisionComponent.h"
#include "Cesium3DTileset.h"
#include "CesiumSunSky.h"
#include "CesiumGlobeAnchorComponent.h"
#include "CesiumGeoreference.h"
#include "BuoyancyComponent.h"
#include "WaterBodyComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "CesiumCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Json.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/TargetPoint.h"

namespace
{
float SpawnHullRadius(APawn* Pawn)
{const auto* H=UShipCombatComponent::PhysicalHull(Pawn);if(!H)return 1000;const auto B=H->GetLocalBounds().GetBox();const auto S=H->GetComponentScale().GetAbs();return FVector2D(FMath::Max(FMath::Abs(B.Min.X),FMath::Abs(B.Max.X))*S.X,FMath::Max(FMath::Abs(B.Min.Y),FMath::Abs(B.Max.Y))*S.Y).Size()/100;}

}
bool UNavalBattlefieldSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{return Type==EWorldType::Game || Type==EWorldType::PIE;}
TStatId UNavalBattlefieldSubsystem::GetStatId() const
{RETURN_QUICK_DECLARE_CYCLE_STAT(UNavalBattlefieldSubsystem,STATGROUP_Tickables);}
bool UNavalBattlefieldSubsystem::IsEnabled() const
{const auto* S=GetWorld() && GetWorld()->GetGameInstance()?GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>():nullptr;return S && S->bUseSectorBattlefield;}
bool UNavalBattlefieldSubsystem::AreTacticsEnabled() const
{const auto* S=GetWorld() && GetWorld()->GetGameInstance()?GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>():nullptr;return IsEnabled() && S && S->bEnableSectorTactics;}
bool UNavalBattlefieldSubsystem::IsDefensiveTargetAllowed(AActor* Ship,AActor* Target) const
{
    if(!bReady || !AreTacticsEnabled())return true;
    const auto* A=Assignments.FindByPredicate([Ship](const FNavalSpawnAssignment& Item){return Item.Ship==Ship && Item.Team==0 && !Item.Ship->IsPlayerControlled();});
    if(!A)return true;
    if(!IsValid(Target))return false;
    // Defense bounds restrict movement, not a legitimate shot at a nearby
    // attacker across the sector line. The shared gun still enforces range,
    // LOS, mechanical arc, alignment and the real ballistic path.
    if(const auto* C=Ship->FindComponentByClass<UShipCombatComponent>();C &&
        FVector::Distance(Ship->GetActorLocation(),Target->GetActorLocation())<=C->EngagementRadiusM*100)return true;
    const auto Enemy=GetWorld()->GetSubsystem<UNavalSectorSubsystem>()->WorldToChart(Target->GetActorLocation());
    return Enemy.X<=14750 && FVector2D::Distance(Enemy,A->DefensiveAnchorEN)<=DefensiveLeashM+250;
}
void UNavalBattlefieldSubsystem::Fail(const FString& Reason)
{bReady=false;InitializationError=Reason;Status=TEXT("Battlefield setup failed: ")+Reason;UE_LOG(LogTemp,Warning,TEXT("NavalBattlefield seed=%d SETUP FAILED: %s"),ActualSeed,*Reason);if(auto* B=GetWorld()->GetSubsystem<UNavalBattleSubsystem>()){B->ConfigurationError=Status;}}
void UNavalBattlefieldSubsystem::SpawnLoadingCamera(const FVector& Eye,const FVector& Focus,bool bFocusPlayerView)
{
    LoadingCamera=GetWorld()->SpawnActor<ACameraActor>(Eye,(Focus-Eye).Rotation());
    if(!LoadingCamera)return;
    FCesiumCamera View;View.ParameterSource=ECameraParameterSource::CameraComponent;
    View.CameraComponent=LoadingCamera->GetCameraComponent();View.ViewportSize=FVector2D(8192,8192);
    // A fixed, non-rendering Cesium view keeps the field streamed
    // independently of the spectator/player camera direction.
    StreamingCameraManager=ACesiumCameraManager::GetDefaultCameraManager(this);
    if(StreamingCameraManager.IsValid())StreamingCameraManager->AdditionalCameras.Add(View);
    if(bFocusPlayerView)
        if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetViewTarget(LoadingCamera);
}
void UNavalBattlefieldSubsystem::InitializeFleet(const TArray<APawn*>& Ships)
{
    if(LoadingCamera)
    {
        if(auto* Manager=StreamingCameraManager.Get())Manager->AdditionalCameras.RemoveAll([this](const FCesiumCamera& C){return C.CameraComponent.Get()==LoadingCamera->GetCameraComponent();});
        LoadingCamera->Destroy();LoadingCamera=nullptr;
    }
    StreamingCameraManager.Reset();
    bInitialized=true;bReady=false;InitializedAt=GetWorld()->GetTimeSeconds();PlacedAt=-1;LayoutAttempts=0;InitializationError.Empty();Assignments.Reset();
    RetainedFleet.Reset();for(auto* P:Ships)RetainedFleet.Add(P);
    PhysicalLayoutRetries=0;bUsedSafeFallback=false;bPhysicalSearch=false;LastPhysicalRejection.Empty();TerrainStableSince=-1;OperationalCharts.Reset();
    AssaultLeader.Reset();AssaultLeaderChanges=0;bAssaultInitialized=false;LastAssaultUpdateAt=-1000;ScoutContactAt=-1000;
    ObservedContacts.Reset();
    auto* S=GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>();
    bUsingAuthoredLayout=!IsEnabled();
    if(bUsingAuthoredLayout)
    {
        ActualSeed=S->BattlefieldSeed>0?S->BattlefieldSeed:FMath::RandRange(1,MAX_int32);
        FBox Coverage(ForceInit);
        for(APawn* Ship:Ships)
        {
            Coverage+=Ship->GetActorLocation();
            if(auto* Nav=Ship->FindComponentByClass<UShipEnemyNavigationComponent>())
            {
                FString Error;
                // Validate references now; the exact directed routes must use
                // the physical chart built after this authored area is streamed.
                if(!Nav->PrepareAuthoredPatrol(Error,false)){Fail(Error);return;}
                Coverage+=Nav->PatrolActorA->GetActorLocation();Coverage+=Nav->PatrolActorB->GetActorLocation();
            }
        }
        // Stream the user's area, without changing any authored pose or marker.
        if (Coverage.IsValid)
        {
            const FVector Focus = Coverage.GetCenter();
            const FVector Eye = Focus + FVector(0, 0, FMath::Max(230000., Coverage.GetExtent().GetMax() * 1.5));
            SpawnLoadingCamera(Eye, Focus, false);
        }
        Status=TEXT("MY MAP: settling authored ships; patrol starts at A");
        if(auto* Battle=GetWorld()->GetSubsystem<UNavalBattleSubsystem>())Battle->ConfigurationError=Status;
        return;
    }
    if(S->bUseIslandPatrolCircuit && AreTacticsEnabled())
        for(TActorIterator<ACesiumSunSky> Sky(GetWorld());Sky;++Sky)
        {Sky->SolarTime=10.5;Sky->UseDaylightSavingTime=false;Sky->UpdateSun();}
    ActualSeed=S->BattlefieldSeed>0?S->BattlefieldSeed:FMath::RandRange(1,MAX_int32);LayoutRandom.Initialize(ActualSeed);
    auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();
    if(!Grid || !Grid->bGridReady){Fail(Grid?Grid->GridError:TEXT("Sector subsystem missing"));return;}
    if(!GenerateLayout(Ships))
    {
        bUsedSafeFallback=true;LayoutRandom.Initialize(20261028);
        if(!GenerateLayout(Ships)){Fail(TEXT("No separated, connected layout after bounded search and checked fallback"));return;}
    }
    const FVector Focus=Grid->ChartToWorld(Grid->CenterEN),Eye=Focus+FVector(0,0,230000);
    SpawnLoadingCamera(Eye,Focus,true);
    Status=TEXT("Preparing battlefield...");
    if(auto* B=GetWorld()->GetSubsystem<UNavalBattleSubsystem>()){B->ConfigurationError=Status;}
}
TSharedPtr<const FSeaNavigationMap> UNavalBattlefieldSubsystem::NavigationChartForShip(APawn* Ship) const
{
    const auto* M=Ship?Ship->FindComponentByClass<UShipMovementComponent>():nullptr;
    if(M)if(const auto* Chart=OperationalCharts.Find(M->HullClass))return *Chart;
    return GetWorld()->GetSubsystem<UNavalSectorSubsystem>()->NavigationChart();
}
FString UNavalBattlefieldSubsystem::InspectNavigationChart(APawn* Ship) const
{
    const auto Map=NavigationChartForShip(Ship);auto J=MakeShared<FJsonObject>();
    if(Map.IsValid())
    {
        J->SetNumberField(TEXT("width"),Map->Width);J->SetNumberField(TEXT("height"),Map->Height);
        J->SetNumberField(TEXT("cell_size_m"),Map->CellSize);J->SetNumberField(TEXT("min_east"),Map->MinEast);J->SetNumberField(TEXT("min_north"),Map->MinNorth);
        J->SetStringField(TEXT("cells_base64"),FBase64::Encode(Map->Cells));
        J->SetBoolField(TEXT("surveyed"),bPhysicalSearch);
    }
    FString Out;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Out));return Out;
}
FString UNavalBattlefieldSubsystem::InspectNavigationRoute(APawn* Ship,FVector2D Start,FVector2D Goal) const
{
    const auto Map=NavigationChartForShip(Ship);auto J=MakeShared<FJsonObject>();
    FSeaRouteResult Route;if(Map.IsValid())Route=Map->FindPath(Start,Goal,75000,250);
    J->SetBoolField(TEXT("success"),Route.bSuccess);J->SetStringField(TEXT("failure"),Route.Failure);
    J->SetNumberField(TEXT("expansions"),Route.Expansions);J->SetNumberField(TEXT("milliseconds"),Route.Milliseconds);
    TArray<TSharedPtr<FJsonValue>> Points;
    for(const auto& P:Route.Points)Points.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y)}));
    J->SetArrayField(TEXT("points"),Points);
    FString Out;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Out));return Out;
}
bool UNavalBattlefieldSubsystem::ValidateDevelopmentPlacement(APawn* Ship,FVector2D Position,float HeadingDegrees)
{
    if(!bReady || !IsValid(Ship) || !NavigationChartForShip(Ship)->IsWater(Position))return false;
    FNavalSpawnAssignment A;A.Ship=Ship;A.SpawnEN=A.TargetEN=Position;A.HeadingDegrees=HeadingDegrees;A.HullRadiusM=SpawnHullRadius(Ship);
    FSeaRouteResult Route;Route.bSuccess=true;Route.Points.Add(Position);bool Waiting=false;
    return CheckPhysicalAssignment(A,Route,Waiting);
}
bool UNavalBattlefieldSubsystem::BuildOperationalCharts()
{
    auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();const auto Coast=Grid->NavigationChart();auto* Ocean=NavalWaterQueries::FindOcean(GetWorld());
    if(!Coast.IsValid() || !Ocean){Fail(TEXT("Cannot survey terrain without chart and ocean"));return false;}
    FCollisionQueryParams Q(SCENE_QUERY_STAT(NavalOperationalChart),true);
    NavalWaterQueries::IgnoreWaterActors(GetWorld(),Q);
    for(APawn* P:RetainedFleet)Q.AddIgnoredActor(P);
    for(APawn* P:RetainedFleet)
    {
        auto* M=P->FindComponentByClass<UShipMovementComponent>();auto* H=UShipCombatComponent::PhysicalHull(P);
        if(!M || !H || OperationalCharts.Contains(M->HullClass))continue;
        auto Chart=MakeShared<FSeaNavigationMap>(*Coast);const FBox B=H->GetLocalBounds().GetBox();const FVector Scale=H->GetComponentScale().GetAbs();
        // Logical sectors stay 100 m. Collision sampling uses a lightweight
        // local 25 m grid, retaining the exact ECEF/ENU frame and original coast.
        const auto* Settings=GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>();
        // The drawing's west passage touches the spawn box edge. Survey an
        // additional western corridor instead of reducing real hull clearance.
        // The original 625 logical sectors and all spawn bounds remain intact.
        const double WestExtension=AreTacticsEnabled() && Settings->bUseIslandPatrolCircuit?600.:0.;
        Chart->CellSize=25;
        if(bUsingAuthoredLayout)
        {
            FBox2D Bounds(ForceInit);
            for(APawn* Ship:RetainedFleet)
            {
                Bounds+=Grid->WorldToChart(Ship->GetActorLocation());
                if(const auto* Nav=Ship->FindComponentByClass<UShipEnemyNavigationComponent>())
                {if(Nav->PatrolActorA)Bounds+=Grid->WorldToChart(Nav->PatrolActorA->GetActorLocation());if(Nav->PatrolActorB)Bounds+=Grid->WorldToChart(Nav->PatrolActorB->GetActorLocation());}
            }
            Bounds=Bounds.ExpandBy(500.);
            Chart->MinEast=FMath::FloorToDouble(Bounds.Min.X/25)*25;Chart->MinNorth=FMath::FloorToDouble(Bounds.Min.Y/25)*25;
            Chart->Width=FMath::CeilToInt((Bounds.Max.X-Chart->MinEast)/25);Chart->Height=FMath::CeilToInt((Bounds.Max.Y-Chart->MinNorth)/25);
            if(int64(Chart->Width)*Chart->Height>250000){Fail(TEXT("MY MAP: authored area is too large for the bounded local collision survey"));return false;}
        }
        else
        {
            Chart->Width=FMath::RoundToInt((Grid->WidthM+WestExtension)/25);Chart->Height=FMath::RoundToInt(Grid->HeightM/25);
            Chart->MinEast=Grid->CenterEN.X-Grid->WidthM*.5-WestExtension;Chart->MinNorth=Grid->CenterEN.Y-Grid->HeightM*.5;
        }
        Chart->Cells.Init(0,Chart->Width*Chart->Height);Chart->Clearance.Init(0,Chart->Cells.Num());
        const float Radius=SpawnHullRadius(P);
        FVector Extent=B.GetExtent()*Scale+FVector(200);Extent.X=Extent.Y=(Radius+2)*100;
        for(int32 Id=0;Id<Chart->Cells.Num();++Id)
        {
            const auto Center=Chart->Center(Id);
            const double Inset=Radius+30+Chart->CellSize*.5;
            bool Clear=Coast->IsUnbufferedWater(Center) && Center.X>=Chart->MinEast+Inset && Center.X<=Chart->MinEast+Chart->Width*25-Inset &&
                Center.Y>=Chart->MinNorth+Inset && Center.Y<=Chart->MinNorth+Chart->Height*25-Inset;
            if(Clear)for(int32 Corner=0;Corner<4 && Clear;++Corner)
            {
                const FVector2D EN=Center+FVector2D(Corner&1?.5:-.5,Corner&2?.5:-.5)*Chart->CellSize;
                FVector World=Grid->ChartToWorld(EN),Water,Normal,Velocity;float Depth=0;
                Ocean->GetWaterSurfaceInfoAtLocation(World,Water,Normal,Velocity,Depth,false);World.Z=Water.Z;
                bool Covered=false;
                for(TActorIterator<ACesium3DTileset> I(GetWorld());I;++I)if(const auto* F=I->FindComponentByClass<USeaTerrainCollisionComponent>();F && F->HasCollisionCoverageAt(World)){Covered=true;break;}
                Clear=Covered && !GetWorld()->OverlapBlockingTestByChannel(World+B.GetCenter()*Scale,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeBox(Extent),Q);
            }
            Chart->Cells[Id]=Clear?1:0;
            if(Clear){const int32 OriginalId=Coast->CellAt(Center);Chart->Clearance[Id]=Coast->Clearance.IsValidIndex(OriginalId)?Coast->Clearance[OriginalId]:0;}
        }
        Chart->RebuildConnectedRegions();
        OperationalCharts.Add(M->HullClass,Chart);
    }
    return !OperationalCharts.IsEmpty();
}
bool UNavalBattlefieldSubsystem::GenerateLayout(const TArray<APawn*>& Ships)
{
    const auto* Settings=GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>();
    if(AreTacticsEnabled() && Settings->bUseIslandPatrolCircuit)
    {
        if(!bPhysicalSearch){Assignments.Reset();return true;}
        return GenerateIslandLayout(Ships);
    }
    auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();const auto SourceMap=Grid->NavigationChart();
    TArray<APawn*> Blue,Red;
    for(auto* P:Ships)if(IsValid(P))if(auto* W=P->FindComponentByClass<UShipWeaponComponent>()){if(W->Team()==0)Blue.Add(P);else if(W->Team()==1)Red.Add(P);}
    auto Sort=[](const APawn& A,const APawn& B){return A.GetFName().LexicalLess(B.GetFName());};Blue.Sort(Sort);Red.Sort(Sort);
    if(auto* Player=UGameplayStatics::GetPlayerPawn(this,0);Blue.Remove(Player))Blue.Insert(Player,0);
    // Prefer the actual selected Small hull as the leading scout, keeping all
    // selected classes and pawn identities intact in custom fleet presets.
    int32 Scout=INDEX_NONE,BestClass=MAX_int32;
    for(int32 I=0;I<Red.Num();++I)if(!Red[I]->IsPlayerControlled())if(const auto* M=Red[I]->FindComponentByClass<UShipMovementComponent>())
    {const int32 Rank=M->HullClass==EShipHullClass::Small?0:M->HullClass==EShipHullClass::Medium?1:2;if(Rank<BestClass){BestClass=Rank;Scout=I;}}
    if(Scout>0){APawn* P=Red[Scout];Red.RemoveAt(Scout);Red.Insert(P,0);}
    if(Blue.IsEmpty() || Red.IsEmpty())return false;
    TArray<int32> Defense,Patrol,NorthWatch,Staging,LeadStaging,Approach;
    for(int32 I=0;I<Grid->Sectors.Num();++I)
    {
        const auto& S=Grid->Sectors[I];if(!S.bSpawnEligible)continue;
        if(S.Zones.Contains(TEXT("BLUE_DEFENSE_CORE")) && S.AnchorEN.X<14450)Defense.Add(I);
        if(S.Zones.Contains(TEXT("BLUE_WEST_ISLAND_PATROL")) && S.AnchorEN.X<14450)Patrol.Add(I);
        if(S.Zones.Contains(TEXT("BLUE_NORTH_WATCH")) && S.AnchorEN.X<14450)NorthWatch.Add(I);
        if(S.Zones.Contains(TEXT("RED_STAGING")))
        {Staging.Add(I);if(S.AnchorEN.Y<4200)LeadStaging.Add(I);}
        if(S.Zones.Contains(TEXT("RED_APPROACH")))Approach.Add(I);
    }
    if(Defense.IsEmpty() || Patrol.IsEmpty() || Staging.IsEmpty() || LeadStaging.IsEmpty() || Approach.IsEmpty())return false;
    auto Pick=[&](const TArray<int32>& Pool){return Pool[LayoutRandom.RandRange(0,Pool.Num()-1)];};
    auto SafePoint=[&](APawn* P,FVector2D EN,float Heading)
    {
        const auto Map=NavigationChartForShip(P);
        const float Radius=SpawnHullRadius(P)+30;
        if(!Grid->IsInsideField(EN,Radius) || !Map->IsWater(EN))return false;
        const auto* Hull=UShipCombatComponent::PhysicalHull(P);const auto Box=Hull->GetLocalBounds().GetBox();const auto Scale=Hull->GetComponentScale().GetAbs();
        for(int32 Corner=0;Corner<4;++Corner)
        {
            const FVector2D Local((Corner&1?Box.Max.X:Box.Min.X)*Scale.X/100,-(Corner&2?Box.Max.Y:Box.Min.Y)*Scale.Y/100);
            const auto Q=EN+Local.GetRotated(-Heading);
            if(!SourceMap->IsWater(Q) || !SourceMap->SegmentClear(EN,Q))return false;
        }
        return true;
    };
    for(LayoutAttempts=1;LayoutAttempts<=512;++LayoutAttempts)
    {
        Assignments.Reset();TArray<TArray<FVector2D>> AcceptedRoutes;bool Good=true;
        auto Add=[&](APawn* P,int32 Team,int32 Slot,int32 Cell,FVector2D Goal)
        {
            const auto Map=NavigationChartForShip(P);
            const auto& S=Grid->Sectors[Cell];auto Start=S.SamplesEN[LayoutRandom.RandRange(0,S.SamplesEN.Num()-1)];
            if(bPhysicalSearch)
            {
                const int32 StartCell=Map->CellAt(Start),GoalCell=Map->CellAt(Goal);
                if(StartCell==INDEX_NONE || GoalCell==INDEX_NONE)return false;
                Start=Map->Center(StartCell);Goal=Map->Center(GoalCell);
            }
            const bool Player=P->IsPlayerControlled();if(Player)Goal=Start;
            if(const auto* N=P->FindComponentByClass<UShipEnemyNavigationComponent>();N && FVector2D::Distance(Start,Goal)<=N->ArrivalRadiusM*2+20)return false;
            FSeaRouteResult Route;
            if(Player){Route.bSuccess=true;Route.Points.Add(Start);}
            else Route=Map->FindPath(Start,Goal,75000,250);
            if(!Route.bSuccess || (!Player && Route.Points.Num()<2))return false;
            for(const auto& Q:Route.Points)if(!Grid->IsInsideField(Q))return false;
            int32 First=1;
            while(First<Route.Points.Num()-1 && FVector2D::Distance(Start,Route.Points[First])<80 && Map->SegmentClear(Start,Route.Points[First+1]))++First;
            const auto D=Player?FVector2D(Team==0?1:-1,0):Route.Points[First]-Start;const float Heading=FMath::RadiansToDegrees(FMath::Atan2(-D.Y,D.X));
            if(!SafePoint(P,Start,Heading) || !SafePoint(P,Goal,Heading))return false;
            const float Radius=SpawnHullRadius(P);
            for(int32 OtherIndex=0;OtherIndex<Assignments.Num();++OtherIndex)
            {
                const auto& Other=Assignments[OtherIndex];
                const float Separation=FMath::Max(220.f,Radius+Other.HullRadiusM+65);
                if(FVector2D::Distance(Start,Other.SpawnEN)<Separation)return false;
                if(Team==Other.Team && (Team==1 || (!Player && !Other.Ship->IsPlayerControlled())) && FVector2D::Distance(Goal,Other.TargetEN)<220)return false;
                if(Team==0 && Other.Team==0)
                {
                    for(int32 K=1;K<Route.Points.Num();++K)
                        if(FMath::PointDistToSegment(FVector(Other.SpawnEN,0),FVector(Route.Points[K-1],0),FVector(Route.Points[K],0))<FMath::Max(300.f,Separation))return false;
                    if(!Other.Ship->IsPlayerControlled())for(int32 K=1;K<AcceptedRoutes[OtherIndex].Num();++K)
                    {
                        const auto& Prior=AcceptedRoutes[OtherIndex];
                        if(FMath::PointDistToSegment(FVector(Start,0),FVector(Prior[K-1],0),FVector(Prior[K],0))<FMath::Max(300.f,Separation))return false;
                    }
                }
            }
            FNavalSpawnAssignment A;A.Ship=P;A.Team=Team;A.Slot=Slot;A.SpawnSector=S.Id;A.SpawnEN=Start;A.TargetEN=Goal;A.HeadingDegrees=Heading;A.HullRadiusM=Radius;A.DefensiveAnchorEN=(Start+Goal)*.5;
            bool Waiting=false;if(bPhysicalSearch && !CheckPhysicalAssignment(A,Route,Waiting))return false;
            Assignments.Add(A);AcceptedRoutes.Add(Route.Points);return true;
        };
        for(int32 I=0;I<Blue.Num() && Good;++I)
        {
            const auto Map=NavigationChartForShip(Blue[I]);
            // Red-player mode adds a third Blue AI. Reserve the existing north-watch
            // corridor for that extra patrol instead of crowding all three into west routes.
            const auto& PatrolPool=I==0 && !Blue[I]->IsPlayerControlled() && !NorthWatch.IsEmpty()?NorthWatch:Patrol;
            // Keep each already accepted defender while trying the next slot.
            // Restarting the entire six-ship draw on one unsafe sample makes
            // the probability of a valid mixed-hull layout needlessly tiny.
            bool Placed=false;
            for(int32 PlacementTry=0;PlacementTry<48 && !Placed;++PlacementTry)
            {
                const int32 Cell=Pick(Defense);const auto Start=Grid->Sectors[Cell].AnchorEN;FVector2D Goal=Start;bool Found=Blue[I]->IsPlayerControlled();
                for(int32 Try=0;Try<32 && !Found;++Try)
                {const auto Candidate=Grid->Sectors[Pick(PatrolPool)].AnchorEN;const double D=FVector2D::Distance(Start,Candidate);if(D>350 && D<1000 && Map->FindPath(Start,Candidate,75000,250).bSuccess){Goal=Candidate;Found=true;}}
                if(Found)Placed=Add(Blue[I],0,I,Cell,Goal);
            }
            Good=Placed;
        }
        if(!Good)continue;
        const int32 LeadCell=Pick(LeadStaging),GoalCell=Pick(Approach);const FVector2D Lead=Grid->Sectors[LeadCell].AnchorEN,Goal=Grid->Sectors[GoalCell].AnchorEN;
        const auto Back=(Lead-Goal).GetSafeNormal();const FVector2D Side(-Back.Y,Back.X);const float Spacing=LayoutRandom.FRandRange(235,270);
        for(int32 I=0;I<Red.Num() && Good;++I)
        {
            int32 Cell=LeadCell;
            if(I>0)
            {
                const auto Desired=Lead+Back*Spacing*I+Side*(I%2?45:-45);
                double Best=DBL_MAX;
                for(int32 Candidate:Staging){const double D=FVector2D::Distance(Grid->Sectors[Candidate].AnchorEN,Desired);if(D<Best){Best=D;Cell=Candidate;}}
                if(Best>100){Good=false;break;}
            }
            // Trailing column goals, rather than three abreast at a narrow
            // island approach. Each follower still plans its own real A* route.
            const FVector2D Objective=Goal+Back*Spacing*I+Side*(I==0?0:I%2?45:-45);
            Good=Add(Red[I],1,I,Cell,Objective);
        }
        if(Good && Assignments.Num()==Ships.Num())return true;
    }
    Assignments.Reset();return false;
}
bool UNavalBattlefieldSubsystem::GenerateIslandLayout(const TArray<APawn*>& Ships)
{
    auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();
    FString Text;TSharedPtr<FJsonObject> Data;
    if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("SeaNavigation/quangninh_demo_routes.json"))) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data) || !Data.IsValid())
    {LastPhysicalRejection=TEXT("Island demo route configuration missing/invalid");return false;}
    auto ReadPoints=[&](const TCHAR* Key,TArray<FVector2D>& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
        if(!Data->TryGetArrayField(Key,Values))return false;
        for(const auto& Value:*Values)
        {
            const TArray<TSharedPtr<FJsonValue>>* XY=nullptr;
            if(!Value->TryGetArray(XY) || XY->Num()!=2)return false;
            double X=0,Y=0;if(!(*XY)[0]->TryGetNumber(X) || !(*XY)[1]->TryGetNumber(Y) || !FMath::IsFinite(X) || !FMath::IsFinite(Y))return false;
            Out.Emplace(X,Y);
        }
        return true;
    };
    TArray<FVector2D> Loop,Heavy,RedSpawns,RedGoals;
    if(!ReadPoints(TEXT("blue_small_circuit_en_m"),Loop) || Loop.Num()<6 || !ReadPoints(TEXT("blue_heavy_patrol_en_m"),Heavy) || Heavy.Num()!=2 ||
        !ReadPoints(TEXT("red_spawn_hints_en_m"),RedSpawns) || RedSpawns.Num()!=3 || !ReadPoints(TEXT("red_approach_hints_en_m"),RedGoals) || RedGoals.Num()!=3)
    {LastPhysicalRejection=TEXT("Island demo route points are malformed");return false;}
    TArray<APawn*> Teams[2];
    for(APawn* P:Ships)if(IsValid(P))if(const auto* W=P->FindComponentByClass<UShipWeaponComponent>();W && W->Team()>=0 && W->Team()<2)Teams[W->Team()].Add(P);
    for(auto& Team:Teams)Team.Sort([](const APawn& A,const APawn& B){return A.GetFName().LexicalLess(B.GetFName());});
    if(auto* Player=UGameplayStatics::GetPlayerPawn(this,0))for(auto& Team:Teams)if(Team.Remove(Player))Team.Insert(Player,0);
    Teams[1].StableSort([](const APawn& A,const APawn& B)
    {
        const auto* MA=A.FindComponentByClass<UShipMovementComponent>();const auto* MB=B.FindComponentByClass<UShipMovementComponent>();
        auto Rank=[](const UShipMovementComponent* M){return M && M->HullClass==EShipHullClass::Small?0:M && M->HullClass==EShipHullClass::Medium?1:2;};
        return Rank(MA)<Rank(MB);
    });
    auto Nearby=[&](APawn* P,FVector2D Desired,FVector2D& Out,int32 SpawnTeam=-1,bool Goal=false)
    {
        const auto Map=NavigationChartForShip(P);TArray<TPair<double,FVector2D>> Options;
        for(int32 I=0;I<Map->Cells.Num();++I)if(Map->Cells[I])
        {
            const auto Q=Map->Center(I);const double D=FVector2D::Distance(Q,Desired);
            if(SpawnTeam>=0 && (!Grid->IsInsideField(Q,SpawnHullRadius(P)+30) ||
                (SpawnTeam==0 && Q.X>=(P->IsPlayerControlled()?14750:14500)) || (SpawnTeam==1 && Q.X<=14500)))continue;
            if(Goal)
            {
                bool Occupied=false;
                for(const auto& Other:Assignments)if(Other.Ship!=P && Other.Ship->IsPlayerControlled() &&
                    FVector2D::Distance(Q,Other.SpawnEN)<FMath::Max(260.f,Other.HullRadiusM+SpawnHullRadius(P)+100)){Occupied=true;break;}
                if(Occupied)continue;
            }
            if(D<=(Goal?350:180))Options.Emplace(D,Q);
        }
        Options.Sort([](const auto& A,const auto& B){return A.Key<B.Key;});
        for(int32 I=0;I<FMath::Min(40,Options.Num());++I)
        {
            FNavalSpawnAssignment Probe;Probe.Ship=P;Probe.SpawnEN=Probe.TargetEN=Options[I].Value;Probe.HullRadiusM=SpawnHullRadius(P);
            FSeaRouteResult R;R.bSuccess=true;R.Points.Add(Probe.SpawnEN);bool Waiting=false;
            if(CheckPhysicalAssignment(Probe,R,Waiting)){Out=Probe.SpawnEN;return true;}
        }
        return false;
    };
    LayoutRandom.Initialize(bUsedSafeFallback?20261028:ActualSeed);
    for(LayoutAttempts=1;LayoutAttempts<=64;++LayoutAttempts)
    {
        Assignments.Reset();bool Good=true;
        for(int32 Team=0;Team<2 && Good;++Team)for(int32 Slot=0;Slot<Teams[Team].Num() && Good;++Slot)
        {
            APawn* P=Teams[Team][Slot];const auto* M=P->FindComponentByClass<UShipMovementComponent>();const auto Map=NavigationChartForShip(P);
            FNavalSpawnAssignment A;A.Ship=P;A.Team=Team;A.Slot=Slot;A.HullRadiusM=SpawnHullRadius(P);A.DefensiveAnchorEN=FVector2D(13900,3450);
            const bool Small=M && M->HullClass==EShipHullClass::Small;
            FVector2D StartHint=Team==0?(P->IsPlayerControlled()?FVector2D(14687.5,3287.5):!Small && Slot==2?FVector2D(14212.5,2937.5):Loop[FMath::Min(Slot,2)]):RedSpawns[Slot];
            if(ActualSeed!=20261028 || LayoutAttempts>1)StartHint+=FVector2D(LayoutRandom.FRandRange(-60,60),LayoutRandom.FRandRange(-60,60));
            if(!Nearby(P,StartHint,A.SpawnEN,Team))
            {Good=false;break;}
            if(Team==0 && Small && !P->IsPlayerControlled())
            {
                for(const auto& Hint:Loop)
                {FVector2D Q;if(!Nearby(P,Hint,Q)){Good=false;break;}A.PatrolCircuitEN.Add(Q);}
                if(!Good)break;
                // Validate every closed-loop edge with the actual hull, not
                // just the first leg or a nearest-water destination sample.
                double Winding=0;
                for(int32 I=0;I<A.PatrolCircuitEN.Num();++I)
                {
                    FNavalSpawnAssignment Edge=A;Edge.SpawnEN=A.PatrolCircuitEN[I];Edge.TargetEN=A.PatrolCircuitEN[(I+1)%A.PatrolCircuitEN.Num()];
                    const auto R=Map->FindPath(Edge.SpawnEN,Edge.TargetEN,75000,250);bool Waiting=false;
                    if(!CheckPhysicalAssignment(Edge,R,Waiting)){Good=false;break;}
                    for(int32 K=1;K<R.Points.Num();++K)
                    {
                        const auto U=R.Points[K-1]-FVector2D(13750,3400),V=R.Points[K]-FVector2D(13750,3400);
                        Winding+=FMath::FindDeltaAngleRadians(FMath::Atan2(U.Y,U.X),FMath::Atan2(V.Y,V.X));
                    }
                }
                if(Good && FMath::Abs(FMath::Abs(Winding)-2*PI)>.1)
                {LastPhysicalRejection=TEXT("Patrol circuit backtracks instead of encircling the island");Good=false;}
                A.CircuitStartIndex=(Slot+1)%A.PatrolCircuitEN.Num();A.TargetEN=A.PatrolCircuitEN[A.CircuitStartIndex];
            }
            else
            {
                FVector2D GoalHint=Team==0?Heavy[Slot%2==0?1:0]:RedGoals[Slot];
                if(ActualSeed!=20261028 || LayoutAttempts>1)GoalHint+=FVector2D(LayoutRandom.FRandRange(-40,40),LayoutRandom.FRandRange(-40,40));
                if(!Nearby(P,GoalHint,A.TargetEN,-1,true)){Good=false;break;}
            }
            if(!Good)break;
            if(P->IsPlayerControlled())A.TargetEN=A.SpawnEN;
            FSeaRouteResult Route;
            if(P->IsPlayerControlled()){Route.bSuccess=true;Route.Points.Add(A.SpawnEN);}
            else Route=Map->FindPath(A.SpawnEN,A.TargetEN,75000,250);
            if(!Route.bSuccess || Route.Points.IsEmpty()){Good=false;break;}
            const auto Direction=Route.Points.Num()>1?Route.Points[1]-A.SpawnEN:FVector2D(1,0);
            A.HeadingDegrees=FMath::RadiansToDegrees(FMath::Atan2(-Direction.Y,Direction.X));
            for(const auto& Other:Assignments)
                if(FVector2D::Distance(A.SpawnEN,Other.SpawnEN)<FMath::Max(240.f,A.HullRadiusM+Other.HullRadiusM+65)){Good=false;break;}
            bool Waiting=false;if(!Good || !CheckPhysicalAssignment(A,Route,Waiting)){Good=false;break;}
            const int32 Sector=Grid->SectorAtChartPosition(A.SpawnEN);A.SpawnSector=Grid->Sectors.IsValidIndex(Sector)?Grid->Sectors[Sector].Id:NAME_None;
            Assignments.Add(MoveTemp(A));
        }
        if(Good && Assignments.Num()==Ships.Num())return true;
    }
    Assignments.Reset();return false;
}
bool UNavalBattlefieldSubsystem::ValidatePhysicalLayout(bool& Waiting)
{
    Waiting=false;auto* Ocean=NavalWaterQueries::FindOcean(GetWorld());
    if(!Ocean){Fail(TEXT("WaterBodyOcean missing"));return false;}
    if(GetWorld()->GetTimeSeconds()-InitializedAt<6){Waiting=true;return false;}
    for(TActorIterator<ACesium3DTileset> I(GetWorld());I;++I)
        if(I->FindComponentByClass<USeaTerrainCollisionComponent>() && I->GetLoadProgress()<99.9f){TerrainStableSince=-1;Waiting=true;return false;}
    if(TerrainStableSince<0)TerrainStableSince=GetWorld()->GetTimeSeconds();
    if(GetWorld()->GetTimeSeconds()-TerrainStableSince<2){Waiting=true;return false;}
    if(!bPhysicalSearch)
    {
        if(!BuildOperationalCharts())return false;
        bPhysicalSearch=true;TArray<APawn*> Ships;for(APawn* P:RetainedFleet)if(IsValid(P))Ships.Add(P);
        if(!GenerateLayout(Ships))
        {
            bUsedSafeFallback=true;LayoutRandom.Initialize(20261028);
            if(!GenerateLayout(Ships)){Fail(TEXT("No physically clear initial layout after bounded search/fallback: ")+LastPhysicalRejection);return false;}
        }
    }
    for(auto& A:Assignments)
    {
        FSeaRouteResult Route;
        if(A.Ship->IsPlayerControlled()){Route.bSuccess=true;Route.Points.Add(A.SpawnEN);}
        else Route=NavigationChartForShip(A.Ship)->FindPath(A.SpawnEN,A.TargetEN,75000,250);
        if(!CheckPhysicalAssignment(A,Route,Waiting)){if(!Waiting)Fail(LastPhysicalRejection);return false;}
    }
    return true;
}
bool UNavalBattlefieldSubsystem::CheckPhysicalAssignment(FNavalSpawnAssignment& A,const FSeaRouteResult& Route,bool& Waiting)
{
    auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();auto* Ocean=NavalWaterQueries::FindOcean(GetWorld());
    if(!Ocean){LastPhysicalRejection=TEXT("WaterBodyOcean missing");return false;}
    FCollisionQueryParams Q(SCENE_QUERY_STAT(NavalSpawnClearance),true);
    NavalWaterQueries::IgnoreWaterActors(GetWorld(),Q);
    for(APawn* P:RetainedFleet)Q.AddIgnoredActor(P);
        auto* Hull=UShipCombatComponent::PhysicalHull(A.Ship);const FBox B=Hull->GetLocalBounds().GetBox();const FVector Scale=Hull->GetComponentScale().GetAbs();
        if(!Route.bSuccess || Route.Points.IsEmpty()){LastPhysicalRejection=TEXT("Initial route lost native connectivity");return false;}
        auto Clear=[&](FVector2D EN,float Heading,bool Turning)
        {
            FVector P=Grid->ChartToWorld(EN),Water,Normal,Velocity;float Depth=0;Ocean->GetWaterSurfaceInfoAtLocation(P,Water,Normal,Velocity,Depth,false);P.Z=Water.Z;
            bool Coverage=false;
            for(TActorIterator<ACesium3DTileset> I(GetWorld());I;++I)if(const auto* F=I->FindComponentByClass<USeaTerrainCollisionComponent>();F && F->HasCollisionCoverageAt(P)){Coverage=true;break;}
            if(!Coverage){Waiting=true;LastPhysicalRejection=TEXT("Streamed collision coverage missing");return false;}
            const FQuat Rotation=FRotator(0,Heading,0).Quaternion();
            FVector Extent=B.GetExtent()*Scale+FVector(200);
            if(Turning)Extent.X=Extent.Y=(A.HullRadiusM+30)*100;
            const FVector Center=P+Rotation.RotateVector(B.GetCenter()*Scale);
            TArray<FOverlapResult> Hits;
            if(GetWorld()->OverlapMultiByChannel(Hits,Center,Rotation,ECC_Visibility,FCollisionShape::MakeBox(Extent),Q))
            {
                for(const auto& Hit:Hits)if(Hit.bBlockingHit)
                {LastPhysicalCollider=FString::Printf(TEXT("EN=(%.1f,%.1f) actor=%s component=%s"),EN.X,EN.Y,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()));break;}
                return false;
            }
            return true;
        };
        if(!Clear(A.SpawnEN,A.HeadingDegrees,true) || !Clear(A.TargetEN,A.HeadingDegrees,true))
        {if(!Waiting)LastPhysicalRejection=FString::Printf(TEXT("Seed %d: unsafe turning/keel clearance for %s: %s"),ActualSeed,*A.Ship->GetActorNameOrLabel(),*LastPhysicalCollider);return false;}
        for(int32 I=1;I<Route.Points.Num();++I)
        {
            const auto D=Route.Points[I]-Route.Points[I-1];const int32 Steps=FMath::Max(1,FMath::CeilToInt(D.Size()/75));
            const float Heading=FMath::RadiansToDegrees(FMath::Atan2(-D.Y,D.X));
            for(int32 K=0;K<=Steps;++K)if(!Clear(FMath::Lerp(Route.Points[I-1],Route.Points[I],double(K)/Steps),Heading,false))
            {if(!Waiting)LastPhysicalRejection=FString::Printf(TEXT("Seed %d: unsafe operational route for %s: %s"),ActualSeed,*A.Ship->GetActorNameOrLabel(),*LastPhysicalCollider);return false;}
        }
        A.bPhysicalClearanceVerified=true;A.DepthStatus=TEXT("Streamed actual hull, turn areas and entire initial route checked at waterline with 2 m clearance; dynamic stability and bathymetry not certified");
    return true;
}

bool UNavalBattlefieldSubsystem::PlaceFleet()
{
    auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();auto* Ocean=NavalWaterQueries::FindOcean(GetWorld());
    for(auto& A:Assignments)
    {
        auto* Hull=UShipCombatComponent::PhysicalHull(A.Ship);auto* Anchor=A.Ship->FindComponentByClass<UCesiumGlobeAnchorComponent>();if(!Hull || !Anchor || !Ocean)return false;
        auto* Geo=Anchor->ResolveGeoreference();FVector P=Grid->ChartToWorld(A.SpawnEN),Water,Normal,Velocity;float Depth=0;
        Ocean->GetWaterSurfaceInfoAtLocation(P,Water,Normal,Velocity,Depth,false);P.Z=Water.Z+600;
        Anchor->MoveToLongitudeLatitudeHeight(Geo->TransformUnrealPositionToLongitudeLatitudeHeight(P+FVector(GetWorld()->OriginLocation)));
        A.Ship->SetActorRotation(FRotator(0,A.HeadingDegrees,0));Anchor->Sync();Hull->SetPhysicsLinearVelocity(FVector::ZeroVector);Hull->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Hull->WakeAllRigidBodies();
        if(auto* N=A.Ship->FindComponentByClass<UShipEnemyNavigationComponent>())
        {
            for(AActor* OldMarker:N->PatrolCircuitActors)
                if(IsValid(OldMarker) && OldMarker->ActorHasTag(TEXT("GeneratedPatrolMarker")))OldMarker->Destroy();
            N->PatrolCircuitActors.Reset();N->PatrolCircuitIndex=A.CircuitStartIndex;
            for(int32 I=0;I<A.PatrolCircuitEN.Num();++I)
            {
                auto* Marker=GetWorld()->SpawnActor<ATargetPoint>();if(!Marker)return false;
                Marker->Tags.Add(TEXT("GeneratedPatrolMarker"));
                FVector T=Grid->ChartToWorld(A.PatrolCircuitEN[I]);Ocean->GetWaterSurfaceInfoAtLocation(T,Water,Normal,Velocity,Depth,false);T.Z=Water.Z;
                Marker->GetRootComponent()->SetMobility(EComponentMobility::Movable);Marker->SetActorLocation(T);
#if WITH_EDITOR
                Marker->SetActorLabel(FString::Printf(TEXT("TP_Blue%d_Circuit_%02d"),A.Slot+1,I+1));
#endif
                N->PatrolCircuitActors.Add(Marker);
            }
            for(int32 I=0;I<2;++I)
            {
                AActor* Marker=I==0?N->PatrolActorA.Get():N->PatrolActorB.Get();if(!IsValid(Marker))return false;
                FVector T=Grid->ChartToWorld(I==0?A.SpawnEN:A.TargetEN);Ocean->GetWaterSurfaceInfoAtLocation(T,Water,Normal,Velocity,Depth,false);T.Z=Water.Z;
                Marker->GetRootComponent()->SetMobility(EComponentMobility::Movable);Marker->SetActorLocation(T);
                if(auto* M=Marker->FindComponentByClass<UCesiumGlobeAnchorComponent>())M->Sync();
            }
            N->PatrolOffsetA=N->PatrolOffsetB=FVector::ZeroVector;N->bStartAtPatrolB=true;N->StartPatrol();
            if(!N->UseOperationalChart(NavigationChartForShip(A.Ship)))return false;
            if(AreTacticsEnabled() && A.Team==0)if(auto* C=A.Ship->FindComponentByClass<UShipCombatComponent>())C->SetCombatRole(EShipEnemyRole::Fighter);
            const auto* Settings=GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>();
            if(AreTacticsEnabled() && Settings->bEnableRedAssault && A.Team==1 && A.Slot>0)N->StopNavigation();
        }
    }
    return true;
}
void UNavalBattlefieldSubsystem::Tick(float)
{
    if(bUsingAuthoredLayout)
    {
        if(!bInitialized || bReady || !InitializationError.IsEmpty())return;
        const double Age=GetWorld()->GetTimeSeconds()-InitializedAt;
        if(Age>30){Fail(TEXT("MY MAP: a placed ship did not settle in water. Check its starting height, water contact and collision."));return;}
        if(Age<6)return;
        for(TActorIterator<ACesium3DTileset> Tile(GetWorld());Tile;++Tile)
            if(Tile->FindComponentByClass<USeaTerrainCollisionComponent>() && Tile->GetLoadProgress()<99.9f){TerrainStableSince=-1;return;}
        if(TerrainStableSince<0)TerrainStableSince=GetWorld()->GetTimeSeconds();
        if(GetWorld()->GetTimeSeconds()-TerrainStableSince<2)return;
        if(!bPhysicalSearch)
        {
            if(!BuildOperationalCharts())return;
            bPhysicalSearch=true;
            for(APawn* Ship:RetainedFleet)if(auto* Nav=Ship->FindComponentByClass<UShipEnemyNavigationComponent>())
            {
                FString Error;
                if(!Nav->UseOperationalChart(NavigationChartForShip(Ship)) || !Nav->PrepareAuthoredPatrol(Error))
                {Fail(Error.IsEmpty()?TEXT("MY MAP: could not apply the surveyed route chart"):Error);return;}
            }
        }
        for(APawn* Ship:RetainedFleet)
        {
            const auto* Buoyancy=IsValid(Ship)?Ship->FindComponentByClass<UBuoyancyComponent>():nullptr;
            const auto* Hull=IsValid(Ship)?UShipCombatComponent::PhysicalHull(Ship):nullptr;
            if(!Buoyancy || !Buoyancy->IsInWaterBody() || !Hull || !Hull->IsSimulatingPhysics())return;
        }
        bReady=true;Status=FString::Printf(TEXT("MY MAP | A-B PATROL | SEED %d"),ActualSeed);
        if(auto* Battle=GetWorld()->GetSubsystem<UNavalBattleSubsystem>()){Battle->ConfigurationError.Empty();Battle->Notification=Status;}
        if(auto* PC=Cast<ACustomPlayerController>(UGameplayStatics::GetPlayerController(this,0));PC && PC->bFullAISpectating)PC->ShowBattlefieldOverview();
        return;
    }
    if(LoadingCamera)if(auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>())LoadingCamera->SetActorLocation(Grid->ChartToWorld(Grid->CenterEN)+FVector(0,0,230000));
    if(bReady && AreTacticsEnabled()){UpdateDefenders();UpdateAssault();}
    if(!IsEnabled() || !bInitialized || bReady || !InitializationError.IsEmpty())return;
    const double Now=GetWorld()->GetTimeSeconds();
    if(Now-InitializedAt>30){Fail(TEXT("Terrain/water initialization exceeded 30 seconds; no combat commands enabled"));return;}
    if(PlacedAt<0)
    {
        ++PhysicalLayoutRetries;
        bool Waiting=false;if(!ValidatePhysicalLayout(Waiting))
        {
            if(Waiting)return;
            if(InitializationError.IsEmpty())Fail(TEXT("No separated, connected per-hull layout after bounded physical search and fallback"));
            return;
        }
        if(!PlaceFleet()){Fail(TEXT("Existing ship anchor or patrol marker missing"));return;}
        PlacedAt=Now;Status=TEXT("Settling fleet on water...");return;
    }
    if(Now-PlacedAt<2)return;
    for(const auto& A:Assignments)
    {
        const auto* B=A.Ship->FindComponentByClass<UBuoyancyComponent>();auto* H=UShipCombatComponent::PhysicalHull(A.Ship);
        if(!B || !B->IsInWaterBody() || !H || !H->IsSimulatingPhysics())return;
        if(!GetWorld()->GetSubsystem<UNavalSectorSubsystem>()->NavigationChart()->IsWater(GetWorld()->GetSubsystem<UNavalSectorSubsystem>()->WorldToChart(H->GetComponentLocation())))
        {Fail(TEXT("Ship drifted outside accepted water while settling"));return;}
    }
    bReady=true;Status=FString::Printf(TEXT("SEED %d"),ActualSeed);
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0))
    {
        if(auto* Custom=Cast<ACustomPlayerController>(PC);Custom && Custom->bFullAISpectating)Custom->ShowBattlefieldOverview();
        else if(APawn* P=PC->GetPawn())PC->SetViewTarget(P);
    }
    if(auto* B=GetWorld()->GetSubsystem<UNavalBattleSubsystem>()){B->ConfigurationError.Empty();B->Notification=Status;}
}
void UNavalBattlefieldSubsystem::Deinitialize()
{
    if(LoadingCamera)
    {
        if(auto* Manager=StreamingCameraManager.Get())Manager->AdditionalCameras.RemoveAll([this](const FCesiumCamera& C){return C.CameraComponent.Get()==LoadingCamera->GetCameraComponent();});
        LoadingCamera->Destroy();
    }
    LoadingCamera=nullptr;StreamingCameraManager.Reset();Assignments.Reset();RetainedFleet.Reset();OperationalCharts.Reset();ObservedContacts.Reset();Super::Deinitialize();
}
FString UNavalBattlefieldSubsystem::LayoutReport() const
{
    auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("seed"),ActualSeed);J->SetBoolField(TEXT("ready"),bReady);J->SetStringField(TEXT("status"),Status);J->SetStringField(TEXT("error"),InitializationError);J->SetNumberField(TEXT("attempts"),LayoutAttempts);
    J->SetBoolField(TEXT("authored_layout"),bUsingAuthoredLayout);
    J->SetStringField(TEXT("assault_leader"),AssaultLeader.IsValid()?AssaultLeader->GetActorNameOrLabel():TEXT("none"));J->SetNumberField(TEXT("leader_changes"),AssaultLeaderChanges);
    J->SetNumberField(TEXT("physical_layout_retries"),PhysicalLayoutRetries);J->SetBoolField(TEXT("used_safe_fallback"),bUsedSafeFallback);
    auto Charts=MakeShared<FJsonObject>();
    for(const auto& Pair:OperationalCharts)
    {
        auto C=MakeShared<FJsonObject>();int32 Water=0,Regions=0;
        for(int32 I=0;I<Pair.Value->Cells.Num();++I){Water+=Pair.Value->Cells[I]?1:0;Regions=FMath::Max(Regions,Pair.Value->ConnectedRegions[I]);}
        C->SetNumberField(TEXT("clear_cells"),Water);C->SetNumberField(TEXT("connected_regions"),Regions);Charts->SetObjectField(UEnum::GetValueAsString(Pair.Key),C);
    }
    J->SetObjectField(TEXT("physical_navigation_charts"),Charts);
    TArray<TSharedPtr<FJsonValue>> Items;
    for(const auto& A:Assignments)
    {
        auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("ship"),A.Ship?A.Ship->GetActorNameOrLabel():TEXT("missing"));O->SetNumberField(TEXT("team"),A.Team);O->SetNumberField(TEXT("slot"),A.Slot);O->SetStringField(TEXT("sector"),A.SpawnSector.ToString());
        O->SetArrayField(TEXT("spawn_en_m"),{MakeShared<FJsonValueNumber>(A.SpawnEN.X),MakeShared<FJsonValueNumber>(A.SpawnEN.Y)});O->SetArrayField(TEXT("target_en_m"),{MakeShared<FJsonValueNumber>(A.TargetEN.X),MakeShared<FJsonValueNumber>(A.TargetEN.Y)});O->SetNumberField(TEXT("heading"),A.HeadingDegrees);O->SetBoolField(TEXT("physical_clearance"),A.bPhysicalClearanceVerified);O->SetStringField(TEXT("depth_status"),A.DepthStatus);
        TArray<TSharedPtr<FJsonValue>> Circuit;
        for(const auto& P:A.PatrolCircuitEN)Circuit.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y)}));
        O->SetArrayField(TEXT("patrol_circuit_en_m"),Circuit);O->SetNumberField(TEXT("circuit_start_index"),A.CircuitStartIndex);
        O->SetArrayField(TEXT("defense_anchor_en_m"),{MakeShared<FJsonValueNumber>(A.DefensiveAnchorEN.X),MakeShared<FJsonValueNumber>(A.DefensiveAnchorEN.Y)});
        O->SetStringField(TEXT("mission"),A.MissionState);O->SetNumberField(TEXT("interceptions_started"),A.InterceptionsStarted);O->SetNumberField(TEXT("defensive_returns"),A.DefensiveReturns);
        O->SetNumberField(TEXT("assault_phase"),A.AssaultPhase);O->SetNumberField(TEXT("assault_goals_reached"),A.AssaultGoalsReached);O->SetBoolField(TEXT("assault_released"),A.bAssaultReleased);O->SetNumberField(TEXT("formation_distance_m"),A.FormationDistanceM);O->SetNumberField(TEXT("mission_route_retries"),A.MissionRouteRetries);
        O->SetBoolField(TEXT("searching_circuit"),A.bSearchingCircuit);O->SetNumberField(TEXT("search_circuit_index"),A.SearchCircuitIndex);O->SetNumberField(TEXT("search_goals_reached"),A.SearchGoalsReached);
        O->SetArrayField(TEXT("mission_goal_en_m"),{MakeShared<FJsonValueNumber>(A.MissionGoalEN.X),MakeShared<FJsonValueNumber>(A.MissionGoalEN.Y)});Items.Add(MakeShared<FJsonValueObject>(O));
    }
    J->SetArrayField(TEXT("ships"),Items);FString Out;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Out));return Out;
}
