#include "SeaNavigationMap.h"
#include "ShipEnemyNavigationComponent.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "CesiumGeoreference.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ShipEnemyProjectile.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeaGridCases,"ShipEnemyNavigation.Planner.Geometry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeaGridCases::RunTest(const FString&)
{
    auto Grid=[](){FSeaNavigationMap M;M.Width=30;M.Height=30;M.CellSize=100;M.Cells.Init(1,900);M.Clearance.Init(5,900);for(int32 I=0;I<30;++I){M.Cells[I]=M.Cells[870+I]=M.Cells[I*30]=M.Cells[I*30+29]=0;}return M;};
    auto Certify=[&](const FSeaNavigationMap& M,const FSeaRouteResult& R){TestTrue(TEXT("A* succeeds"),R.bSuccess);for(int32 I=1;I<R.Points.Num();++I)TestTrue(TEXT("Every smoothed segment safe"),M.SegmentClear(R.Points[I-1],R.Points[I]));};
    auto M=Grid();Certify(M,M.FindPath(FVector2D(250,250),FVector2D(2650,2650),75000,1000));
    for(int32 Y=8;Y<22;++Y)for(int32 X=12;X<18;++X)M.Cells[Y*30+X]=0;
    TestFalse(TEXT("Island blocks direct shortcut"),M.SegmentClear(FVector2D(350,1550),FVector2D(2650,1550)));
    auto R=M.FindPath(FVector2D(350,1550),FVector2D(2650,1550),75000,1000);Certify(M,R);TestTrue(TEXT("Island requires detour"),R.Points.Num()>2);
    TestFalse(TEXT("Destination in land rejected"),M.FindPath(FVector2D(250,250),FVector2D(1450,1550)).bSuccess);
    TestFalse(TEXT("Out of coverage rejected"),M.FindPath(FVector2D(-1,250),FVector2D(2650,1550)).bSuccess);
    TestFalse(TEXT("Expansion budget enforced"),M.FindPath(FVector2D(350,1550),FVector2D(2650,1550),1,1000).bSuccess);
    M=Grid();for(int32 Y=1;Y<29;++Y)M.Cells[Y*30+15]=0;
    TestFalse(TEXT("Disconnected water returns no route"),M.FindPath(FVector2D(250,250),FVector2D(2650,2650),75000,1000).bSuccess);
    M.Cells[15*30+15]=1;Certify(M,M.FindPath(FVector2D(250,1550),FVector2D(2650,1550),75000,1000));
    M.Cells[15*30+15]=0;TestFalse(TEXT("Passage removed by clearance inflation cannot be crossed"),M.FindPath(FVector2D(250,1550),FVector2D(2650,1550),75000,1000).bSuccess);
    M=Grid();M.Cells[2*30+3]=M.Cells[3*30+2]=0;
    TestFalse(TEXT("Diagonal cannot cut blocked corner"),M.SegmentClear(FVector2D(250,250),FVector2D(350,350)));
    // U-shaped inlet remains escapable through its opening rather than crossing its walls.
    M=Grid();for(int32 Y=8;Y<22;++Y){M.Cells[Y*30+8]=0;M.Cells[Y*30+21]=0;}for(int32 X=8;X<22;++X)M.Cells[8*30+X]=0;
    Certify(M,M.FindPath(FVector2D(1450,1450),FVector2D(1450,450),75000,1000));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeaRealMapCases,"ShipEnemyNavigation.Planner.RealOSM",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeaRealMapCases::RunTest(const FString&)
{
    FString Error;auto Map=FSeaNavigationMap::Load(FPaths::ProjectContentDir()/TEXT("SeaNavigation/halong_navigation.json"),Error);
    if(!TestTrue(TEXT("Actual packaged OSM map loads: ")+Error,Map.IsValid()))return false;
    auto Again=FSeaNavigationMap::Load(FPaths::ProjectContentDir()/TEXT("SeaNavigation/halong_navigation.json"),Error);
    TestTrue(TEXT("Multiple ships share immutable map"),Map.Get()==Again.Get());
    FString Data;TSharedPtr<FJsonObject> J;
    if(!FFileHelper::LoadFileToString(Data,*(FPaths::ProjectDir()/TEXT("Data/SeaNavigation/validation_report.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Data),J)){AddError(TEXT("Missing independently validated real-data examples"));return false;}
    int32 Detours=0;
    for(const auto& V:J->GetArrayField(TEXT("examples")))
    {
        auto E=V->AsObject();const auto& A=E->GetArrayField(TEXT("start_en_m"));const auto& B=E->GetArrayField(TEXT("goal_en_m"));
        auto R=Map->FindPath(FVector2D(A[0]->AsNumber(),A[1]->AsNumber()),FVector2D(B[0]->AsNumber(),B[1]->AsNumber()),75000,1000);
        TestTrue(E->GetStringField(TEXT("name")),R.bSuccess);
        for(int32 I=1;I<R.Points.Num();++I)TestTrue(TEXT("Native smoothed OSM segment safe"),Map->SegmentClear(R.Points[I-1],R.Points[I]));
        if(E->GetBoolField(TEXT("straight_crosses_buffered_land"))){++Detours;TestTrue(TEXT("Actual island detour has intermediate waypoints"),R.Points.Num()>2);}
        auto Return=Map->FindPath(FVector2D(B[0]->AsNumber(),B[1]->AsNumber()),FVector2D(A[0]->AsNumber(),A[1]->AsNumber()),75000,1000);
        TestTrue(TEXT("Reverse patrol leg is routable"),Return.bSuccess);
    }
    TestTrue(TEXT("At least one genuine OSM island detour executed"),Detours>0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeaComponentState,"ShipEnemyNavigation.Component.PublicState",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeaComponentState::RunTest(const FString&)
{
    auto* C=NewObject<UShipEnemyNavigationComponent>();
    TestFalse(TEXT("New component retains legacy by default"),C->bNewNavigationEnabled);
    C->SetGeographicDestination(FVector(107.3,20.98,0));
    TestTrue(TEXT("Explicit destination sets mode"),C->NavigationMode==EShipNavigationMode::Destination);
    TestTrue(TEXT("Explicit destination requests replan"),C->NavigationState==EShipNavigationState::Replan);
    C->StopNavigation();TestTrue(TEXT("Stop enters idle"),C->NavigationState==EShipNavigationState::Idle);
    C->StartPatrol();TestTrue(TEXT("Patrol selects patrol mode"),C->NavigationMode==EShipNavigationMode::Patrol);
    TestEqual(TEXT("Heading +180 wrap"),FMath::FindDeltaAngleDegrees(179.f,-179.f),2.f);
    TestEqual(TEXT("Heading -180 wrap"),FMath::FindDeltaAngleDegrees(-179.f,179.f),-2.f);
    // A caller outside the selected arbitration pipeline cannot write commands.
    C->SetDriveCommand(1,1);TestEqual(TEXT("Inactive writer cannot issue throttle"),C->DesiredThrottle,0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeaLiveComponentCases,"ShipEnemyNavigation.Component.IsolatedWorld",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeaLiveComponentCases::RunTest(const FString&)
{
    // This world is disposable. MainMapLevel and its actors/settings never participate.
    UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,FName(TEXT("SeaNavigationAutomationWorld")));
    if(!TestNotNull(TEXT("Transient test world created"),W))return false;
    W->GetPackage()->SetFlags(RF_Transient);
    const int32 PreviousWorldContexts=GEngine->GetWorldContexts().Num();
    UGameInstance* GameInstance=NewObject<UGameInstance>(GEngine);
    FWorldContext& Context=GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.OwningGameInstance=GameInstance;
    Context.SetCurrentWorld(W);
    W->SetGameInstance(GameInstance);
    W->SetShouldTick(false);
    ON_SCOPE_EXIT
    {
        W->BeginTearingDown();
        // No world BeginPlay/tick: inherited input, buoyancy and weapon graphs never execute.
        GEngine->DestroyWorldContext(W);
        W->DestroyWorld(false);
        // Only this fixture's disposable package; never clear dirty flags on project assets.
        W->GetPackage()->SetDirtyFlag(false);
        TestEqual(TEXT("Transient world context removed during cleanup"),GEngine->GetWorldContexts().Num(),PreviousWorldContexts);
        TestFalse(TEXT("Destroyed test world is no longer rooted"),W->IsRooted());
        TestFalse(TEXT("Transient test package is clean after cleanup"),W->GetPackage()->IsDirty());
    };
    // Spawn through the real engine lifecycle. Possess alone does not register a controller
    // or create its PlayerState in a world whose actors have never been initialized.
    // Explicit native GameMode avoids loading the project's GMBP/default map.
    FURL URL;
    URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
    if(!TestTrue(TEXT("Native test GameMode created"),W->SetGameMode(URL)))return false;
    W->InitializeActorsForPlay(URL);
    TestTrue(TEXT("Test world actors initialized without starting gameplay"),W->AreActorsInitialized() && !W->HasBegunPlay());
    auto* Geo=W->SpawnActor<ACesiumGeoreference>();
    Geo->SetOriginLongitudeLatitudeHeight(FVector(107.296416,20.992597,0));
    Geo->Tags.AddUnique(FName(TEXT("DEFAULT_GEOREFERENCE")));
    UClass* EnemyClass=LoadClass<APawn>(nullptr,TEXT("/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy_C"));
    UClass* PlayerClass=LoadClass<APawn>(nullptr,TEXT("/Game/Ship/BluePrint/BP_Ship.BP_Ship_C"));
    if(!TestNotNull(TEXT("Integrated enemy class loads"),EnemyClass) || !TestNotNull(TEXT("Protected player class loads read-only"),PlayerClass))return false;
    FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    APawn* Ship=W->SpawnActor<APawn>(EnemyClass,FVector(6370,194550,680),FRotator(0,-100,0),Spawn);
    auto* C=Ship?Ship->FindComponentByClass<UShipEnemyNavigationComponent>():nullptr;
    if(!TestNotNull(TEXT("Spawned existing Blueprint has component"),C))return false;
    auto* Sensor=FindFProperty<FArrayProperty>(EnemyClass,TEXT("ScanDistances"));
    if(!TestNotNull(TEXT("Existing Blueprint sensor field"),Sensor))return false;
    FScriptArrayHelper Samples(Sensor,Sensor->ContainerPtrToValuePtr<void>(Ship));Samples.Resize(12);
    auto* Number=CastFieldChecked<FNumericProperty>(Sensor->Inner);
    for(int32 I=0;I<12;++I)Number->SetFloatingPointPropertyValue(Samples.GetRawPtr(I),80000);
    C->bNewNavigationEnabled=false;
    TestFalse(TEXT("Legacy selection returns false without becoming a writer"),C->UpdateNavigation(.1f));
    C->bNewNavigationEnabled=true;
    TestTrue(TEXT("New selection owns commands synchronously"),C->UpdateNavigation(.1f));
    TestTrue(TEXT("Native planner actually ran from arbitrary existing ship position"),C->bPathfindingSucceeded);
    if(!C->bPathfindingSucceeded){AddError(C->LastNavigationFailureReason);return false;}
    TestTrue(TEXT("Live component holds real detour waypoints"),C->CurrentRoute.Num()>2);
    TestTrue(TEXT("Commands bounded"),FMath::Abs(C->DesiredThrottle)<=1 && FMath::Abs(C->DesiredRudder)<=1);
    C->SetDriveCommand(5,-5);TestEqual(TEXT("Active command clamp throttle"),C->DesiredThrottle,1.f);TestEqual(TEXT("Active command clamp rudder"),C->DesiredRudder,-1.f);
    TestEqual(TEXT("Blueprint AIThrottle receives selected command"),C->ThrottleProperty->GetFloatingPointPropertyValue(C->ThrottleProperty->ContainerPtrToValuePtr<void>(Ship)),1.);
    TestEqual(TEXT("Blueprint AIRudder receives selected command"),C->RudderProperty->GetFloatingPointPropertyValue(C->RudderProperty->ContainerPtrToValuePtr<void>(Ship)),-1.);
    C->bNewNavigationEnabled=false;
    TestFalse(TEXT("Legacy can be restored after using native navigation"),C->UpdateNavigation(.1f));
    C->SetDriveCommand(0,0);
    TestEqual(TEXT("Deselected native writer cannot overwrite legacy throttle"),C->ThrottleProperty->GetFloatingPointPropertyValue(C->ThrottleProperty->ContainerPtrToValuePtr<void>(Ship)),1.);
    TestEqual(TEXT("Deselected native writer cannot overwrite legacy rudder"),C->RudderProperty->GetFloatingPointPropertyValue(C->RudderProperty->ContainerPtrToValuePtr<void>(Ship)),-1.);
    C->bNewNavigationEnabled=true;
    C->StopNavigation();C->UpdateNavigation(.1f);C->UpdateNavigation(.1f);
    TestTrue(TEXT("Explicit stop remains idle"),C->NavigationState==EShipNavigationState::Idle);
    TestEqual(TEXT("Explicit stop writes zero throttle"),C->DesiredThrottle,0.f);
    C->LastPlanTime=-1000;C->StartPatrol();C->UpdateNavigation(.1f);
    const auto Persistent=C->CurrentRoute;const FVector2D Before=C->PositionMetric();
    // Modify ONLY a newly created georeference in this disposable world.
    Geo->SetOriginLongitudeLatitudeHeight(FVector(107.31,21.0,0));
    C->UpdateNavigation(.1f);
    TestEqual(TEXT("Georeference transition issues neutral command"),C->DesiredThrottle,0.f);
    const FVector2D BeforeWorldRebase=C->PositionMetric();
    W->SetNewWorldOrigin(FIntVector(50000,-25000,0));
    C->UpdateNavigation(.1f);
    TestTrue(TEXT("Stable chart position survives Unreal origin rebasing"),FVector2D::Distance(BeforeWorldRebase,C->PositionMetric())<0.01);
    TestTrue(TEXT("World rebasing retains stable route"),Persistent==C->CurrentRoute);
    TestTrue(TEXT("Georeference event retains route"),Persistent==C->CurrentRoute);
    TestTrue(TEXT("Anchored position invariant under shifted georeference"),FVector2D::Distance(Before,C->PositionMetric())<0.01);
    TestEqual(TEXT("World-origin transition issues neutral command"),C->DesiredThrottle,0.f);
    C->UpdateNavigation(.1f);
    Number->SetFloatingPointPropertyValue(Samples.GetRawPtr(0),1000);
    C->DesiredThrottle=.4;C->UpdateNavigation(.1f);
    TestTrue(TEXT("Sudden front obstacle immediately suppresses forward thrust"),C->DesiredThrottle<=0);
    C->AvoidSide=1;C->AvoidElapsed=.5;C->UpdateNavigation(.1f);
    TestEqual(TEXT("Avoidance side is latched"),C->AvoidSide,1);
    C->ReadSensors();C->RecoveryPhase=2;C->RecoveryElapsed=0;C->SensorDistances[6]=0;
    float Throttle=0,Rudder=0;
    C->TryRecovery(C->PositionMetric(),0,.1,Throttle,Rudder);
    TestTrue(TEXT("Blocked rear prevents reverse"),Throttle>=0 && C->RecoveryPhase==4);
    C->RecoveryAttemptCount=C->MaxRecoveryAttempts;C->RecoveryPhase=4;C->RecoveryElapsed=3;
    C->TryRecovery(C->PositionMetric(),0,.1,Throttle,Rudder);
    TestTrue(TEXT("Recovery budget produces safe failure"),C->NavigationState==EShipNavigationState::Failed);
    C->RequestReplan();TestEqual(TEXT("Explicit retry resets recovery budget"),C->RecoveryAttemptCount,0);
    for(int32 I=0;I<12;++I)Number->SetFloatingPointPropertyValue(Samples.GetRawPtr(I),80000);
    C->Role=EShipEnemyRole::Passive;C->StartPatrol();C->PatrolWaitSeconds=0;
    // Fixture repositioning simulates arrival; no force or navigation movement is being tested here.
    const int32 Legs=C->PatrolLegsCompleted;
    for(int32 I=0;I<4;++I)
    {
        C->LastPlanTime=-1000; // simulate elapsed request cooldown without ticking physical actors
        Ship->SetActorLocation(C->GeographicToWorld(C->bTargetB?C->PatrolPointB:C->PatrolPointA));
        C->UpdateNavigation(.1f);
    }
    TestEqual(TEXT("Four simulated arrivals alternate A/B patrol legs"),C->PatrolLegsCompleted,Legs+4);
    APawn* Second=W->SpawnActor<APawn>(EnemyClass,FVector(506370,694550,680),FRotator::ZeroRotator,Spawn);
    auto* C2=Second->FindComponentByClass<UShipEnemyNavigationComponent>();C2->bNewNavigationEnabled=true;C2->InitializeNavigation();
    TestTrue(TEXT("Independent enemies share same immutable chart"),C->Map.Get()==C2->Map.Get());
    TestTrue(TEXT("Independent enemy command state"),C!=C2 && C2->RecoveryAttemptCount==0);
    APawn* Player=W->SpawnActor<APawn>(PlayerClass,Ship->GetActorLocation()+FVector(20000,0,0),FRotator::ZeroRotator,Spawn);
    auto* PC=W->SpawnActor<APlayerController>();
    if(!TestNotNull(TEXT("Player ship spawned"),Player) || !TestNotNull(TEXT("Player controller spawned"),PC))return false;
    PC->Possess(Player);
    TestTrue(TEXT("Controller genuinely possesses player ship"),PC->GetPawn()==Player && Player->GetController()==PC);
    TestNotNull(TEXT("Possessed player has initialized non-bot PlayerState"),Player->GetPlayerState());
    TestTrue(TEXT("Possessed ship is player controlled"),Player->IsPlayerControlled());
    TestTrue(TEXT("World lookup returns the possessed player ship"),UGameplayStatics::GetPlayerPawn(C,0)==Player);
    C->PlayerShipClass=PlayerClass;C->UpdatePerception();
    AddInfo(FString::Printf(TEXT("Perception diagnostic: pawn=%s candidate=%s controller=%s playerControlled=%d ship=%s player=%s second=%s"),*GetNameSafe(Player),*GetNameSafe(UGameplayStatics::GetPlayerPawn(C,0)),*GetNameSafe(Player->GetController()),Player->IsPlayerControlled(),*Ship->GetActorLocation().ToString(),*Player->GetActorLocation().ToString(),*Second->GetActorLocation().ToString()));
    TestTrue(TEXT("Distance/visibility check acquires possessed player ship"),C->DetectedPlayer==Player);
    auto ValidateBehaviorRoute=[&](const TCHAR* Behavior)
    {
        const FSeaRouteResult Route=C->Map->FindPath(C->PositionMetric(),C->GoalMetric,75000,1000);
        TestTrue(FString(Behavior)+TEXT(" objective has an actual A* route"),Route.bSuccess);
        for(int32 I=1;I<Route.Points.Num();++I)
            TestTrue(FString(Behavior)+TEXT(" smoothed route clears buffered land"),C->Map->SegmentClear(Route.Points[I-1],Route.Points[I]));
        TestTrue(FString(Behavior)+TEXT(" retains correct player target"),C->DetectedPlayer==Player);
    };
    C->Role=EShipEnemyRole::Coward;C->bHasGoal=false;C->SelectObjective(C->PositionMetric());
    TestTrue(TEXT("Coward selects a connected water escape"),C->NavigationState==EShipNavigationState::Flee && C->Map->SameWaterRegion(C->PositionMetric(),C->GoalMetric));
    ValidateBehaviorRoute(TEXT("Coward"));
    C->Role=EShipEnemyRole::Fighter;C->bHasGoal=false;C->SelectObjective(C->PositionMetric());
    TestTrue(TEXT("Fighter selects attack objective"),C->NavigationState==EShipNavigationState::Attack && C->Map->SameWaterRegion(C->PositionMetric(),C->GoalMetric));
    ValidateBehaviorRoute(TEXT("Fighter"));
    C->Role=EShipEnemyRole::Hunter;C->bHasGoal=false;C->SelectObjective(C->PositionMetric());
    TestTrue(TEXT("Hunter selects pursuit objective"),C->NavigationState==EShipNavigationState::Chase && C->Map->SameWaterRegion(C->PositionMetric(),C->GoalMetric));
    ValidateBehaviorRoute(TEXT("Hunter"));
    const FVector NearPlayerLocation=Player->GetActorLocation();
    const FVector FarPlayerLocation=Ship->GetActorLocation()+FVector((C->LoseTargetRadiusM+1000)*100,0,0);
    for(EShipEnemyRole Role:{EShipEnemyRole::Coward,EShipEnemyRole::Fighter,EShipEnemyRole::Hunter})
    {
        C->Role=Role;
        Player->SetActorLocation(NearPlayerLocation);
        C->UpdatePerception();
        TestTrue(TEXT("Each role reacquires possessed player"),C->DetectedPlayer==Player);
        C->bHasGoal=false;C->SelectObjective(C->PositionMetric());
        Player->SetActorLocation(FarPlayerLocation);
        C->UpdatePerception();
        TestNull(TEXT("Leaving loss radius clears player target"),C->DetectedPlayer.Get());
        C->LastPlanTime=-1000;C->UpdateNavigation(.1f);
        TestTrue(TEXT("Losing target returns each role to a valid patrol route"),C->bPathfindingSucceeded && C->NavigationState==EShipNavigationState::FollowRoute);
        TestTrue(TEXT("Resumed goal is the current patrol endpoint"),C->GoalMetric.Equals(C->LLHToMetric(C->bTargetB?C->PatrolPointB:C->PatrolPointA),1));
    }
    Player->SetActorLocation(NearPlayerLocation);C->UpdatePerception();
    PC->UnPossess();C->UpdatePerception();
    TestNull(TEXT("Unpossessed player ship cannot remain an engagement target"),C->DetectedPlayer.Get());
    PC->Possess(Player);C->UpdatePerception();
    TestTrue(TEXT("Repossessed ship is acquired again"),C->DetectedPlayer==Player);
    const FVector RequestedDestination=C->bTargetB?C->PatrolPointB:C->PatrolPointA;
    C->SetGeographicDestination(RequestedDestination);
    C->Role=EShipEnemyRole::Hunter;C->SelectObjective(C->PositionMetric());
    TestTrue(TEXT("Combat retains the explicitly requested final destination"),C->CurrentFinalDestination.Equals(RequestedDestination,1.e-8));
    // Commit the temporary combat route first, so resumption must invalidate a real stale route.
    TestTrue(TEXT("Temporary pursuit objective can be planned"),C->PlanRoute(C->PositionMetric(),C->GoalMetric));
    Player->SetActorLocation(FarPlayerLocation);C->UpdatePerception();
    C->LastPlanTime=-1000;C->UpdateNavigation(.1f);
    TestTrue(TEXT("Target loss restores explicit destination instead of last pursuit goal"),C->GoalMetric.Equals(C->LLHToMetric(RequestedDestination),1));
    TestTrue(TEXT("Target loss replans the restored explicit destination"),C->bPathfindingSucceeded && C->LastGoal.Equals(C->GoalMetric,1) && C->NavigationState==EShipNavigationState::FollowRoute);
    C->StartPatrol();
    FActorSpawnParameters ProjectileSpawn=Spawn;ProjectileSpawn.Owner=Ship;ProjectileSpawn.Instigator=Ship;
    auto* Projectile=W->SpawnActor<AShipEnemyProjectile>(AShipEnemyProjectile::StaticClass(),Ship->GetActorLocation(),FRotator::ZeroRotator,ProjectileSpawn);
    Projectile->LaunchVelocity=FVector(20000,0,0);Projectile->DispatchBeginPlay();
    TestTrue(TEXT("Projectile owner and instigator assigned"),Projectile->GetOwner()==Ship && Projectile->GetInstigator()==Ship);
    TestTrue(TEXT("Projectile sweep excludes firing ship"),Projectile->Collision->GetMoveIgnoreActors().Contains(Ship));
    TestEqual(TEXT("Legacy projectile gravity preserved"),Projectile->Movement->ProjectileGravityScale,1.f);
    TestEqual(TEXT("Legacy radial damage preserved"),Projectile->Damage,10000.f);
    C->Health=20000;C->OnOwnerDamaged(Ship,1000,nullptr,nullptr,Projectile);
    TestEqual(TEXT("Enemy rejects own projectile damage"),C->Health,20000.f);
    C->OnOwnerDamaged(Ship,30000,nullptr,PC,Player);
    TestTrue(TEXT("Enemy health death is terminal and neutral"),C->bDead && C->NavigationState==EShipNavigationState::Dead && C->DesiredThrottle==0);
    AddInfo(TEXT("Isolated world executes command/state/perception/ownership checks, NOT physical ship motion or final gameplay acceptance."));
    return true;
}
#endif
