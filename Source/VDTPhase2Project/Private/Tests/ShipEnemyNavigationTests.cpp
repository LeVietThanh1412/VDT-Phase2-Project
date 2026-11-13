#include "Navigation/SeaNavigationMap.h"
#include "Combat/ShipCombatComponent.h"
#include "Navigation/ShipEnemyNavigationComponent.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Utilities/NavalCesiumCompatibility.h"
#include "CesiumGeoreference.h"
#include "CesiumGlobeAnchorComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"
#include "Combat/ShipEnemyProjectile.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeaGridCases,
                                 "ShipEnemyNavigation.Planner.Geometry",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
bool FSeaGridCases::RunTest(const FString &) {
  auto Grid = []() {
    FSeaNavigationMap M;
    M.Width = 30;
    M.Height = 30;
    M.CellSize = 100;
    M.Cells.Init(1, 900);
    M.Clearance.Init(5, 900);
    for (int32 I = 0; I < 30; ++I) {
      M.Cells[I] = M.Cells[870 + I] = M.Cells[I * 30] = M.Cells[I * 30 + 29] =
          0;
    }
    return M;
  };
  auto Certify = [&](const FSeaNavigationMap &M, const FSeaRouteResult &R) {
    TestTrue(TEXT("A* succeeds"), R.bSuccess);
    for (int32 I = 1; I < R.Points.Num(); ++I)
      TestTrue(TEXT("Every smoothed segment safe"),
               M.SegmentClear(R.Points[I - 1], R.Points[I]));
  };
  auto M = Grid();
  Certify(M,
          M.FindPath(FVector2D(250, 250), FVector2D(2650, 2650), 75000, 1000));
  M.RebuildConnectedRegions();
  TestTrue(TEXT("Rebuilt open chart has connected endpoints"),
           M.SameWaterRegion(FVector2D(250, 250), FVector2D(2650, 2650)));
  for (int32 Y = 1; Y < 29; ++Y)
    M.Cells[Y * 30 + 15] = 0;
  M.RebuildConnectedRegions();
  TestFalse(TEXT("Surveyed wall splits connectivity after rebuild"),
            M.SameWaterRegion(FVector2D(250, 250), FVector2D(2650, 2650)));
  M.Cells[15 * 30 + 15] = 1;
  M.RebuildConnectedRegions();
  TestTrue(TEXT("Opening restores connectivity after rebuild"),
           M.SameWaterRegion(FVector2D(250, 250), FVector2D(2650, 2650)));
  M = Grid();
  const FVector2D OffCenter(2667.125, 2641.75);
  const auto Exact =
      M.FindPath(FVector2D(251.25, 242.5), OffCenter, 75000, 1000);
  Certify(M, Exact);
  TestTrue(TEXT("Off-center goal node is accepted water"),
           M.IsWater(M.Center(M.CellAt(OffCenter))));
  TestTrue(TEXT("Snapped grid node deliberately differs from requested point"),
           !M.Center(M.CellAt(OffCenter)).Equals(OffCenter, 1.e-6));
  TestTrue(TEXT("Safe final connector reaches exact requested coordinates"),
           Exact.bSuccess && Exact.Points.Last().Equals(OffCenter, 1.e-8));
  TestTrue(TEXT("Goal-node final connector clears all exclusions"),
           M.SegmentClear(M.Center(M.CellAt(OffCenter)), OffCenter));
  for (int32 Y = 8; Y < 22; ++Y)
    for (int32 X = 12; X < 18; ++X)
      M.Cells[Y * 30 + X] = 0;
  TestFalse(TEXT("Island blocks direct shortcut"),
            M.SegmentClear(FVector2D(350, 1550), FVector2D(2650, 1550)));
  auto R = M.FindPath(FVector2D(350, 1550), FVector2D(2650, 1550), 75000, 1000);
  Certify(M, R);
  TestTrue(TEXT("Island requires detour"), R.Points.Num() > 2);
  TestFalse(TEXT("Destination in land rejected"),
            M.FindPath(FVector2D(250, 250), FVector2D(1450, 1550)).bSuccess);
  TestFalse(TEXT("Out of coverage rejected"),
            M.FindPath(FVector2D(-1, 250), FVector2D(2650, 1550)).bSuccess);
  TestFalse(TEXT("Expansion budget enforced"),
            M.FindPath(FVector2D(350, 1550), FVector2D(2650, 1550), 1, 1000)
                .bSuccess);
  M = Grid();
  for (int32 Y = 1; Y < 29; ++Y)
    M.Cells[Y * 30 + 15] = 0;
  TestFalse(TEXT("Disconnected water returns no route"),
            M.FindPath(FVector2D(250, 250), FVector2D(2650, 2650), 75000, 1000)
                .bSuccess);
  M.Cells[15 * 30 + 15] = 1;
  Certify(M,
          M.FindPath(FVector2D(250, 1550), FVector2D(2650, 1550), 75000, 1000));
  M.Cells[15 * 30 + 15] = 0;
  TestFalse(TEXT("Passage removed by clearance inflation cannot be crossed"),
            M.FindPath(FVector2D(250, 1550), FVector2D(2650, 1550), 75000, 1000)
                .bSuccess);
  M = Grid();
  M.Cells[2 * 30 + 3] = M.Cells[3 * 30 + 2] = 0;
  TestFalse(TEXT("Diagonal cannot cut blocked corner"),
            M.SegmentClear(FVector2D(250, 250), FVector2D(350, 350)));
  M = Grid();
  const FVector2D TemporaryBlock(1450, 1450);
  const auto Detour = M.FindPath(FVector2D(250, 1450), FVector2D(2650, 1450),
                                 75000, 1000, {TemporaryBlock});
  Certify(M, Detour);
  for (int32 I = 1; I < Detour.Points.Num(); ++I) {
    const FVector2D A = Detour.Points[I - 1], D = Detour.Points[I] - A;
    const double T = FMath::Clamp(FVector2D::DotProduct(TemporaryBlock - A, D) /
                                      FMath::Max(D.SizeSquared(), 1.e-9),
                                  0., 1.);
    TestTrue(
        TEXT("All temporary-exclusion detour edges clear the entire circle"),
        FVector2D::Distance(TemporaryBlock, A + D * T) >= 150.);
  }
  TestFalse(TEXT("Temporary exclusion covering start fails safely"),
            M.FindPath(TemporaryBlock, FVector2D(2650, 1450), 75000, 1000,
                       {TemporaryBlock})
                .bSuccess);
  for (double Radius : {80., 262.5, 350.}) {
    const auto Traffic = M.FindPath(FVector2D(250, 1450), FVector2D(2650, 1450),
                                    75000, 1000, {TemporaryBlock}, Radius);
    Certify(M, Traffic);
    for (int32 I = 1; I < Traffic.Points.Num(); ++I) {
      const FVector2D A = Traffic.Points[I - 1], D = Traffic.Points[I] - A;
      const double T =
          FMath::Clamp(FVector2D::DotProduct(TemporaryBlock - A, D) /
                           FMath::Max(D.SizeSquared(), 1.e-9),
                       0., 1.);
      TestTrue(TEXT("Traffic detour honors the requested full hull/separation "
                    "radius"),
               FVector2D::Distance(TemporaryBlock, A + D * T) >= Radius);
    }
  }
  TestFalse(TEXT("Invalid distant endpoint rejected"),
            M.FindPath(FVector2D(1.e100, 0), FVector2D(250, 250)).bSuccess);
  M.Cells.Reset();
  const auto Empty = M.FindPath(FVector2D(250, 250), FVector2D(2650, 1450));
  TestTrue(TEXT("Empty malformed grid fails without a partial route"),
           !Empty.bSuccess && Empty.Points.IsEmpty() &&
               !Empty.Failure.IsEmpty());
  M = Grid();
  M.CoastWidth = M.Width;
  M.CoastHeight = M.Height;
  M.CoastCellSize = M.CellSize;
  M.CoastMinEast = M.MinEast;
  M.CoastMinNorth = M.MinNorth;
  M.CoastWater = M.Cells;
  // A real-water marker can lie inside the vessel exclusion. Its stopping
  // point must stay on safe chart water AND inside the authored arrival zone.
  M.Cells[15 * 30 + 15] = 0;
  M.RebuildConnectedRegions();
  FVector2D Approach;
  TestTrue(TEXT("Real-water excluded marker resolves a bounded approach"),
           M.ResolveArrivalGoal(FVector2D(1550, 1550), 200,
                                FVector2D(350, 1550), Approach));
  TestTrue(TEXT("Approach is accepted water within the arrival zone"),
           M.IsWater(Approach) &&
               FVector2D::Distance(Approach, FVector2D(1550, 1550)) < 190);
  Certify(M, M.FindPath(FVector2D(350, 1550), Approach, 75000, 1000));
  TestFalse(TEXT("No safe point outside a small arrival zone is accepted"),
            M.ResolveArrivalGoal(FVector2D(1550, 1550), 40,
                                 FVector2D(350, 1550), Approach));
  M.CoastWater[15 * 30 + 15] = 0;
  TestFalse(TEXT("A marker on raw land never becomes a water approach"),
            M.ResolveArrivalGoal(FVector2D(1550, 1550), 200,
                                 FVector2D(350, 1550), Approach));
  // U-shaped inlet remains escapable through its opening rather than crossing
  // its walls.
  M = Grid();
  for (int32 Y = 8; Y < 22; ++Y) {
    M.Cells[Y * 30 + 8] = 0;
    M.Cells[Y * 30 + 21] = 0;
  }
  for (int32 X = 8; X < 22; ++X)
    M.Cells[8 * 30 + X] = 0;
  Certify(M,
          M.FindPath(FVector2D(1450, 1450), FVector2D(1450, 450), 75000, 1000));
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeaRealMapCases,
                                 "ShipEnemyNavigation.Planner.RealOSM",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
bool FSeaRealMapCases::RunTest(const FString &) {
  FString Error;
  auto Map =
      FSeaNavigationMap::Load(FPaths::ProjectContentDir() /
                                  TEXT("SeaNavigation/halong_navigation.json"),
                              Error);
  if (!TestTrue(TEXT("Actual packaged OSM map loads: ") + Error, Map.IsValid()))
    return false;
  auto Again =
      FSeaNavigationMap::Load(FPaths::ProjectContentDir() /
                                  TEXT("SeaNavigation/halong_navigation.json"),
                              Error);
  TestTrue(TEXT("Multiple ships share immutable map"),
           Map.Get() == Again.Get());
  FString Data;
  TSharedPtr<FJsonObject> J;
  if (!FFileHelper::LoadFileToString(
          Data, *(FPaths::ProjectDir() /
                  TEXT("Data/SeaNavigation/validation_report.json"))) ||
      !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Data), J)) {
    AddError(TEXT("Missing independently validated real-data examples"));
    return false;
  }
  int32 Detours = 0;
  for (const auto &V : J->GetArrayField(TEXT("examples"))) {
    auto E = V->AsObject();
    const auto &A = E->GetArrayField(TEXT("start_en_m"));
    const auto &B = E->GetArrayField(TEXT("goal_en_m"));
    auto R = Map->FindPath(FVector2D(A[0]->AsNumber(), A[1]->AsNumber()),
                           FVector2D(B[0]->AsNumber(), B[1]->AsNumber()), 75000,
                           1000);
    TestTrue(E->GetStringField(TEXT("name")), R.bSuccess);
    for (int32 I = 1; I < R.Points.Num(); ++I)
      TestTrue(TEXT("Native smoothed OSM segment safe"),
               Map->SegmentClear(R.Points[I - 1], R.Points[I]));
    if (!Map->SegmentClear(FVector2D(A[0]->AsNumber(), A[1]->AsNumber()),
                           FVector2D(B[0]->AsNumber(), B[1]->AsNumber()))) {
      ++Detours;
      TestTrue(TEXT("Actual island detour has intermediate waypoints"),
               R.Points.Num() > 2);
    }
    auto Return = Map->FindPath(FVector2D(B[0]->AsNumber(), B[1]->AsNumber()),
                                FVector2D(A[0]->AsNumber(), A[1]->AsNumber()),
                                75000, 1000);
    TestTrue(TEXT("Reverse patrol leg is routable"), Return.bSuccess);
  }
  TestTrue(TEXT("At least one genuine OSM island detour executed"),
           Detours > 0);
  const FVector2D PlacedShip(-262.7, -1945.5), OpenSea(-600, -2200),
      OtherOpenSea(-200, -2400);
  const auto Direct = Map->FindPath(PlacedShip, OpenSea, 75000, 1000);
  TestTrue(TEXT("New open-sea target needs only start and exact destination; "
                "no fixed right-side waypoints"),
           Direct.bSuccess && Direct.Points.Num() == 2 &&
               Direct.Points.Last().Equals(OpenSea, 1.e-8));
  const auto Different = Map->FindPath(PlacedShip, OtherOpenSea, 75000, 1000);
  TestTrue(TEXT("Fresh A* request follows a different current target instead "
                "of reusing the previous endpoint"),
           Different.bSuccess && Different.Expansions > 0 &&
               Different.Points.Last().Equals(OtherOpenSea, 1.e-8) &&
               Different.Points != Direct.Points);
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeaComponentState,
                                 "ShipEnemyNavigation.Component.PublicState",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
