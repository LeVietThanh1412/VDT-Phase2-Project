#include "NavalBattlefieldSubsystem.h"
#include "NavalSectorSubsystem.h"
#include "NavalMatchSettings.h"
#include "NavalBattleUI.h"
#include "ShipCombatComponent.h"
#include "ShipEnemyNavigationComponent.h"
#include "ShipMovementComponent.h"
#include "SeaTerrainCollisionComponent.h"
#include "Cesium3DTileset.h"
#include "CesiumGlobeAnchorComponent.h"
#include "CesiumGeoreference.h"
#include "BuoyancyComponent.h"
#include "WaterBodyOceanActor.h"
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

namespace
{
float SpawnHullRadius(APawn* Pawn)
{const auto* H=UShipCombatComponent::PhysicalHull(Pawn);if(!H)return 1000;const auto B=H->GetLocalBounds().GetBox();const auto S=H->GetComponentScale().GetAbs();return FVector2D(FMath::Max(FMath::Abs(B.Min.X),FMath::Abs(B.Max.X))*S.X,FMath::Max(FMath::Abs(B.Min.Y),FMath::Abs(B.Max.Y))*S.Y).Size()/100;}
UWaterBodyComponent* SectorOcean(UWorld* World)
{for(TActorIterator<AWaterBodyOcean> I(World);I;++I)return I->GetWaterBodyComponent();return nullptr;}
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
    const auto Enemy=GetWorld()->GetSubsystem<UNavalSectorSubsystem>()->WorldToChart(Target->GetActorLocation());
    return Enemy.X<=14750 && FVector2D::Distance(Enemy,A->DefensiveAnchorEN)<=DefensiveLeashM+250;
}
void UNavalBattlefieldSubsystem::Fail(const FString& Reason)
{bReady=false;InitializationError=Reason;Status=TEXT("Battlefield setup failed: ")+Reason;if(auto* B=GetWorld()->GetSubsystem<UNavalBattleSubsystem>()){B->ConfigurationError=Status;}}
void UNavalBattlefieldSubsystem::InitializeFleet(const TArray<APawn*>& Ships)
{
    if(!IsEnabled()){Status=TEXT("Sector placement disabled during development");return;}
    bInitialized=true;bReady=false;InitializedAt=GetWorld()->GetTimeSeconds();PlacedAt=-1;LayoutAttempts=0;InitializationError.Empty();Assignments.Reset();
    RetainedFleet.Reset();for(auto* P:Ships)RetainedFleet.Add(P);
    PhysicalLayoutRetries=0;bUsedSafeFallback=false;bPhysicalSearch=false;LastPhysicalRejection.Empty();TerrainStableSince=-1;OperationalCharts.Reset();
    AssaultLeader.Reset();AssaultLeaderChanges=0;bAssaultInitialized=false;LastAssaultUpdateAt=-1000;ScoutContactAt=-1000;
    auto* S=GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>();
    ActualSeed=S->BattlefieldSeed>0?S->BattlefieldSeed:FMath::RandRange(1,MAX_int32);LayoutRandom.Initialize(ActualSeed);
    auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();
    if(!Grid || !Grid->bGridReady){Fail(Grid?Grid->GridError:TEXT("Sector subsystem missing"));return;}
    if(!GenerateLayout(Ships))
    {
        bUsedSafeFallback=true;LayoutRandom.Initialize(20261028);
        if(!GenerateLayout(Ships)){Fail(TEXT("No separated, connected layout after bounded search and checked fallback"));return;}
    }
    const FVector Focus=Grid->ChartToWorld(Grid->CenterEN),Eye=Focus+FVector(0,0,230000);
    LoadingCamera=GetWorld()->SpawnActor<ACameraActor>(Eye,(Focus-Eye).Rotation());
    if(LoadingCamera)
    {
        FCesiumCamera Coverage;Coverage.ParameterSource=ECameraParameterSource::CameraComponent;
        Coverage.CameraComponent=LoadingCamera->GetCameraComponent();Coverage.ViewportSize=FVector2D(8192,8192);
        // A fixed, non-rendering Cesium view keeps the selected 2.5 km field
        // streamed independently of spectator/player camera direction.
        StreamingCameraManager=ACesiumCameraManager::GetDefaultCameraManager(this);
        if(StreamingCameraManager.IsValid())StreamingCameraManager->AdditionalCameras.Add(Coverage);
    }
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0))if(LoadingCamera)PC->SetViewTarget(LoadingCamera);
    Status=TEXT("Preparing battlefield...");
    if(auto* B=GetWorld()->GetSubsystem<UNavalBattleSubsystem>()){B->ConfigurationError=Status;}
}
TSharedPtr<const FSeaNavigationMap> UNavalBattlefieldSubsystem::NavigationChartForShip(APawn* Ship) const
{
    const auto* M=Ship?Ship->FindComponentByClass<UShipMovementComponent>():nullptr;
    if(M)if(const auto* Chart=OperationalCharts.Find(M->HullClass))return *Chart;
    return GetWorld()->GetSubsystem<UNavalSectorSubsystem>()->NavigationChart();
}
bool UNavalBattlefieldSubsystem::BuildOperationalCharts()
{
    auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();const auto Coast=Grid->NavigationChart();auto* Ocean=SectorOcean(GetWorld());
    if(!Coast.IsValid() || !Ocean){Fail(TEXT("Cannot survey terrain without chart and ocean"));return false;}
    FCollisionQueryParams Q(SCENE_QUERY_STAT(NavalOperationalChart),true);
    for(TActorIterator<AActor> I(GetWorld());I;++I)
    {const auto Name=I->GetClass()->GetName();if(Name.StartsWith(TEXT("WaterBody")) || Name==TEXT("WaterZone"))Q.AddIgnoredActor(*I);}
    for(APawn* P:RetainedFleet)Q.AddIgnoredActor(P);
    for(APawn* P:RetainedFleet)
    {
        auto* M=P->FindComponentByClass<UShipMovementComponent>();auto* H=UShipCombatComponent::PhysicalHull(P);
        if(!M || !H || OperationalCharts.Contains(M->HullClass))continue;
        auto Chart=MakeShared<FSeaNavigationMap>(*Coast);const FBox B=H->GetLocalBounds().GetBox();const FVector Scale=H->GetComponentScale().GetAbs();
        // Logical sectors stay 100 m. Collision sampling uses a lightweight
        // local 25 m grid, retaining the exact ECEF/ENU frame and original coast.
        Chart->CellSize=25;Chart->Width=FMath::RoundToInt(Grid->WidthM/25);Chart->Height=FMath::RoundToInt(Grid->HeightM/25);
        Chart->MinEast=Grid->CenterEN.X-Grid->WidthM*.5;Chart->MinNorth=Grid->CenterEN.Y-Grid->HeightM*.5;
        Chart->Cells.Init(0,Chart->Width*Chart->Height);Chart->Clearance.Init(0,Chart->Cells.Num());
        const float Radius=SpawnHullRadius(P);
        FVector Extent=B.GetExtent()*Scale+FVector(200);Extent.X=Extent.Y=(Radius+2)*100;
        for(int32 Id=0;Id<Chart->Cells.Num();++Id)
        {
            const auto Center=Chart->Center(Id);
            bool Clear=Coast->IsWater(Center) && Grid->IsInsideField(Center,Radius+30+Chart->CellSize*.5);
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
        // Recompute true per-class connectivity after physical exclusions.
        Chart->ConnectedRegions.Init(0,Chart->Cells.Num());int32 Region=0;TArray<int32> Queue;
        for(int32 Seed=0;Seed<Chart->Cells.Num();++Seed)
        {
            if(!Chart->Cells[Seed] || Chart->ConnectedRegions[Seed])continue;
            ++Region;Queue.Reset();Queue.Add(Seed);Chart->ConnectedRegions[Seed]=Region;
            for(int32 Head=0;Head<Queue.Num();++Head)
            {
                const int32 Id=Queue[Head],X=Id%Chart->Width,Y=Id/Chart->Width;
                for(int32 DY=-1;DY<=1;++DY)for(int32 DX=-1;DX<=1;++DX)
                {
                    if((DX==0 && DY==0) || !Chart->IsWater(X+DX,Y+DY) || (DX && DY && (!Chart->IsWater(X+DX,Y) || !Chart->IsWater(X,Y+DY))))continue;
                    const int32 Next=(Y+DY)*Chart->Width+X+DX;
                    if(!Chart->ConnectedRegions[Next]){Chart->ConnectedRegions[Next]=Region;Queue.Add(Next);}
                }
            }
        }
        OperationalCharts.Add(M->HullClass,Chart);
    }
    return !OperationalCharts.IsEmpty();
}
bool UNavalBattlefieldSubsystem::GenerateLayout(const TArray<APawn*>& Ships)
{
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
bool UNavalBattlefieldSubsystem::ValidatePhysicalLayout(bool& Waiting)
{
    Waiting=false;auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();auto* Ocean=SectorOcean(GetWorld());
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
    auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();auto* Ocean=SectorOcean(GetWorld());
    if(!Ocean){LastPhysicalRejection=TEXT("WaterBodyOcean missing");return false;}
    FCollisionQueryParams Q(SCENE_QUERY_STAT(NavalSpawnClearance),true);
    for(TActorIterator<AActor> I(GetWorld());I;++I)
    {const auto Name=I->GetClass()->GetName();if(Name.StartsWith(TEXT("WaterBody")) || Name==TEXT("WaterZone"))Q.AddIgnoredActor(*I);}
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
    auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();auto* Ocean=SectorOcean(GetWorld());
    for(auto& A:Assignments)
    {
        auto* Hull=UShipCombatComponent::PhysicalHull(A.Ship);auto* Anchor=A.Ship->FindComponentByClass<UCesiumGlobeAnchorComponent>();if(!Hull || !Anchor || !Ocean)return false;
        auto* Geo=Anchor->ResolveGeoreference();FVector P=Grid->ChartToWorld(A.SpawnEN),Water,Normal,Velocity;float Depth=0;
        Ocean->GetWaterSurfaceInfoAtLocation(P,Water,Normal,Velocity,Depth,false);P.Z=Water.Z+600;
        Anchor->MoveToLongitudeLatitudeHeight(Geo->TransformUnrealPositionToLongitudeLatitudeHeight(P+FVector(GetWorld()->OriginLocation)));
        A.Ship->SetActorRotation(FRotator(0,A.HeadingDegrees,0));Anchor->Sync();Hull->SetPhysicsLinearVelocity(FVector::ZeroVector);Hull->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Hull->WakeAllRigidBodies();
        if(auto* N=A.Ship->FindComponentByClass<UShipEnemyNavigationComponent>())
        {
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
bool UNavalBattlefieldSubsystem::FindMissionGoal(const FNavalSpawnAssignment& A,FVector2D Desired,bool Defender,FVector2D& Out,FVector2D Threat,float MinimumThreatDistance) const
{
    const auto Map=NavigationChartForShip(A.Ship);const auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();
    if(!Map.IsValid() || !IsValid(A.Ship))return false;
    const auto Position=Grid->WorldToChart(A.Ship->GetActorLocation());
    const int32 Start=Map->CellAt(Position);
    if(!Map->IsWater(Position) || !Map->ConnectedRegions.IsValidIndex(Start))return false;
    TArray<TPair<double,FVector2D>> Candidates;
    for(int32 I=0;I<Map->Cells.Num();++I)
    {
        if(!Map->Cells[I] || Map->ConnectedRegions[I]!=Map->ConnectedRegions[Start])continue;
        const auto P=Map->Center(I);
        if(!Grid->IsInsideField(P,A.HullRadiusM+30) || FVector2D::Distance(Position,P)<160)continue;
        if(Defender && (P.X>14700 || FVector2D::Distance(P,A.DefensiveAnchorEN)>DefensiveLeashM))continue;
        if(MinimumThreatDistance>0 && FVector2D::Distance(P,Threat)<MinimumThreatDistance)continue;
        bool Occupied=false;
        for(const auto& Other:Assignments)
        {
            if(Other.Ship==A.Ship || !IsValid(Other.Ship))continue;
            const auto* W=Other.Ship->FindComponentByClass<UShipWeaponComponent>();if(!W || W->bIsDestroyed)continue;
            if(FVector2D::Distance(P,Grid->WorldToChart(Other.Ship->GetActorLocation()))<A.HullRadiusM+Other.HullRadiusM+45){Occupied=true;break;}
        }
        if(!Occupied)Candidates.Emplace(FVector2D::Distance(P,Desired)+FVector2D::Distance(Position,P)*.15,P);
    }
    Candidates.Sort([](const auto& L,const auto& R){return L.Key<R.Key;});
    for(int32 I=0;I<FMath::Min(12,Candidates.Num());++I)
    {
        const auto Route=Map->FindPath(Position,Candidates[I].Value,75000,20);if(!Route.bSuccess || Route.Points.Num()<2)continue;
        bool Safe=true;
        for(const auto& P:Route.Points)
            if(!Grid->IsInsideField(P) || (Defender && (P.X>14750 || FVector2D::Distance(P,A.DefensiveAnchorEN)>DefensiveLeashM+150))){Safe=false;break;}
        if(Safe){Out=Candidates[I].Value;return true;}
    }
    return false;
}
void UNavalBattlefieldSubsystem::UpdateDefenders()
{
    const double Now=GetWorld()->GetTimeSeconds();if(Now-LastMissionUpdateAt<.5)return;LastMissionUpdateAt=Now;
    auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();
    for(auto& A:Assignments)
    {
        if(A.Team!=0 || !IsValid(A.Ship) || A.Ship->IsPlayerControlled())continue;
        auto* W=A.Ship->FindComponentByClass<UShipWeaponComponent>();auto* C=A.Ship->FindComponentByClass<UShipCombatComponent>();auto* N=A.Ship->FindComponentByClass<UShipEnemyNavigationComponent>();
        if(!W || !C || !N || W->bIsDestroyed || W->Team()!=0){A.MissionState=TEXT("Defender unavailable");continue;}
        if(!UNavalBattleSubsystem::CanShipAct(A.Ship))continue;
        const auto State=C->TacticalState;
        if(State==EShipTacticalState::Retreat || State==EShipTacticalState::Escape || State==EShipTacticalState::BackwardRetreat || State==EShipTacticalState::ReturnToPatrol)
        {A.MissionState=TEXT("Shared retreat / returning to defense");continue;}
        if(A.bDefensiveIntercept && !N->HasTacticalDestination())A.bDefensiveIntercept=false;
        if(A.bDefensiveIntercept && N->NavigationState==EShipNavigationState::Failed)
        {
            N->ReturnToPatrol();A.bDefensiveIntercept=false;++A.DefensiveReturns;A.MissionRetryAt=Now+30;
            A.MissionState=TEXT("Interception blocked: returning to defense");continue;
        }
        const auto Position=Grid->WorldToChart(A.Ship->GetActorLocation());
        const bool TargetValid=C->bEnableCombat && C->IsHostile(C->Target);
        const auto Enemy=TargetValid?Grid->WorldToChart(C->Target->GetActorLocation()):FVector2D::ZeroVector;
        const bool Intercept=Now>=A.MissionRetryAt && TargetValid && Enemy.X<=14750 && FVector2D::Distance(Enemy,A.DefensiveAnchorEN)<=DefensiveLeashM+250 &&
            FVector2D::Distance(Position,A.DefensiveAnchorEN)<=DefensiveLeashM+100 && W->CurrentHealth>W->MaxHealth*.4;
        if(!Intercept)
        {
            if(A.bDefensiveIntercept)
            {N->ReturnToPatrol();A.bDefensiveIntercept=false;++A.DefensiveReturns;A.LastMissionPlanAt=Now;}
            A.MissionState=TEXT("Western island defensive patrol");continue;
        }
        if(Now-A.LastMissionPlanAt<5)continue;
        A.LastMissionPlanAt=Now;
        // Seek a safe firing position on the defensive side of the threat.
        // The actual gun still uses the shared faction/LOS/ballistic pipeline.
        FVector2D Goal;const auto TowardDefense=(A.DefensiveAnchorEN-Enemy).GetSafeNormal();
        const float StandOff=FMath::Max(C->MinimumRetreatDistanceM+150,C->EngagementRadiusM*.6f);
        if(FindMissionGoal(A,Enemy+TowardDefense*StandOff,true,Goal,Enemy,C->MinimumRetreatDistanceM+80))
        {
            const bool SameGoal=A.bDefensiveIntercept && N->HasTacticalDestination() &&
                FVector2D::Distance(Grid->WorldToChart(N->GeographicToWorld(N->CurrentFinalDestination)),Goal)<100;
            if(SameGoal){A.MissionState=TEXT("Following bounded interception route");continue;}
            if(N->BeginTacticalMoveTo(N->WorldToGeographic(Grid->ChartToWorld(Goal))))
            {
                if(!A.bDefensiveIntercept)++A.InterceptionsStarted;
                A.bDefensiveIntercept=true;A.MissionState=TEXT("Bounded defensive interception");
            }
        }
        else if(!A.bDefensiveIntercept)A.MissionState=TEXT("Patrolling: no safe interception route");
    }
}
bool UNavalBattlefieldSubsystem::SetAssaultGoal(FNavalSpawnAssignment& A,FVector2D Goal)
{
    auto* N=A.Ship->FindComponentByClass<UShipEnemyNavigationComponent>();auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();auto* Ocean=SectorOcean(GetWorld());
    if(!N || !Ocean || !IsValid(N->PatrolActorB))return false;
    const auto Map=NavigationChartForShip(A.Ship);const auto P=Grid->WorldToChart(A.Ship->GetActorLocation());
    if(!Map->FindPath(P,Goal,75000,20).bSuccess)return false;
    FVector World=Grid->ChartToWorld(Goal),Water,Normal,Velocity;float Depth=0;
    Ocean->GetWaterSurfaceInfoAtLocation(World,Water,Normal,Velocity,Depth,false);World.Z=Water.Z;
    N->PatrolActorB->GetRootComponent()->SetMobility(EComponentMobility::Movable);N->PatrolActorB->SetActorLocation(World);
    if(auto* Anchor=N->PatrolActorB->FindComponentByClass<UCesiumGlobeAnchorComponent>())Anchor->Sync();
    A.MissionGoalEN=Goal;A.bHoldingForLeader=false;A.FormationWaitSeconds=0;A.MissionRouteRetries=0;
    N->bStartAtPatrolB=true;N->StartPatrol();return true;
}
bool UNavalBattlefieldSubsystem::AdvanceAssault(FNavalSpawnAssignment& A)
{
    auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();auto* C=A.Ship->FindComponentByClass<UShipCombatComponent>();
    const int32 Next=FMath::Min(3,A.AssaultPhase+1);
    auto Zone=[&](FName Name)
    {const auto* Index=Grid->ZoneRepresentatives().Find(Name);return Index?Grid->Sectors[*Index].AnchorEN:Grid->CenterEN;};
    const auto Crossing=Zone(TEXT("CENTER_CROSSING")),Core=Zone(TEXT("BLUE_DEFENSE_CORE"));
    FVector2D Desired=Next==1?Crossing:Core;
    const auto Back=(A.SpawnEN-Desired).GetSafeNormal();const FVector2D Side(-Back.Y,Back.X);
    if(Next<3)Desired+=Back*FormationSpacingM*A.Slot+Side*(A.Slot==0?0:A.Slot%2?40:-40);
    FVector2D Threat=FVector2D::ZeroVector;float MinThreat=0;
    if(Next>=3)
    {
        FVector2D Contact=GetWorld()->GetTimeSeconds()-ScoutContactAt<15?ScoutContactEN:Core;
        if(C && C->IsHostile(C->Target)){Contact=Grid->WorldToChart(C->Target->GetActorLocation());Threat=Contact;MinThreat=C->MinimumRetreatDistanceM+80;}
        const float Range=C?FMath::Max(C->MinimumRetreatDistanceM+150,C->EngagementRadiusM*.6f):700;
        const double Angle=FMath::DegreesToRadians(double((ActualSeed%360)+A.Slot*65+A.AssaultGoalsReached*70));
        Desired=Contact+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*Range;
    }
    FVector2D Goal;
    if(!FindMissionGoal(A,Desired,false,Goal,Threat,MinThreat) || !SetAssaultGoal(A,Goal))return false;
    A.AssaultPhase=Next;++A.AssaultGoalsReached;
    A.MissionState=Next==1?TEXT("Assault: central crossing"):Next==2?TEXT("Assault: Blue contact area"):TEXT("Attack: safe flank and reposition");return true;
}
void UNavalBattlefieldSubsystem::UpdateAssault()
{
    const auto* Settings=GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>();
    if(!Settings || !Settings->bEnableRedAssault)return;
    const double Now=GetWorld()->GetTimeSeconds();if(Now-LastAssaultUpdateAt<.5)return;
    const float Dt=FMath::Min(1.,Now-LastAssaultUpdateAt);LastAssaultUpdateAt=Now;
    auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>();
    bool AnyActive=false;for(const auto& A:Assignments)if(A.Team==1 && IsValid(A.Ship) && UNavalBattleSubsystem::CanShipAct(A.Ship)){AnyActive=true;break;}
    if(!AnyActive)return;
    if(!bAssaultInitialized)
    {
        for(auto& A:Assignments)if(A.Team==1 && IsValid(A.Ship))
        {
            if(auto* N=A.Ship->FindComponentByClass<UShipEnemyNavigationComponent>())A.BaseCruiseSpeedMps=N->CruiseSpeedMps;
            if(auto* C=A.Ship->FindComponentByClass<UShipCombatComponent>())C->SetCombatRole(EShipEnemyRole::Fighter);
            A.MissionGoalEN=A.TargetEN;A.MissionState=TEXT("Convoy staging");
            if(A.Slot==0)AssaultLeader=A.Ship;
        }
        bAssaultInitialized=true;
    }
    auto Usable=[&](const FNavalSpawnAssignment& A)
    {
        if(A.Team!=1 || !IsValid(A.Ship))return false;
        const auto* W=A.Ship->FindComponentByClass<UShipWeaponComponent>();const auto* C=A.Ship->FindComponentByClass<UShipCombatComponent>();const auto* N=A.Ship->FindComponentByClass<UShipEnemyNavigationComponent>();
        return W && !W->bIsDestroyed && W->Team()==1 && C && N && A.MissionRouteRetries<3 && N->NavigationState!=EShipNavigationState::Failed && N->RecoveryAttemptCount<2 &&
            C->TacticalState!=EShipTacticalState::Retreat && C->TacticalState!=EShipTacticalState::Escape && C->TacticalState!=EShipTacticalState::BackwardRetreat;
    };
    auto* Leader=Assignments.FindByPredicate([&](const FNavalSpawnAssignment& A){return A.Ship==AssaultLeader.Get() && Usable(A);});
    if(!Leader)
    {
        FNavalSpawnAssignment* Best=nullptr;
        for(auto& A:Assignments)if(Usable(A) && (!Best || A.AssaultPhase>Best->AssaultPhase || (A.AssaultPhase==Best->AssaultPhase && A.Slot<Best->Slot)))Best=&A;
        if(Best)
        {if(AssaultLeader.Get()!=Best->Ship){AssaultLeader=Best->Ship;++AssaultLeaderChanges;}Leader=Best;}
        else AssaultLeader.Reset();
    }
    if(Leader && UNavalBattleSubsystem::CanShipAct(Leader->Ship))
        if(const auto* C=Leader->Ship->FindComponentByClass<UShipCombatComponent>();C && C->IsHostile(C->Target) && C->bTargetVisible)
        {ScoutContactEN=Grid->WorldToChart(C->Target->GetActorLocation());ScoutContactAt=Now;}
    for(auto& A:Assignments)
    {
        if(A.Team!=1 || !IsValid(A.Ship))continue;
        auto* W=A.Ship->FindComponentByClass<UShipWeaponComponent>();auto* C=A.Ship->FindComponentByClass<UShipCombatComponent>();auto* N=A.Ship->FindComponentByClass<UShipEnemyNavigationComponent>();
        if(!W || W->bIsDestroyed || W->Team()!=1 || !C || !N){A.MissionState=TEXT("Assault ship unavailable");continue;}
        if(!UNavalBattleSubsystem::CanShipAct(A.Ship))continue;
        const bool IsLeader=Leader && Leader->Ship==A.Ship;
        const auto Position=Grid->WorldToChart(A.Ship->GetActorLocation());
        const bool Retreat=C->TacticalState==EShipTacticalState::Retreat || C->TacticalState==EShipTacticalState::Escape || C->TacticalState==EShipTacticalState::BackwardRetreat || C->TacticalState==EShipTacticalState::ReturnToPatrol;
        if(Retreat){A.MissionState=TEXT("Shared retreat; assault objective retained");continue;}
        if(!A.bAssaultReleased)
        {
            A.FormationWaitSeconds+=Dt;
            const FNavalSpawnAssignment* Predecessor=Leader;
            for(const auto& Other:Assignments)
                if(Other.Slot<A.Slot && Usable(Other) && (!Predecessor || Other.Slot>Predecessor->Slot))Predecessor=&Other;
            const bool Timed=Predecessor && Predecessor->bAssaultReleased && Now-Predecessor->AssaultReleasedAt>=FormationDepartureIntervalSeconds;
            const bool Moving=Predecessor && FVector2D::Distance(Grid->WorldToChart(Predecessor->Ship->GetActorLocation()),Predecessor->SpawnEN)>=80;
            // A stalled predecessor cannot deadlock the convoy; timing still applies to fallback release.
            if(!IsLeader && Predecessor && (!Timed || (!Moving && A.FormationWaitSeconds<20)))
            {A.MissionState=TEXT("Convoy: awaiting sequential safe departure");continue;}
            A.bAssaultReleased=true;A.AssaultReleasedAt=Now;A.FormationWaitSeconds=0;N->bStartAtPatrolB=true;N->StartPatrol();
        }
        N->CruiseSpeedMps=FMath::Max(1.f,A.BaseCruiseSpeedMps);
        A.FormationDistanceM=Leader?FVector2D::Distance(Position,Grid->WorldToChart(Leader->Ship->GetActorLocation())):0;
        if(Leader && !IsLeader)
        {
            const auto LeadPosition=Grid->WorldToChart(Leader->Ship->GetActorLocation());
            const auto Direction=(Leader->MissionGoalEN-LeadPosition).GetSafeNormal();
            const bool Ahead=FVector2D::DotProduct(Position-LeadPosition,Direction)>-FormationSpacingM*.4f;
            if(A.FormationDistanceM<FormationSpacingM*.85f || (Ahead && A.FormationDistanceM<FormationSpacingM*2.5f))
                N->CruiseSpeedMps=1.5f;
            else if(A.FormationDistanceM<FormationSpacingM*1.4f)N->CruiseSpeedMps=FMath::Min(N->CruiseSpeedMps,FMath::Max(2.f,Leader->BaseCruiseSpeedMps*.65f));
        }
        else if(IsLeader)
        {
            for(const auto& Follower:Assignments)if(Follower.Team==1 && Follower.Ship!=A.Ship && Usable(Follower) &&
                FVector2D::Distance(Position,Grid->WorldToChart(Follower.Ship->GetActorLocation()))>FormationSpacingM*2.5f)
                N->CruiseSpeedMps=FMath::Min(N->CruiseSpeedMps,Follower.BaseCruiseSpeedMps*.8f);
        }
        auto* Hull=UShipCombatComponent::PhysicalHull(A.Ship);
        const bool AtGoal=FVector2D::Distance(Position,A.MissionGoalEN)<=N->ArrivalRadiusM && Hull && Hull->GetPhysicsLinearVelocity().Size()<150;
        if(AtGoal)
        {
            if(A.MissionRouteRetries>=3){A.MissionState=TEXT("Assault route unavailable after bounded retries");continue;}
            A.FormationWaitSeconds+=Dt;
            if(!IsLeader && Leader && A.AssaultPhase>=Leader->AssaultPhase && A.FormationWaitSeconds<20)
            {
                if(!A.bHoldingForLeader){N->StopNavigation();A.bHoldingForLeader=true;}
                A.MissionState=TEXT("Convoy: reserving phase spacing");continue;
            }
            if(Now<A.MissionRetryAt)continue;
            if(AdvanceAssault(A))continue;
            N->StopNavigation();A.bHoldingForLeader=true;A.MissionRetryAt=Now+15;++A.MissionRouteRetries;
            A.MissionState=TEXT("No safe next assault route; bounded retry");
            if(A.MissionRouteRetries>=3)A.MissionState=TEXT("Assault route unavailable after bounded retries");
        }
        else if(N->NavigationState==EShipNavigationState::Failed && Now>=A.MissionRetryAt && A.MissionRouteRetries<3)
        {
            ++A.MissionRouteRetries;A.MissionRetryAt=Now+15;
            const int32 Retries=A.MissionRouteRetries;
            FVector2D Goal;if(FindMissionGoal(A,A.MissionGoalEN,false,Goal))SetAssaultGoal(A,Goal);
            A.MissionRouteRetries=Retries;
            A.MissionState=TEXT("Recovering independent assault route");
        }
        else if(A.bHoldingForLeader && Leader && (IsLeader || A.AssaultPhase<Leader->AssaultPhase || A.FormationWaitSeconds>=20))
        {A.bHoldingForLeader=false;N->bStartAtPatrolB=true;N->StartPatrol();}
        else if(!AtGoal)A.MissionState=IsLeader?TEXT("Scout leading assault"):TEXT("Following independent convoy route");
    }
}
void UNavalBattlefieldSubsystem::Tick(float Dt)
{
    if(LoadingCamera)if(auto* Grid=GetWorld()->GetSubsystem<UNavalSectorSubsystem>())LoadingCamera->SetActorLocation(Grid->ChartToWorld(Grid->CenterEN)+FVector(0,0,230000));
    if(bReady && AreTacticsEnabled()){UpdateDefenders();UpdateAssault();}
    if(!IsEnabled() || !bInitialized || bReady || !InitializationError.IsEmpty())return;
    const double Now=GetWorld()->GetTimeSeconds();
    if(Now-InitializedAt>30){Fail(TEXT("Terrain/water initialization exceeded 30 seconds; no combat commands enabled"));return;}
    if(PlacedAt<0)
    {
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
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0))if(APawn* P=PC->GetPawn())PC->SetViewTarget(P);
    if(auto* B=GetWorld()->GetSubsystem<UNavalBattleSubsystem>()){B->ConfigurationError.Empty();B->Notification=Status;}
}
void UNavalBattlefieldSubsystem::Deinitialize()
{
    if(LoadingCamera)
    {
        if(auto* Manager=StreamingCameraManager.Get())Manager->AdditionalCameras.RemoveAll([this](const FCesiumCamera& C){return C.CameraComponent.Get()==LoadingCamera->GetCameraComponent();});
        LoadingCamera->Destroy();
    }
    LoadingCamera=nullptr;StreamingCameraManager.Reset();Assignments.Reset();RetainedFleet.Reset();OperationalCharts.Reset();Super::Deinitialize();
}
FString UNavalBattlefieldSubsystem::LayoutReport() const
{
    auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("seed"),ActualSeed);J->SetBoolField(TEXT("ready"),bReady);J->SetStringField(TEXT("status"),Status);J->SetStringField(TEXT("error"),InitializationError);J->SetNumberField(TEXT("attempts"),LayoutAttempts);
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
        O->SetArrayField(TEXT("defense_anchor_en_m"),{MakeShared<FJsonValueNumber>(A.DefensiveAnchorEN.X),MakeShared<FJsonValueNumber>(A.DefensiveAnchorEN.Y)});
        O->SetStringField(TEXT("mission"),A.MissionState);O->SetNumberField(TEXT("interceptions_started"),A.InterceptionsStarted);O->SetNumberField(TEXT("defensive_returns"),A.DefensiveReturns);
        O->SetNumberField(TEXT("assault_phase"),A.AssaultPhase);O->SetNumberField(TEXT("assault_goals_reached"),A.AssaultGoalsReached);O->SetBoolField(TEXT("assault_released"),A.bAssaultReleased);O->SetNumberField(TEXT("formation_distance_m"),A.FormationDistanceM);O->SetNumberField(TEXT("mission_route_retries"),A.MissionRouteRetries);
        O->SetArrayField(TEXT("mission_goal_en_m"),{MakeShared<FJsonValueNumber>(A.MissionGoalEN.X),MakeShared<FJsonValueNumber>(A.MissionGoalEN.Y)});Items.Add(MakeShared<FJsonValueObject>(O));
    }
    J->SetArrayField(TEXT("ships"),Items);FString Out;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Out));return Out;
}