bool FSeaComponentState::RunTest(const FString &) {
  auto *C = NewObject<UShipEnemyNavigationComponent>();
  TestFalse(TEXT("New component retains legacy by default"),
            C->bNewNavigationEnabled);
  C->SetGeographicDestination(FVector(107.3, 20.98, 0));
  TestTrue(TEXT("Explicit destination sets mode"),
           C->NavigationMode == EShipNavigationMode::Destination);
  TestTrue(TEXT("Explicit destination requests replan"),
           C->NavigationState == EShipNavigationState::Replan);
  C->StopNavigation();
  TestTrue(TEXT("Stop enters idle"),
           C->NavigationState == EShipNavigationState::Idle);
  C->StartPatrol();
  TestTrue(TEXT("Patrol selects patrol mode"),
           C->NavigationMode == EShipNavigationMode::Patrol);
  TestEqual(TEXT("Heading +180 wrap"),
            FMath::FindDeltaAngleDegrees(179.f, -179.f), 2.f);
  TestEqual(TEXT("Heading -180 wrap"),
            FMath::FindDeltaAngleDegrees(-179.f, 179.f), -2.f);
  // A caller outside the selected arbitration pipeline cannot write commands.
  C->SetDriveCommand(1, 1);
  TestEqual(TEXT("Inactive writer cannot issue throttle"), C->DesiredThrottle,
            0.f);
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeaLiveComponentCases,
                                 "ShipEnemyNavigation.Component.IsolatedWorld",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)
bool FSeaLiveComponentCases::RunTest(const FString &) {
  // This world is disposable. MainMapLevel and its actors/settings never
  // participate.
  UWorld *W = UWorld::CreateWorld(EWorldType::Game, false,
                                  FName(TEXT("SeaNavigationAutomationWorld")));
  if (!TestNotNull(TEXT("Transient test world created"), W))
    return false;
  W->GetPackage()->SetFlags(RF_Transient);
  const int32 PreviousWorldContexts = GEngine->GetWorldContexts().Num();
  UGameInstance *GameInstance = NewObject<UGameInstance>(GEngine);
  FWorldContext &Context = GEngine->CreateNewWorldContext(EWorldType::Game);
  Context.OwningGameInstance = GameInstance;
  Context.SetCurrentWorld(W);
  W->SetGameInstance(GameInstance);
  W->SetShouldTick(false);
  ON_SCOPE_EXIT {
    W->BeginTearingDown();
    // No world BeginPlay/tick: inherited input, buoyancy and weapon graphs
    // never execute.
    GEngine->DestroyWorldContext(W);
    W->DestroyWorld(false);
    // Only this fixture's disposable package; never clear dirty flags on
    // project assets.
    W->GetPackage()->SetDirtyFlag(false);
    TestEqual(TEXT("Transient world context removed during cleanup"),
              GEngine->GetWorldContexts().Num(), PreviousWorldContexts);
    TestFalse(TEXT("Destroyed test world is no longer rooted"), W->IsRooted());
    TestFalse(TEXT("Transient test package is clean after cleanup"),
              W->GetPackage()->IsDirty());
  };
  // Spawn through the real engine lifecycle. Possess alone does not register a
  // controller or create its PlayerState in a world whose actors have never
  // been initialized. Explicit native GameMode avoids loading the project's
  // GMBP/default map.
  FURL URL;
  URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
  if (!TestTrue(TEXT("Native test GameMode created"), W->SetGameMode(URL)))
    return false;
  W->InitializeActorsForPlay(URL);
  TestTrue(TEXT("Test world actors initialized without starting gameplay"),
           W->AreActorsInitialized() && !W->HasBegunPlay());
  auto *Geo = W->SpawnActor<ACesiumGeoreference>();
  Geo->SetOriginLongitudeLatitudeHeight(FVector(107.296416, 20.992597, 0));
  Geo->Tags.AddUnique(FName(TEXT("DEFAULT_GEOREFERENCE")));
  UClass *EnemyClass = LoadClass<APawn>(
      nullptr, TEXT("/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy_C"));
  UClass *PlayerClass =
      LoadClass<APawn>(nullptr, TEXT("/Game/Ship/BluePrint/BP_Ship.BP_Ship_C"));
  if (!TestNotNull(TEXT("Integrated enemy class loads"), EnemyClass) ||
      !TestNotNull(TEXT("Protected player class loads read-only"), PlayerClass))
    return false;
  FActorSpawnParameters Spawn;
  Spawn.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  APawn *Ship = W->SpawnActor<APawn>(EnemyClass, FVector(6370, 194550, 680),
                                     FRotator(0, -100, 0), Spawn);
  auto *C = Ship ? Ship->FindComponentByClass<UShipEnemyNavigationComponent>()
                 : nullptr;
  if (!TestNotNull(TEXT("Spawned existing Blueprint has component"), C))
    return false;
  // This unticked fixture exercises the retained legacy perception/damage
  // fallback. Real shared-combat ownership is covered by the PIE suite.
  // Saved Blueprint assets and the main world never participate in this edit.
  if (auto *Combat = Ship->FindComponentByClass<UShipCombatComponent>())
    Combat->DestroyComponent();
  if (auto *Weapon = Ship->FindComponentByClass<UShipWeaponComponent>())
    Weapon->DestroyComponent();
  // Blueprint globe anchors can retain an authored ECEF transform. Explicitly
  // place this isolated fixture through its anchor rather than assuming the
  // spawn transform wins over serialized geospatial state during registration.
  auto *FixtureAnchor =
      Ship->FindComponentByClass<UCesiumGlobeAnchorComponent>();
  if (!TestNotNull(TEXT("Fixture has geographic anchor"), FixtureAnchor))
    return false;
  FixtureAnchor->SetGeoreference(Geo);
  FixtureAnchor->MoveToLongitudeLatitudeHeight(
      Geo->TransformUnrealPositionToLongitudeLatitudeHeight(
          FVector(6370, 194550, 680)));
  FixtureAnchor->Sync();
  auto *Sensor =
      FindFProperty<FArrayProperty>(EnemyClass, TEXT("ScanDistances"));
  if (!TestNotNull(TEXT("Existing Blueprint sensor field"), Sensor))
    return false;
  FScriptArrayHelper Samples(Sensor,
                             Sensor->ContainerPtrToValuePtr<void>(Ship));
  Samples.Resize(12);
  auto *Number = CastFieldChecked<FNumericProperty>(Sensor->Inner);
  for (int32 I = 0; I < 12; ++I)
    Number->SetFloatingPointPropertyValue(Samples.GetRawPtr(I), 80000);
  // Exercise the real Blueprint DestinationActor adapter before the patrol
  // checks.
  const FVector InitialShipPosition = Ship->GetActorLocation();
  auto *DestinationProperty =
      FindFProperty<FObjectPropertyBase>(EnemyClass, TEXT("DestinationActor"));
  if (!TestNotNull(TEXT("Existing enemy exposes DestinationActor"),
                   DestinationProperty))
    return false;
  auto *TargetPoint = W->SpawnActor<ATargetPoint>();
  C->bNewNavigationEnabled = true;
  if (!TestTrue(
          TEXT("Destination adapter initializes existing ship components"),
          C->InitializeNavigation()))
    return false;
  AddInfo(FString::Printf(TEXT("Fixture start chart=%s world=%s geo=%s"),
                          *C->PositionMetric().ToString(),
                          *Ship->GetActorLocation().ToString(),
                          *GetNameSafe(C->Georeference)));
  if (!TestTrue(TEXT("Fixture physical start is accepted chart water"),
                C->Map->IsWater(C->PositionMetric())))
    return false;
  // Regression for the real PIE false positive: wide horizontal clearance must
  // not turn into a 40 m draft and collide with water-floor terrain at 24 m
  // depth.
  AActor *Floor = W->SpawnActor<AActor>();
  UBoxComponent *FloorShape = NewObject<UBoxComponent>(Floor);
  Floor->SetRootComponent(FloorShape);
  Floor->AddInstanceComponent(FloorShape);
  FloorShape->SetBoxExtent(FVector(30000, 30000, 50));
  FloorShape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
  FloorShape->SetCollisionResponseToAllChannels(ECR_Block);
  FloorShape->RegisterComponent();
  Floor->SetActorLocation(C->Hull->GetComponentLocation() -
                          FVector(0, 0, 2400));
  const FVector SweepStart = C->Hull->GetComponentLocation(),
                SweepEnd = SweepStart + FVector(10000, 0, 0);
  TestTrue(
      TEXT("Hull clearance sweep does not mistake deep seabed for an obstacle"),
      C->SweepClear(SweepStart, SweepEnd, 42));
  const FQuat Upright = C->Hull->GetComponentQuat();
  C->Hull->SetWorldRotation(FRotator(0, -100, 8));
  TestTrue(
      TEXT("Wide turning sweep preserves real keel depth while the hull rolls"),
      C->SweepClear(SweepStart, SweepStart, 111));
  C->Hull->SetWorldRotation(Upright);
  Floor->SetActorLocation(SweepStart + FVector(0, 0, -500));
  TestFalse(TEXT("Hull clearance sweep still blocks terrain above the keel"),
            C->SweepClear(SweepStart, SweepEnd, 42));
  Floor->Destroy();
  TargetPoint->SetActorLocation(C->GeographicToWorld(C->PatrolPointB));
  DestinationProperty->SetObjectPropertyValue_InContainer(Ship, TargetPoint);
  C->NavigationMode = EShipNavigationMode::Destination;
  TestTrue(TEXT("DestinationActor adapter owns command pipeline"),
           C->UpdateNavigation(.1f));
  TestTrue(TEXT("Blueprint TargetPoint produces a real A* route"),
           C->bPathfindingSucceeded && C->CurrentRoute.Num() > 2);
  TestTrue(TEXT("TargetPoint is converted into correct geographic goal"),
           C->GoalMetric.Equals(C->LLHToMetric(C->PatrolPointB), 1));
  TestTrue(
      TEXT("Requested destination round-trips to actual actor world position"),
      FVector::Distance(C->GeographicToWorld(C->CurrentFinalDestination),
                        TargetPoint->GetActorLocation()) < .01);
  TestTrue(
      TEXT("Final actual world waypoint matches exact actor including height"),
      !C->CurrentRoute.IsEmpty() &&
          FVector::Distance(C->MetricToWorld(FVector2D(C->CurrentRoute.Last())),
                            TargetPoint->GetActorLocation()) < .01);
  const auto UnchangedRoute = C->CurrentRoute;
  const double UnchangedPlanTime = C->LastPlanTime;
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("Stationary actor reuses route without another A* search"),
           C->CurrentRoute == UnchangedRoute &&
               C->LastPlanTime == UnchangedPlanTime);
  const FVector SavedTarget = TargetPoint->GetActorLocation();
  const FVector2D MovedGoal = C->GoalMetric + FVector2D(10, 0);
  TargetPoint->SetActorLocation(C->MetricToWorld(MovedGoal));
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("Moving target invalidates stale route and waits for replan "
                "interval"),
           C->CurrentRoute.IsEmpty() &&
               C->NavigationState == EShipNavigationState::Replan &&
               C->DesiredThrottle == 0);
  C->LastPlanTime = -1000;
  C->UpdateNavigation(.1f);
  TestTrue(
      TEXT("Moving actor produces new exact final waypoint"),
      C->bPathfindingSucceeded && C->GoalMetric.Equals(MovedGoal, .01) &&
          FVector::Distance(C->MetricToWorld(FVector2D(C->CurrentRoute.Last())),
                            TargetPoint->GetActorLocation()) < .01);
  TargetPoint->SetActorLocation(SavedTarget);
  C->LastPlanTime = -1000;
  C->UpdateNavigation(.1f);
  for (int32 I = 1; I < C->CurrentRoute.Num(); ++I)
    TestTrue(TEXT("TargetPoint route segments clear buffered land"),
             C->Map->SegmentClear(FVector2D(C->CurrentRoute[I - 1]),
                                  FVector2D(C->CurrentRoute[I])));
  Ship->SetActorLocation(TargetPoint->GetActorLocation());
  // This fixture simulates a fully stopped arrival rather than ticking physics.
  C->SetDriveCommand(0, 0);
  C->UpdateNavigation(.1f);
  TestTrue(
      TEXT("DestinationActor arrival stops without starting a patrol return"),
      C->NavigationState == EShipNavigationState::Arrived &&
          C->DesiredThrottle == 0 && C->DesiredRudder == 0 &&
          C->PatrolLegsCompleted == 0);
  Ship->SetActorLocation(InitialShipPosition);
  C->SetGeographicDestination(C->PatrolPointA);
  C->LastPlanTime = -1000;
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("Explicit geographic command overrides assigned TargetPoint"),
           C->GoalMetric.Equals(C->LLHToMetric(C->PatrolPointA), 1));
  C->bHasGoal = false;
  DestinationProperty->SetObjectPropertyValue_InContainer(Ship, nullptr);
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("Missing DestinationActor fails with neutral commands"),
           C->NavigationState == EShipNavigationState::Failed &&
               C->DesiredThrottle == 0 && C->DesiredRudder == 0);
  TargetPoint->SetActorLocation(
      C->GeographicToWorld(FVector(107.29214425167515, 20.983569683709124, 0)));
  DestinationProperty->SetObjectPropertyValue_InContainer(Ship, TargetPoint);
  C->LastPlanTime = -1000;
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("Blocked TargetPoint is rejected without forward thrust"),
           !C->bPathfindingSucceeded &&
               C->NavigationState == EShipNavigationState::Failed &&
               C->DesiredThrottle == 0);
  TargetPoint->SetActorLocation(SavedTarget);
  C->LastPlanTime = -1000;
  C->UpdateNavigation(.1f);
  TestTrue(
      TEXT("Moving unreachable actor back to water recovers automatically"),
      C->bPathfindingSucceeded &&
          C->NavigationState != EShipNavigationState::Failed);
  // A real anchored destination must follow Cesium's changing local frame.
  TargetPoint->GetRootComponent()->SetMobility(EComponentMobility::Movable);
  auto *TargetAnchor = NewObject<UCesiumGlobeAnchorComponent>(TargetPoint);
  TargetPoint->AddInstanceComponent(TargetAnchor);
  TargetAnchor->RegisterComponent();
  const auto ActorRoute = C->CurrentRoute;
  const FVector ActorLLH = C->CurrentFinalDestination;
  const FVector OldOrigin = Geo->GetOriginLongitudeLatitudeHeight();
  Geo->SetOriginLongitudeLatitudeHeight(FVector(107.31, 21.0, 0));
  C->UpdateNavigation(.1f);
  C->UpdateNavigation(.1f);
  TestTrue(
      TEXT("Anchored target and exact endpoint remain aligned after "
           "georeference shift"),
      !C->CurrentRoute.IsEmpty() && C->CurrentRoute == ActorRoute &&
          FVector::Distance(C->MetricToWorld(FVector2D(C->CurrentRoute.Last())),
                            TargetPoint->GetActorLocation()) < .01 &&
          C->CurrentFinalDestination.Equals(ActorLLH, 1.e-8));
  W->SetNewWorldOrigin(FIntVector(20000, -30000, 0));
  C->UpdateNavigation(.1f);
  C->UpdateNavigation(.1f);
  TestTrue(
      TEXT("Anchored target route remains aligned after world-origin rebasing"),
      !C->CurrentRoute.IsEmpty() && C->CurrentRoute == ActorRoute &&
          FVector::Distance(C->MetricToWorld(FVector2D(C->CurrentRoute.Last())),
                            TargetPoint->GetActorLocation()) < .01);
  W->SetNewWorldOrigin(FIntVector::ZeroValue);
  Geo->SetOriginLongitudeLatitudeHeight(OldOrigin);
  C->UpdateNavigation(.1f);
  C->UpdateNavigation(.1f);
  TargetPoint->Destroy();
  C->UpdateNavigation(.1f);
  TestTrue(
      TEXT("Destroyed actor safely clears route and sends neutral commands"),
      C->NavigationState == EShipNavigationState::Failed &&
          C->CurrentRoute.IsEmpty() && C->DesiredThrottle == 0 &&
          C->DesiredRudder == 0);
  C->SetGeographicDestination(C->PatrolPointB);
  C->LastPlanTime = -1000;
  C->UpdateNavigation(.1f);
  C->CurrentRoute.Reset();
  C->bNeedsPlan = false;
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("Empty runtime path fails with neutral commands"),
           C->NavigationState == EShipNavigationState::Failed &&
               C->DesiredThrottle == 0 && C->DesiredRudder == 0);
  C->RequestReplan();
  C->LastPlanTime = -1000;
  C->UpdateNavigation(.1f);
  C->CurrentWaypointIndex = C->CurrentRoute.Num() + 1;
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("Invalid waypoint index fails with neutral commands"),
           C->NavigationState == EShipNavigationState::Failed &&
               C->DesiredThrottle == 0 && C->DesiredRudder == 0);
  C->LastPlanTime = -1000;
  C->StartPatrol();
  C->bNewNavigationEnabled = false;
  TestFalse(TEXT("Legacy selection returns false without becoming a writer"),
            C->UpdateNavigation(.1f));
  C->bNewNavigationEnabled = true;
  TestTrue(TEXT("New selection owns commands synchronously"),
           C->UpdateNavigation(.1f));
  TestTrue(
      TEXT("Native planner actually ran from arbitrary existing ship position"),
      C->bPathfindingSucceeded);
  if (!C->bPathfindingSucceeded) {
    AddError(C->LastNavigationFailureReason);
    return false;
  }
  TestTrue(TEXT("Live component holds real detour waypoints"),
           C->CurrentRoute.Num() > 2);
  // A developer can move a previously failed ship without moving its goal.
  // The old route/recovery state must not survive that physical-start change.
  const FVector BeforeRepositionWorld = Ship->GetActorLocation();
  auto *RepositionAnchor =
      Ship->FindComponentByClass<UCesiumGlobeAnchorComponent>();
  const FVector2D BeforeReposition = C->PositionMetric();
  FVector2D ValidReposition = BeforeReposition;
  for (int32 Direction = 0; Direction < 8; ++Direction) {
    const double Angle = Direction * UE_PI / 4.;
    const FVector2D Candidate =
        BeforeReposition +
        FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * 100.;
    if (C->Map->IsWater(Candidate) &&
        C->Map->FindPath(Candidate, C->GoalMetric, 75000, 1000).bSuccess) {
      ValidReposition = Candidate;
      break;
    }
  }
  TestTrue(TEXT("Reposition fixture found independent valid water"),
           !ValidReposition.Equals(BeforeReposition, 1));
  C->UpdateNavigation(.1f);
  const int32 BeforeResetCount = C->RepositionReinitializations;
  C->Fail(TEXT("Fixture previous recovery exhausted"));
  C->RecoveryAttemptCount = C->MaxRecoveryAttempts;
  Ship->SetActorLocation(C->MetricToWorld(ValidReposition));
  RepositionAnchor->Sync();
  C->UpdateNavigation(.1f);
  TestEqual(TEXT("Physical development jump invalidates navigation once"),
            C->RepositionReinitializations, BeforeResetCount + 1);
  TestTrue(TEXT("Valid reposition escapes previous failure and starts real A* "
                "at new position"),
           C->bPathfindingSucceeded && !C->CurrentRoute.IsEmpty() &&
               FVector2D(C->CurrentRoute[0]).Equals(C->PositionMetric(), .1));
  TestTrue(TEXT("Reposition retains the actual assigned destination"),
           C->NavigationMode == EShipNavigationMode::Patrol);
  const double AfterRepositionPlanTime = C->LastPlanTime;
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("No every-tick replan after reposition"),
           C->RepositionReinitializations == BeforeResetCount + 1 &&
               C->LastPlanTime == AfterRepositionPlanTime);
  Ship->SetActorLocation(
      C->GeographicToWorld(FVector(107.29214425167515, 20.983569683709124, 0)));
  RepositionAnchor->Sync();
  C->UpdateNavigation(.1f);
  TestTrue(
      TEXT("Development placement on land is rejected with neutral commands"),
      C->NavigationState == EShipNavigationState::Failed &&
          C->DesiredThrottle == 0 && C->DesiredRudder == 0);
  Ship->SetActorLocation(BeforeRepositionWorld);
  RepositionAnchor->Sync();
  C->UpdateNavigation(.1f);
  TestTrue(
      TEXT(
          "Returning a grounded development placement to valid water recovers"),
      C->bPathfindingSucceeded &&
          C->NavigationState != EShipNavigationState::Failed);
  // RemovedFromWorld may be followed by reactivation of the same component.
  // EndPlay previously reset Map but left bInitialized true, crashing the next
  // Tick. Only this native component begins play; the world/ship Blueprint stay
  // unticked.
  C->BeginPlay();
  C->EndPlay(EEndPlayReason::RemovedFromWorld);
  TestTrue(TEXT("Streaming teardown invalidates navigation initialization"),
           !C->bInitialized && !C->Map.IsValid());
  C->BeginPlay();
  C->LastPlanTime = -1000;
  TestTrue(TEXT("Reactivated component safely resumes navigation ownership"),
           C->UpdateNavigation(.1f));
  TestTrue(TEXT("Reactivation reloads map and plans a valid water route"),
           C->bInitialized && C->Map.IsValid() && C->bPathfindingSucceeded &&
               C->CurrentRoute.Num() > 2);
  C->Map.Reset();
  C->bInitialized = true;
  C->LastPlanTime = -1000;
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("Stale initialized flag cannot bypass missing map recovery"),
           C->Map.IsValid() && C->bInitialized && C->bPathfindingSucceeded);
  C->EndPlay(EEndPlayReason::RemovedFromWorld);
  C->LastPlanTime = -1000;
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("Commands bounded"), FMath::Abs(C->DesiredThrottle) <= 1 &&
                                         FMath::Abs(C->DesiredRudder) <= 1);
  C->SetDriveCommand(5, -5);
  TestEqual(TEXT("Active command clamp throttle"), C->DesiredThrottle, 1.f);
  TestEqual(TEXT("Active command clamp rudder"), C->DesiredRudder, -1.f);
  TestEqual(TEXT("Blueprint AIThrottle receives selected command"),
            C->ThrottleProperty->GetFloatingPointPropertyValue(
                C->ThrottleProperty->ContainerPtrToValuePtr<void>(Ship)),
            1.);
  TestEqual(TEXT("Blueprint AIRudder receives selected command"),
            C->RudderProperty->GetFloatingPointPropertyValue(
                C->RudderProperty->ContainerPtrToValuePtr<void>(Ship)),
            -1.);
  C->bNewNavigationEnabled = false;
  TestFalse(TEXT("Legacy can be restored after using native navigation"),
            C->UpdateNavigation(.1f));
  C->SetDriveCommand(0, 0);
  TestEqual(TEXT("Deselected native writer cannot overwrite legacy throttle"),
            C->ThrottleProperty->GetFloatingPointPropertyValue(
                C->ThrottleProperty->ContainerPtrToValuePtr<void>(Ship)),
            1.);
  TestEqual(TEXT("Deselected native writer cannot overwrite legacy rudder"),
            C->RudderProperty->GetFloatingPointPropertyValue(
                C->RudderProperty->ContainerPtrToValuePtr<void>(Ship)),
            -1.);
  C->bNewNavigationEnabled = true;
  C->StopNavigation();
  C->UpdateNavigation(.1f);
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("Explicit stop remains idle"),
           C->NavigationState == EShipNavigationState::Idle);
  TestEqual(TEXT("Explicit stop writes zero throttle"), C->DesiredThrottle,
            0.f);
  C->LastPlanTime = -1000;
  C->StartPatrol();
  C->UpdateNavigation(.1f);
  const auto Persistent = C->CurrentRoute;
  const FVector2D Before = C->PositionMetric();
  // Modify ONLY a newly created georeference in this disposable world.
  Geo->SetOriginLongitudeLatitudeHeight(FVector(107.31, 21.0, 0));
  C->UpdateNavigation(.1f);
  TestEqual(TEXT("Georeference transition issues neutral command"),
            C->DesiredThrottle, 0.f);
  const FVector2D BeforeWorldRebase = C->PositionMetric();
  W->SetNewWorldOrigin(FIntVector(50000, -25000, 0));
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("Stable chart position survives Unreal origin rebasing"),
           FVector2D::Distance(BeforeWorldRebase, C->PositionMetric()) < 0.01);
  TestTrue(TEXT("World rebasing retains stable route"),
           Persistent == C->CurrentRoute);
  TestTrue(TEXT("Georeference event retains route"),
           Persistent == C->CurrentRoute);
  TestTrue(TEXT("Anchored position invariant under shifted georeference"),
           FVector2D::Distance(Before, C->PositionMetric()) < 0.01);
  TestEqual(TEXT("World-origin transition issues neutral command"),
            C->DesiredThrottle, 0.f);
  C->UpdateNavigation(.1f);
  Number->SetFloatingPointPropertyValue(Samples.GetRawPtr(0), 1000);
  C->DesiredThrottle = .4;
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("Sudden front obstacle immediately suppresses forward thrust"),
           C->DesiredThrottle <= 0);
  C->AvoidSide = 1;
  C->AvoidElapsed = .5;
  C->UpdateNavigation(.1f);
  TestEqual(TEXT("Avoidance side is latched"), C->AvoidSide, 1);
  C->ReadSensors();
  C->RecoveryPhase = 2;
  C->RecoveryElapsed = 0;
  C->SensorDistances[6] = 0;
  float Throttle = 0, Rudder = 0;
  C->TryRecovery(C->PositionMetric(), 0, .1, Throttle, Rudder);
  TestTrue(TEXT("Blocked rear prevents reverse"),
           Throttle >= 0 && C->RecoveryPhase == 4);
  C->RecoveryAttemptCount = C->MaxRecoveryAttempts;
  C->RecoveryPhase = 4;
  C->RecoveryElapsed = 3;
  C->TryRecovery(C->PositionMetric(), 0, .1, Throttle, Rudder);
  TestTrue(TEXT("Recovery budget produces safe failure"),
           C->NavigationState == EShipNavigationState::Failed);
  C->RequestReplan();
  TestEqual(TEXT("Explicit retry resets recovery budget"),
            C->RecoveryAttemptCount, 0);
  for (int32 I = 0; I < 12; ++I)
    Number->SetFloatingPointPropertyValue(Samples.GetRawPtr(I), 80000);
  C->Role = EShipEnemyRole::Passive;
  C->StartPatrol();
  C->PatrolWaitSeconds = 0;
  // Fixture repositioning simulates arrival; no force or navigation movement is
  // being tested here.
  const int32 Legs = C->PatrolLegsCompleted;
  for (int32 I = 0; I < 4; ++I) {
    C->LastPlanTime = -1000; // simulate elapsed request cooldown without
                             // ticking physical actors
    Ship->SetActorLocation(
        C->GeographicToWorld(C->bTargetB ? C->PatrolPointB : C->PatrolPointA));
    C->UpdateNavigation(.1f);
  }
  TestEqual(TEXT("Four simulated arrivals alternate A/B patrol legs"),
            C->PatrolLegsCompleted, Legs + 4);
  APawn *Second = W->SpawnActor<APawn>(EnemyClass, FVector(506370, 694550, 680),
                                       FRotator::ZeroRotator, Spawn);
  auto PlaceFixture = [Geo](APawn *Pawn, const FVector &World) {
    auto *Anchor = Pawn->FindComponentByClass<UCesiumGlobeAnchorComponent>();
    Anchor->SetGeoreference(Geo);
    Anchor->MoveToLongitudeLatitudeHeight(
        Geo->TransformUnrealPositionToLongitudeLatitudeHeight(World));
    Anchor->Sync();
  };
  PlaceFixture(Second, FVector(506370, 694550, 680));
  auto *C2 = Second->FindComponentByClass<UShipEnemyNavigationComponent>();
  C2->bNewNavigationEnabled = true;
  C2->InitializeNavigation();
  TestTrue(TEXT("Independent enemies share same immutable chart"),
           C->Map.Get() == C2->Map.Get());
  TestTrue(TEXT("Independent enemy command state"),
           C != C2 && C2->RecoveryAttemptCount == 0);
  APawn *Player = W->SpawnActor<APawn>(
      PlayerClass, Ship->GetActorLocation() + FVector(20000, 0, 0),
      FRotator::ZeroRotator, Spawn);
  auto *PC = W->SpawnActor<APlayerController>();
  if (!TestNotNull(TEXT("Player ship spawned"), Player) ||
      !TestNotNull(TEXT("Player controller spawned"), PC))
    return false;
  PlaceFixture(Player, Ship->GetActorLocation() + FVector(20000, 0, 0));
  PC->Possess(Player);
  TestTrue(TEXT("Controller genuinely possesses player ship"),
           PC->GetPawn() == Player && Player->GetController() == PC);
  TestNotNull(TEXT("Possessed player has initialized non-bot PlayerState"),
              Player->GetPlayerState());
  TestTrue(TEXT("Possessed ship is player controlled"),
           Player->IsPlayerControlled());
  TestTrue(TEXT("World lookup returns the possessed player ship"),
           UGameplayStatics::GetPlayerPawn(C, 0) == Player);
  C->PlayerShipClass = PlayerClass;
  C->UpdatePerception();
  AddInfo(FString::Printf(
      TEXT("Perception diagnostic: pawn=%s candidate=%s controller=%s "
           "playerControlled=%d ship=%s player=%s second=%s"),
      *GetNameSafe(Player), *GetNameSafe(UGameplayStatics::GetPlayerPawn(C, 0)),
      *GetNameSafe(Player->GetController()), Player->IsPlayerControlled(),
      *Ship->GetActorLocation().ToString(),
      *Player->GetActorLocation().ToString(),
      *Second->GetActorLocation().ToString()));
  TestTrue(TEXT("Distance/visibility check acquires possessed player ship"),
           C->DetectedPlayer == Player);
  auto ValidateBehaviorRoute = [&](const TCHAR *Behavior) {
    const FSeaRouteResult Route =
        C->Map->FindPath(C->PositionMetric(), C->GoalMetric, 75000, 1000);
    TestTrue(FString(Behavior) + TEXT(" objective has an actual A* route"),
             Route.bSuccess);
    for (int32 I = 1; I < Route.Points.Num(); ++I)
      TestTrue(FString(Behavior) + TEXT(" smoothed route clears buffered land"),
               C->Map->SegmentClear(Route.Points[I - 1], Route.Points[I]));
    TestTrue(FString(Behavior) + TEXT(" retains correct player target"),
             C->DetectedPlayer == Player);
  };
  C->Role = EShipEnemyRole::Coward;
  C->bHasGoal = false;
  C->SelectObjective(C->PositionMetric());
  TestTrue(TEXT("Coward selects a connected water escape"),
           C->NavigationState == EShipNavigationState::Flee &&
               C->Map->SameWaterRegion(C->PositionMetric(), C->GoalMetric));
  ValidateBehaviorRoute(TEXT("Coward"));
  C->Role = EShipEnemyRole::Fighter;
  C->bHasGoal = false;
  C->SelectObjective(C->PositionMetric());
  TestTrue(TEXT("Fighter selects attack objective"),
           C->NavigationState == EShipNavigationState::Attack &&
               C->Map->SameWaterRegion(C->PositionMetric(), C->GoalMetric));
  ValidateBehaviorRoute(TEXT("Fighter"));
  C->Role = EShipEnemyRole::Hunter;
  C->bHasGoal = false;
  C->SelectObjective(C->PositionMetric());
  TestTrue(TEXT("Hunter selects pursuit objective"),
           C->NavigationState == EShipNavigationState::Chase &&
               C->Map->SameWaterRegion(C->PositionMetric(), C->GoalMetric));
  ValidateBehaviorRoute(TEXT("Hunter"));
  const FVector NearPlayerLocation = Player->GetActorLocation();
  const FVector FarPlayerLocation =
      Ship->GetActorLocation() +
      FVector((C->LoseTargetRadiusM + 1000) * 100, 0, 0);
  for (EShipEnemyRole Role : {EShipEnemyRole::Coward, EShipEnemyRole::Fighter,
                              EShipEnemyRole::Hunter}) {
    C->Role = Role;
    Player->SetActorLocation(NearPlayerLocation);
    C->UpdatePerception();
    TestTrue(TEXT("Each role reacquires possessed player"),
             C->DetectedPlayer == Player);
    C->bHasGoal = false;
    C->SelectObjective(C->PositionMetric());
    Player->SetActorLocation(FarPlayerLocation);
    C->UpdatePerception();
    TestNull(TEXT("Leaving loss radius clears player target"),
             C->DetectedPlayer.Get());
    C->LastPlanTime = -1000;
    C->UpdateNavigation(.1f);
    TestTrue(TEXT("Losing target returns each role to a valid patrol route"),
             C->bPathfindingSucceeded &&
                 C->NavigationState == EShipNavigationState::FollowRoute);
    TestTrue(TEXT("Resumed goal is the current patrol endpoint"),
             C->GoalMetric.Equals(C->LLHToMetric(C->bTargetB ? C->PatrolPointB
                                                             : C->PatrolPointA),
                                  1));
  }
  Player->SetActorLocation(NearPlayerLocation);
  C->UpdatePerception();
  PC->UnPossess();
  C->UpdatePerception();
  TestNull(TEXT("Unpossessed player ship cannot remain an engagement target"),
           C->DetectedPlayer.Get());
  PC->Possess(Player);
  C->UpdatePerception();
  TestTrue(TEXT("Repossessed ship is acquired again"),
           C->DetectedPlayer == Player);
  const FVector RequestedDestination =
      C->bTargetB ? C->PatrolPointB : C->PatrolPointA;
  C->SetGeographicDestination(RequestedDestination);
  C->Role = EShipEnemyRole::Hunter;
  C->SelectObjective(C->PositionMetric());
  TestTrue(TEXT("Combat retains the explicitly requested final destination"),
           C->CurrentFinalDestination.Equals(RequestedDestination, 1.e-8));
  // Commit the temporary combat route first, so resumption must invalidate a
  // real stale route.
  TestTrue(TEXT("Temporary pursuit objective can be planned"),
           C->PlanRoute(C->PositionMetric(), C->GoalMetric));
  Player->SetActorLocation(FarPlayerLocation);
  C->UpdatePerception();
  C->LastPlanTime = -1000;
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("Target loss restores explicit destination instead of last "
                "pursuit goal"),
           C->GoalMetric.Equals(C->LLHToMetric(RequestedDestination), 1));
  TestTrue(TEXT("Target loss replans the restored explicit destination"),
           C->bPathfindingSucceeded && C->LastGoal.Equals(C->GoalMetric, 1) &&
               C->NavigationState == EShipNavigationState::FollowRoute);
  C->StartPatrol();
  FActorSpawnParameters ProjectileSpawn = Spawn;
  ProjectileSpawn.Owner = Ship;
  ProjectileSpawn.Instigator = Ship;
  auto *Projectile = W->SpawnActor<AShipEnemyProjectile>(
      AShipEnemyProjectile::StaticClass(), Ship->GetActorLocation(),
      FRotator::ZeroRotator, ProjectileSpawn);
  Projectile->LaunchVelocity = FVector(20000, 0, 0);
  Projectile->DispatchBeginPlay();
  TestTrue(TEXT("Projectile owner and instigator assigned"),
           Projectile->GetOwner() == Ship &&
               Projectile->GetInstigator() == Ship);
  TestTrue(TEXT("Projectile sweep excludes firing ship"),
           Projectile->Collision->GetMoveIgnoreActors().Contains(Ship));
  TestEqual(TEXT("Legacy projectile gravity preserved"),
            Projectile->Movement->ProjectileGravityScale, 1.f);
  TestEqual(TEXT("Legacy radial damage preserved"), Projectile->Damage,
            10000.f);
  // Actor-backed patrol uses only live transforms; these are existing actors
  // in this disposable fixture, not extra Target Points in MainMapLevel.
  C->Role = EShipEnemyRole::Passive;
  C->DetectedPlayer = nullptr;
  Second->SetActorLocation(C->MetricToWorld(FVector2D(-642.6, -1808.4)));
  Player->SetActorLocation(C->MetricToWorld(FVector2D(-60, -2350.7)));
  Ship->SetActorLocation(C->MetricToWorld(FVector2D(-262.7, -1945.5)));
  C->PatrolActorA = Second;
  C->PatrolActorB = Player;
  C->PatrolWaitSeconds = 0;
  C->StartPatrol();
  C->LastPlanTime = -1000;
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("Actor-backed patrol starts at A with a fresh route"),
           !C->bTargetB && C->bPathfindingSucceeded &&
               DestinationProperty->GetObjectPropertyValue_InContainer(Ship) ==
                   Second);
  const int32 ActorLegs = C->PatrolLegsCompleted;
  for (int32 I = 0; I < 3; ++I) {
    AActor *Active = C->bTargetB ? Player : Second;
    Ship->SetActorLocation(Active->GetActorLocation());
    C->SetDriveCommand(0, 0);
    C->UpdateNavigation(.1f);
    TestEqual(TEXT("Each settled arrival switches exactly one patrol leg"),
              C->PatrolLegsCompleted, ActorLegs + I + 1);
    C->LastPlanTime = -1000;
    C->UpdateNavigation(.1f);
    AActor *Next = C->bTargetB ? Player : Second;
    TestTrue(TEXT("Switch creates a fresh safe route to the exact other actor"),
             C->bPathfindingSucceeded && !C->CurrentRoute.IsEmpty() &&
                 FVector::Distance(
                     C->MetricToWorld(FVector2D(C->CurrentRoute.Last())),
                     Next->GetActorLocation()) < .01);
    C->UpdateNavigation(.1f);
    TestEqual(TEXT("No repeated arrival without physically changing position"),
              C->PatrolLegsCompleted, ActorLegs + I + 1);
  }
  AActor *ActivePatrol = C->bTargetB ? Player : Second;
  ActivePatrol->SetActorLocation(ActivePatrol->GetActorLocation() +
                                 FVector(1000, 0, 0));
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("Live patrol target motion invalidates old route while "
                "cooldown is pending"),
           C->CurrentRoute.IsEmpty() && C->DesiredThrottle == 0);
  C->LastPlanTime = -1000;
  C->UpdateNavigation(.1f);
  TestTrue(
      TEXT("Moved patrol actor is the new exact final destination"),
      C->bPathfindingSucceeded &&
          FVector::Distance(C->MetricToWorld(FVector2D(C->CurrentRoute.Last())),
                            ActivePatrol->GetActorLocation()) < .01);
  ActivePatrol->SetActorLocation(ActivePatrol->GetActorLocation() +
                                 FVector(0, 0, 1000));
  C->UpdateNavigation(.1f);
  C->LastPlanTime = -1000;
  C->UpdateNavigation(.1f);
  TestTrue(
      TEXT("Vertical actor movement also refreshes exact target height"),
      C->bPathfindingSucceeded &&
          FVector::Distance(C->GeographicToWorld(C->CurrentFinalDestination),
                            ActivePatrol->GetActorLocation()) < .01);
  C->PatrolActorA = nullptr;
  C->UpdateNavigation(.1f);
  TestTrue(
      TEXT("Missing patrol actor fails safely without geographic fallback"),
      C->NavigationState == EShipNavigationState::Failed &&
          C->DesiredThrottle == 0 && C->CurrentRoute.IsEmpty());
  C->PatrolActorB = nullptr;
  // Reproduce the real return-leg failure: valid water beside the island,
  // bow facing a blocked grid cell, and a safe stationary rotation envelope.
  // This checks command selection only; the separate PIE test proves motion.
  const FVector2D TurnPosition(-359.6633, -1122.0099);
  Ship->SetActorLocation(C->MetricToWorld(TurnPosition));
  C->Heading->SetWorldRotation(
      (C->MetricToWorld(TurnPosition + FVector2D(0, -100)) -
       C->Hull->GetComponentLocation())
          .Rotation());
  C->SetGeographicDestination(
      C->WorldToGeographic(C->MetricToWorld(FVector2D(-60, -2350.7))));
  C->LastPlanTime = -1000;
  C->SetDriveCommand(0, 0);
  // A very short bow ray must still brake forward motion, but may not suppress
  // a collision-validated stationary turn or poison the next A* start cell.
  for (int32 I : {0, 1, 11})
    Number->SetFloatingPointPropertyValue(Samples.GetRawPtr(I), 0);
  for (int32 I = 0; I < 140; ++I)
    C->UpdateNavigation(.1f);
  TestTrue(TEXT("Safe stopped turnaround steers without forward thrust"),
           C->NavigationState == EShipNavigationState::FollowRoute &&
               C->DesiredThrottle <= 0 && FMath::Abs(C->DesiredRudder) > .01);
  TestTrue(
      TEXT("Turnaround does not blacklist its own accepted starting point"),
      C->FailedCorridors.IsEmpty() && C->RecoveryAttemptCount == 0);
  TestTrue(TEXT("Return remains plannable after prolonged stationary turning"),
           C->Map
               ->FindPath(C->PositionMetric(), C->GoalMetric, 75000, 1000,
                          C->FailedCorridors)
               .bSuccess);
  C->Hull->SetPhysicsLinearVelocity(C->Heading->GetRightVector() * 150);
  C->UpdateNavigation(.1f);
  TestTrue(
      TEXT("Bounded lateral rudder drift retains the collision-validated turn"),
      C->NavigationState == EShipNavigationState::FollowRoute &&
          FMath::Abs(C->DesiredRudder) > .01 &&
          FMath::Abs(C->DesiredRudder) <= .6001f);
  C->Hull->SetPhysicsLinearVelocity(FVector::ZeroVector);
  AActor *TurnObstacle = W->SpawnActor<AActor>();
  auto *TurnShape = NewObject<UBoxComponent>(TurnObstacle);
  TurnObstacle->SetRootComponent(TurnShape);
  TurnObstacle->AddInstanceComponent(TurnShape);
  TurnShape->SetBoxExtent(FVector(500));
  TurnShape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
  TurnShape->SetCollisionResponseToAllChannels(ECR_Block);
  TurnShape->RegisterComponent();
  TurnObstacle->SetActorLocation(C->Hull->GetComponentLocation());
  for (int32 I = 0; I < 25; ++I)
    C->UpdateNavigation(.1f);
  TestTrue(
      TEXT(
          "A real obstruction inside the turning envelope suppresses steering"),
      C->DesiredThrottle <= 0 && C->DesiredRudder == 0);
  C->AvoidElapsed = 12.1f;
  C->TimeSinceProgress = C->StuckWindowSeconds + 1;
  C->UpdateNavigation(.1f);
  TestTrue(
      TEXT(
          "Close avoidance obstruction cannot exclude the ship's own A* start"),
      C->FailedCorridors.IsEmpty());
  TurnObstacle->Destroy();
  // Actual pawn collision in accepted open water must feed the same A*, so
  // steering does not repeatedly return to a straight line through the pawn.
  // The reused patrol marker is itself a physical BP_Ship. Move that old
  // fixture away before placing this test's ship at its former B location.
  Player->SetActorLocation(C->MetricToWorld(FVector2D(5000, -5000)));
  Ship->SetActorLocation(C->MetricToWorld(FVector2D(-60, -2350.7)));
  C->Heading->SetWorldRotation((C->MetricToWorld(FVector2D(110, -2350.7)) -
                                C->Hull->GetComponentLocation())
                                   .Rotation());
  for (int32 I = 0; I < 12; ++I)
    Number->SetFloatingPointPropertyValue(Samples.GetRawPtr(I), 80000);
  auto *PawnObstacle = W->SpawnActor<APawn>();
  auto *PawnShape = NewObject<UBoxComponent>(PawnObstacle);
  PawnObstacle->SetRootComponent(PawnShape);
  PawnObstacle->AddInstanceComponent(PawnShape);
  PawnShape->SetBoxExtent(FVector(2000, 2000, 3000));
  PawnShape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
  PawnShape->SetCollisionResponseToAllChannels(ECR_Block);
  PawnShape->RegisterComponent();
  PawnObstacle->SetActorLocation(C->MetricToWorld(FVector2D(110, -2350.7)));
  // The zero-time fixture cannot qualify an unpossessed hull as a stopped
  // traffic ship (>20 s). Exercise the immediate player-obstacle policy.
  PC->Possess(PawnObstacle);
  TestTrue(TEXT("Dynamic-obstacle fixture goal is confirmed accepted water"),
           C->Map->IsWater(FVector2D(600, -2350.7)));
  C->SetGeographicDestination(
      C->WorldToGeographic(C->MetricToWorld(FVector2D(600, -2350.7))));
  // Query-only registration is synchronous in NullRHI too. Do not depend on
  // the unticked fixture's deferred simulated-body creation or velocity.
  C->Hull->SetPhysicsLinearVelocity(FVector::ZeroVector);
  C->LastPlanTime = -1000;
  C->UpdateNavigation(.1f);
  AddInfo(FString::Printf(TEXT("Dynamic obstacle fixture: exclusions=%d "
                               "needsPlan=%d state=%s reason=%s"),
                          C->FailedCorridors.Num(), C->bNeedsPlan,
                          *UEnum::GetValueAsString(C->NavigationState),
                          *C->LastNavigationFailureReason));
  TestTrue(TEXT("Detected physical pawn requests an A* obstacle detour"),
           C->bNeedsPlan && C->FailedCorridors.Num() == 1 &&
               C->DesiredThrottle <= 0);
  C->Hull->SetPhysicsLinearVelocity(FVector::ZeroVector);
  C->LastPlanTime = -1000;
  C->UpdateNavigation(.1f);
  TestTrue(TEXT("The same native A* routes around the physical pawn exclusion"),
           C->bPathfindingSucceeded && C->CurrentRoute.Num() > 2);
  PawnObstacle->Destroy();
  C->Health = 20000;
  C->OnOwnerDamaged(Ship, 1000, nullptr, nullptr, Projectile);
  TestEqual(TEXT("Enemy rejects own projectile damage"), C->Health, 20000.f);
  C->OnOwnerDamaged(Ship, 30000, nullptr, PC, Player);
  TestTrue(TEXT("Enemy health death is terminal and neutral"),
           C->bDead && C->NavigationState == EShipNavigationState::Dead &&
               C->DesiredThrottle == 0);
  AddInfo(
      TEXT("Isolated world executes command/state/perception/ownership checks, "
           "NOT physical ship motion or final gameplay acceptance."));
  return true;
}
#endif
