#include "ShipEnemyNavigationComponent.h"
#include "SeaNavigationMap.h"
#include "Private/NavalWaterQueries.h"
#include "NavalBattleUI.h"
#include "ShipCombatComponent.h"
#include "ShipMovementComponent.h"
#include "Json.h"
#include "ShipEnemyProjectile.h"
#include "SeaTerrainCollisionComponent.h"
#include "CesiumGeoreference.h"
#include "CesiumGlobeAnchorComponent.h"
#include "CesiumEllipsoid.h"
#include "AIController.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

namespace
{
// Weak, per-component runtime state, removed on EndPlay. Changes to this type
// require a normal Editor rebuild/restart: Live Coding cannot reconstruct a
// retained TMap allocation when its element size changes.
struct FDestinationTracking
{
    TWeakObjectPtr<AActor> Actor;
    FVector SampleECEF=FVector::ZeroVector;
    FVector2D SnappedGoal=FVector2D::ZeroVector;
    FVector2D ApproachRequested=FVector2D::ZeroVector,ApproachResolved=FVector2D::ZeroVector;
    const FSeaNavigationMap* ApproachMap=nullptr;
    double ApproachRadius=0;
    bool bApproachCached=false;
    bool bExplicitCommand=false, bHasSnappedGoal=false;
    int32 PlanCount=0, ConsecutivePlanFailures=0;
    double RetryAt=0, LastTelemetry=-1;
    double RecoveryRetryAt=0;
    int32 RecoveryRetries=0;
    float AvoidAngle=45;
    TWeakObjectPtr<AActor> SweepActor;
    FVector SweepCenter=FVector::ZeroVector;
    FVector2D TrafficCenter=FVector2D::ZeroVector;
    FVector2D TrafficTravelDirection=FVector2D::ZeroVector;
    TWeakObjectPtr<APawn> TrafficActor;
    double TrafficRadius=0, TrafficUntil=0, TrafficCheckAt=0;
    double YieldSince=-1, PriorityUntil=0, PriorityAvailableAt=0;
    bool bYieldingToPlayer=false;
};
TMap<TWeakObjectPtr<UShipEnemyNavigationComponent>,FDestinationTracking> DestinationTracking;
FDestinationTracking& DestinationState(UShipEnemyNavigationComponent* Component)
{
    for(auto I=DestinationTracking.CreateIterator();I;++I)if(!I.Key().IsValid())I.RemoveCurrent();
    return DestinationTracking.FindOrAdd(Component);
}
bool HasShipRightOfWay(const UShipEnemyNavigationComponent* Component,APawn* Other,double Now)
{
    if(Other->IsPlayerControlled())return false;
    const auto* OtherNav=Other->FindComponentByClass<UShipEnemyNavigationComponent>();
    const auto* Own=DestinationTracking.Find(const_cast<UShipEnemyNavigationComponent*>(Component));
    const auto* Theirs=OtherNav?DestinationTracking.Find(const_cast<UShipEnemyNavigationComponent*>(OtherNav)):nullptr;
    const bool OwnPriority=Own && Own->PriorityUntil>Now,OtherPriority=Theirs && Theirs->PriorityUntil>Now;
    if(OwnPriority!=OtherPriority)return OwnPriority;
    return Component->GetOwner()->GetFName().LexicalLess(Other->GetFName());
}
AActor* DestinationActor(const UShipEnemyNavigationComponent* Component)
{
    if(Component->HasTacticalDestination())return nullptr;
    if(Component->NavigationMode==EShipNavigationMode::Patrol)
        if(const auto* State=DestinationTracking.Find(const_cast<UShipEnemyNavigationComponent*>(Component)))return State->Actor.Get();
    const auto* Property=FindFProperty<FObjectPropertyBase>(Component->GetOwner()->GetClass(),TEXT("DestinationActor"));
    return Property?Cast<AActor>(Property->GetObjectPropertyValue_InContainer(Component->GetOwner())):nullptr;
}
template<class T> T* NamedComponent(AActor* A,const TCHAR* Name)
{TArray<T*> Cs;A->GetComponents<T>(Cs);for(T* C:Cs)if(C->GetFName()==FName(Name))return C;return nullptr;}
FNumericProperty* Numeric(UObject* O,const TCHAR* N)
{return CastField<FNumericProperty>(O->GetClass()->FindPropertyByName(FName(N)));}
FVector2D Rotate2D(const FVector2D& V,double Degrees)
{const double A=FMath::DegreesToRadians(Degrees);return FVector2D(V.X*FMath::Cos(A)-V.Y*FMath::Sin(A),V.X*FMath::Sin(A)+V.Y*FMath::Cos(A));}

}

UShipEnemyNavigationComponent::UShipEnemyNavigationComponent()
{PrimaryComponentTick.bCanEverTick=false;ProjectileClass=AShipEnemyProjectile::StaticClass();}

void UShipEnemyNavigationComponent::BeginPlay()
{
    Super::BeginPlay();Health=FMath::Max(MaxHealth,1.f);
    PatrolReadyAt=GetWorld()->GetTimeSeconds()+FMath::Max(0.f,PatrolStartupDelaySeconds);
    GetOwner()->OnTakeAnyDamage.AddUniqueDynamic(this,&UShipEnemyNavigationComponent::OnOwnerDamaged);
    if(bNewNavigationEnabled && NavigationMode==EShipNavigationMode::Patrol && (PatrolActorA || PatrolActorB))
    {
        StartPatrol();
        // Plan from this run's physical spawn, never from a previous PIE route.
        UpdateNavigation(1.f/60.f);
    }
    // No force application and no actor transforms in this component.
}
void UShipEnemyNavigationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    DestinationTracking.Remove(this);
    if(IsValid(Georeference))Georeference->OnGeoreferenceUpdated.RemoveAll(this);
    if(GetOwner())GetOwner()->OnTakeAnyDamage.RemoveAll(this);
    if(OwnedAimController)OwnedAimController->Destroy();
    // Streaming can reactivate this same component after RemovedFromWorld.
    // The cached map and component bindings no longer exist after teardown.
    bInitialized=false;bOwnsCommands=false;bNeedsPlan=true;
    Map.Reset();OriginalCoastMap.Reset();bUsingOperationalChart=false;Hull=nullptr;Heading=nullptr;VisualMesh=nullptr;GlobeAnchor=nullptr;Georeference=nullptr;
    ThrottleProperty=nullptr;RudderProperty=nullptr;OwnedAimController=nullptr;
    SensorDistances.Reset();SensorAngles.Reset();bHasObservedPosition=false;
    Super::EndPlay(Reason);
}
bool UShipEnemyNavigationComponent::InitializeNavigation()
{
    bInitialized=false;
    if(IsValid(Georeference))Georeference->OnGeoreferenceUpdated.RemoveAll(this);
    AActor* A=GetOwner();Hull=NamedComponent<UPrimitiveComponent>(A,TEXT("ShipSM"));Heading=NamedComponent<USceneComponent>(A,TEXT("Arrow"));
    VisualMesh=NamedComponent<USkeletalMeshComponent>(A,TEXT("Ship"));
    GlobeAnchor=A->FindComponentByClass<UCesiumGlobeAnchorComponent>();
    ThrottleProperty=Numeric(A,TEXT("AIThrottle"));RudderProperty=Numeric(A,TEXT("AIRudder"));
    if(!Hull || !Heading || !GlobeAnchor || !ThrottleProperty || !RudderProperty || !ThrottleProperty->IsFloatingPoint() || !RudderProperty->IsFloatingPoint())
    {Fail(TEXT("Missing verified ShipSM/Arrow/anchor or numeric AI command properties"));return false;}
    Georeference=GlobeAnchor->ResolveGeoreference();
    if(!Georeference){Fail(TEXT("Existing globe anchor has no georeference"));return false;}
    FString Error;Map=FSeaNavigationMap::Load(FPaths::ProjectContentDir()/NavigationMapFile,Error);
    if(!Map){Fail(Error);return false;}
    const FBox LocalBox=Hull->GetLocalBounds().GetBox();
    const FVector Min=LocalBox.Min,Max=LocalBox.Max;
    const FVector Scale=Hull->GetComponentScale().GetAbs();
    HullRadiusM=FVector2D(FMath::Max(FMath::Abs(Min.X),FMath::Abs(Max.X))*Scale.X,FMath::Max(FMath::Abs(Min.Y),FMath::Abs(Max.Y))*Scale.Y).Size()/100.;
    HullHalfWidthM=FMath::Max(FMath::Abs(Min.Y),FMath::Abs(Max.Y))*Scale.Y/100.;
    if(HullRadiusM>Map->MaxVesselRadius+0.01){Fail(TEXT("Hull exceeds map's validated vessel radius; rebuild navigation data with larger clearance"));return false;}
    Georeference->OnGeoreferenceUpdated.AddUniqueDynamic(this,&UShipEnemyNavigationComponent::OnGeoreferenceChanged);
    LastWorldOrigin=GetWorld()->OriginLocation;ProgressPosition=PositionMetric();BestRemainingDistance=DBL_MAX;
    bInitialized=true;return true;
}
FVector UShipEnemyNavigationComponent::WorldToECEF(const FVector& P) const
{
    // Installed Cesium 2.30 uses the anchored root's PARENT frame, not arbitrary world positions.
    const USceneComponent* Root=GetOwner()->GetRootComponent();
    const FVector Local=Root && Root->GetAttachParent()?Root->GetAttachParent()->GetComponentTransform().InverseTransformPosition(P):P+FVector(GetWorld()->OriginLocation);
    return Georeference->TransformUnrealPositionToEarthCenteredEarthFixed(Local);
}
FVector UShipEnemyNavigationComponent::MetricToWorld(const FVector2D& P) const
{
    // ENU is a horizontal projection, not an invertible 3D position. Preserve
    // the exact requested actor height for the final approach and its marker.
    if(bHasGoal && P.Equals(LLHToMetric(CurrentFinalDestination),1.e-5))
        return GeographicToWorld(CurrentFinalDestination);
    FVector Local=Georeference->TransformEarthCenteredEarthFixedPositionToUnreal(Map->ToECEF(P));
    const USceneComponent* Root=GetOwner()->GetRootComponent();
    return Root && Root->GetAttachParent()?Root->GetAttachParent()->GetComponentTransform().TransformPosition(Local):Local-FVector(GetWorld()->OriginLocation);
}
FVector UShipEnemyNavigationComponent::GeographicToWorld(FVector LLH) const
{
    if(!Georeference)return FVector::ZeroVector;
    FVector P=Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(LLH);
    const USceneComponent* Root=GetOwner()->GetRootComponent();
    return Root && Root->GetAttachParent()?Root->GetAttachParent()->GetComponentTransform().TransformPosition(P):P-FVector(GetWorld()->OriginLocation);
}
FVector UShipEnemyNavigationComponent::WorldToGeographic(FVector P) const
{return Georeference?Georeference->GetEllipsoid()->EllipsoidCenteredEllipsoidFixedToLongitudeLatitudeHeight(WorldToECEF(P)):FVector::ZeroVector;}
FVector2D UShipEnemyNavigationComponent::PositionMetric() const
{return Map->ToMetric(WorldToECEF(Hull->GetComponentLocation()));}
FVector2D UShipEnemyNavigationComponent::LLHToMetric(const FVector& P) const
{return Map->ToMetric(Georeference->GetEllipsoid()->LongitudeLatitudeHeightToEllipsoidCenteredEllipsoidFixed(P));}
void UShipEnemyNavigationComponent::OnGeoreferenceChanged()
{bOriginChanged=true;SensorDistances.Reset();}
void UShipEnemyNavigationComponent::Fail(const FString& S)
{
    if(bDrawDebug && LastNavigationFailureReason!=S)UE_LOG(LogTemp,Warning,TEXT("ShipNavigation %s: %s"),*GetNameSafe(GetOwner()),*S);
    LastNavigationFailureReason=S;NavigationState=EShipNavigationState::Failed;bPathfindingSucceeded=false;DesiredSpeed=0;
    DestinationState(this).RecoveryRetryAt=0;
    // A moving hull can exhaust local recovery without invalidating this water
    // route. Leave commands stopped during backoff, then let the same planner
    // and traffic arbitration reassess it. Configuration/land failures stay hard.
    if(S==TEXT("Bounded recovery attempts exhausted") || S==TEXT("Local avoidance/replan budget exhausted") ||
       S==TEXT("Recovery could not stabilize velocity within its time budget"))
        DestinationState(this).RecoveryRetryAt=GetWorld()->GetTimeSeconds()+FMath::Max(30.f,StuckWindowSeconds*2);
}
void UShipEnemyNavigationComponent::SetDriveCommand(float T,float R)
{
    if(!bOwnsCommands)return;
    DesiredThrottle=FMath::IsFinite(T)?FMath::Clamp(T,-1.f,1.f):0;
    DesiredRudder=FMath::IsFinite(R)?FMath::Clamp(R,-1.f,1.f):0;
    if(ThrottleProperty)ThrottleProperty->SetFloatingPointPropertyValue(ThrottleProperty->ContainerPtrToValuePtr<void>(GetOwner()),DesiredThrottle);
    if(RudderProperty)RudderProperty->SetFloatingPointPropertyValue(RudderProperty->ContainerPtrToValuePtr<void>(GetOwner()),DesiredRudder);
    // Draw at the command boundary so early failure/arrival/stop returns stay visible.
    if(bDrawDebug && GetWorld() && GetOwner())
    {
        const FColor Color=NavigationState==EShipNavigationState::Failed?FColor::Red:bAvoidingObstacle?FColor::Orange:FColor::Green;
        const AActor* ActiveTarget=DestinationActor(this);
        const double DisplayDistance=Map && IsValid(Georeference) && IsValid(Hull) && IsValid(ActiveTarget)?
            FVector2D::Distance(PositionMetric(),Map->ToMetric(WorldToECEF(ActiveTarget->GetActorLocation()))):-1.;
        const double DisplaySpeed=IsValid(Hull)?FVector::VectorPlaneProject(Hull->GetPhysicsLinearVelocity(),Hull->GetUpVector()).Size()/100.:0.;
        const FString Label=FString::Printf(TEXT("%s | %s | target %s\nWaypoint %d/%d | distance %.1f m | speed %.1f m/s | throttle %.2f rudder %.2f\nPlans %d | completed legs %d\n%s"),
            *GetOwner()->GetActorNameOrLabel(),*UEnum::GetValueAsString(NavigationState),*GetNameSafe(DestinationActor(this)),CurrentWaypointIndex,CurrentRoute.Num()-1,
            DisplayDistance,DisplaySpeed,DesiredThrottle,DesiredRudder,DestinationState(this).PlanCount,PatrolLegsCompleted,*LastNavigationFailureReason);
        DrawDebugString(GetWorld(),FVector(0,0,1200),Label,GetOwner(),Color,0.f,true,1.2f);
        if(Map && IsValid(Georeference) && IsValid(Hull))
        {
            auto& Track=DestinationState(this);
            const double Now=GetWorld()->GetTimeSeconds();
            if(Now-Track.LastTelemetry>=1.)
            {
                Track.LastTelemetry=Now;
                const AActor* Target=DestinationActor(this);
                const double Distance=IsValid(Target)?FVector2D::Distance(PositionMetric(),Map->ToMetric(WorldToECEF(Target->GetActorLocation()))):-1.;
                UE_LOG(LogTemp,Display,TEXT("ShipNavigation TELEMETRY t=%.2f target=%s state=%s waypoint=%d distance=%.3fm speed=%.3fm/s throttle=%.3f rudder=%.3f plans=%d legs=%d world=%s llh=%s"),
                    Now,*GetNameSafe(Target),*UEnum::GetValueAsString(NavigationState),CurrentWaypointIndex,Distance,Hull->GetPhysicsLinearVelocity().Size()/100.,DesiredThrottle,DesiredRudder,
                    Track.PlanCount,PatrolLegsCompleted,*Hull->GetComponentLocation().ToString(),*WorldToGeographic(Hull->GetComponentLocation()).ToString());
            }
            for(const AActor* Target:{PatrolActorA.Get(),PatrolActorB.Get()})if(IsValid(Target))
            {
                const FColor Marker=Target==PatrolActorA.Get()?FColor::Blue:FColor::Orange;
                DrawDebugSphere(GetWorld(),Target->GetActorLocation(),2200,16,Marker,false,0.1,0,8);
                DrawDebugString(GetWorld(),Target->GetActorLocation()+FVector(0,0,3500),Target->GetActorNameOrLabel(),nullptr,Marker,0.f,true,1.2f);
            }
            auto Point=[&](int32 I){return MetricToWorld(FVector2D(CurrentRoute[I].X,CurrentRoute[I].Y));};
            for(int32 I=1;I<CurrentRoute.Num();++I)DrawDebugLine(GetWorld(),Point(I-1),Point(I),FColor::Green,false,0.1,0,15);
            if(CurrentRoute.IsValidIndex(CurrentWaypointIndex))
            {
                const FVector Waypoint=Point(CurrentWaypointIndex);
                DrawDebugSphere(GetWorld(),Waypoint,1000,12,FColor::Yellow,false,0.1,0,8);
                DrawDebugDirectionalArrow(GetWorld(),Hull->GetComponentLocation(),Waypoint,500,Color,false,0.1,0,10);
                const double LookAhead=FMath::Clamp(Hull->GetPhysicsLinearVelocity().Size()/100.*12.,80.,300.);
                DrawDebugCircle(GetWorld(),Waypoint,LookAhead*100,32,FColor::Yellow,false,0.1,0,3,FVector::ForwardVector,FVector::RightVector,false);
            }
            if(bHasGoal)
            {
                const AActor* Target=DestinationActor(this);
                const FVector Requested=IsValid(Target) && !Track.bExplicitCommand?Target->GetActorLocation():GeographicToWorld(CurrentFinalDestination);
                DrawDebugSphere(GetWorld(),Requested,1800,16,FColor::Red,false,0.1,0,8);
                const FVector Goal=MetricToWorld(Track.SnappedGoal);
                if(Track.bHasSnappedGoal)
                {
                    DrawDebugSphere(GetWorld(),Goal,1100,16,FColor::Green,false,0.1,0,8);
                    DrawDebugLine(GetWorld(),Requested,Goal,FColor::Green,false,0.1,0,12);
                }
                const FVector Final=CurrentRoute.IsEmpty()?GeographicToWorld(CurrentFinalDestination):Point(CurrentRoute.Num()-1);
                if(bPathfindingSucceeded && !CurrentRoute.IsEmpty())
                {
                    DrawDebugSphere(GetWorld(),Final,1500,16,FColor::Magenta,false,0.1,0,8);
                    DrawDebugCircle(GetWorld(),Final,ArrivalRadiusM*100,32,FColor::Magenta,false,0.1,0,4,FVector::ForwardVector,FVector::RightVector,false);
                }
                const FString Details=FString::Printf(TEXT("Target %s | A* %s\nRED requested %s\nGREEN grid %s | snap %.3f m\nMAGENTA final %s | endpoint error %.6f m\nShip to requested %.2f m"),
                    *GetNameSafe(Target),bPathfindingSucceeded?TEXT("SUCCESS"):TEXT("NO VALID PATH"),*Requested.ToString(),
                    Track.bHasSnappedGoal?*Goal.ToString():TEXT("none"),Track.bHasSnappedGoal?FVector::Distance(Goal,Requested)/100.:0.,
                    bPathfindingSucceeded?*Final.ToString():TEXT("none"),bPathfindingSucceeded?FVector::Distance(Final,Requested)/100.:0.,FVector::Dist2D(Hull->GetComponentLocation(),Requested)/100.);
                DrawDebugString(GetWorld(),Requested+FVector(0,0,2500),Details,nullptr,FColor::White,0.f,true,1.1f);
            }
            if(bDrawLandGrid)
            {const int32 C=Map->CellAt(PositionMetric());if(C!=INDEX_NONE)for(int32 Y=-10;Y<=10;++Y)for(int32 X=-10;X<=10;++X){const int32 CX=C%Map->Width+X,CY=C/Map->Width+Y;if(!Map->IsWater(CX,CY) && CX>=0 && CY>=0 && CX<Map->Width && CY<Map->Height)DrawDebugBox(GetWorld(),MetricToWorld(Map->Center(CY*Map->Width+CX)),FVector(Map->CellSize*45,Map->CellSize*45,100),FColor::Red,false,0.1);}}
        }
    }
}
void UShipEnemyNavigationComponent::SetGeographicDestination(FVector LLH)
{
    DestinationState(this).bExplicitCommand=true;
    CurrentFinalDestination=LLH;NavigationMode=EShipNavigationMode::Destination;bHasGoal=true;bExplicitStop=false;
    bNeedsPlan=true;LastGoal=FVector2D(DBL_MAX,DBL_MAX);CurrentRoute.Reset();RecoveryAttemptCount=0;RecoveryPhase=0;
    NavigationState=EShipNavigationState::Replan;LastNavigationFailureReason.Empty();
}
void UShipEnemyNavigationComponent::StartPatrol()
{
    bBackwardRetreatActive=false;ReverseThreat.Reset();
    bTacticalOverride=false;
    NavigationMode=EShipNavigationMode::Patrol;bHasGoal=false;bNeedsPlan=true;bExplicitStop=false;
    if(PatrolActorA || PatrolActorB)bTargetB=bStartAtPatrolB;
    PatrolCircuitIndex=PatrolCircuitActors.IsEmpty()?0:FMath::Clamp(PatrolCircuitIndex,0,PatrolCircuitActors.Num()-1);
    CurrentRoute.Reset();bPathfindingSucceeded=false;ArrivalTime=-1;
    RecoveryAttemptCount=0;RecoveryPhase=0;FailedCorridors.Reset();
    AvoidElapsed=0;AvoidSide=0;LastAvoidSide=0;AvoidSideChanges=0;
    DestinationState(this)=FDestinationTracking();NavigationState=EShipNavigationState::Patrol;
}
bool UShipEnemyNavigationComponent::PrepareAuthoredPatrol(FString& OutError,bool bValidateRoutes)
{
    OutError.Empty();
    if(!GetWorld() || !IsValid(GetOwner()))
    {OutError=TEXT("Authored patrol requires a ship in a live world");return false;}
    bNewNavigationEnabled=true;bKeepAuthoredPatrol=true;bOwnsCommands=true;
    auto Reject=[&](const FString& Reason)
    {
        OutError=FString::Printf(TEXT("%s: %s"),*GetOwner()->GetActorNameOrLabel(),*Reason);
        CurrentRoute.Reset();bHasGoal=false;bNeedsPlan=false;Fail(OutError);SetDriveCommand(0,0);return false;
    };
    if(bDead || (GetOwner()->FindComponentByClass<UShipWeaponComponent>() && GetOwner()->FindComponentByClass<UShipWeaponComponent>()->bIsDestroyed))
        return Reject(TEXT("a destroyed ship cannot start an authored patrol"));
    // Keep a prepared operational chart. Reinitialization otherwise reloads the
    // coarse coast map and would silently discard the field's physical survey.
    if(!bInitialized || !Map.IsValid() || !IsValid(Hull) || !IsValid(Heading) || !IsValid(GlobeAnchor) ||
       !IsValid(Georeference) || GlobeAnchor->ResolveGeoreference()!=Georeference)
        if(!InitializeNavigation())return Reject(LastNavigationFailureReason);
    bStartAtPatrolB=false;NavigationMode=EShipNavigationMode::Patrol;
    PatrolCircuitActors.Reset();PatrolCircuitIndex=0;PatrolOffsetA=PatrolOffsetB=FVector::ZeroVector;
    StartPatrol();NotifyDevelopmentReposition();
    if(!IsValid(PatrolActorA) || !IsValid(PatrolActorB) || PatrolActorA==PatrolActorB ||
       PatrolActorA->GetWorld()!=GetWorld() || PatrolActorB->GetWorld()!=GetWorld())
        return Reject(FString::Printf(TEXT("assign two distinct same-world patrol markers (A=%s, B=%s)"),*GetNameSafe(PatrolActorA),*GetNameSafe(PatrolActorB)));
    const FVector2D Start=PositionMetric();
    const FVector2D A=Map->ToMetric(WorldToECEF(PatrolActorA->GetActorLocation()));
    const FVector2D B=Map->ToMetric(WorldToECEF(PatrolActorB->GetActorLocation()));
    if(FVector2D::Distance(A,B)<=ArrivalRadiusM*2)
        return Reject(FString::Printf(TEXT("%s and %s have overlapping arrival zones; separate them by more than %.0f m"),*GetNameSafe(PatrolActorA),*GetNameSafe(PatrolActorB),ArrivalRadiusM*2));
    if(bValidateRoutes)
    {
        FVector2D SafeA,SafeB;
        if(!Map->ResolveArrivalGoal(A,ArrivalRadiusM,Start,SafeA) || !Map->ResolveArrivalGoal(B,ArrivalRadiusM,Start,SafeB))
            return Reject(TEXT("no safe water approach inside an authored A/B arrival zone; check marker clearance for this hull"));
        auto ValidateLeg=[&](const FVector2D& From,const FVector2D& To,const FString& Label)
        {
            const FSeaRouteResult Route=Map->FindPath(From,To,MaxSearchExpansions,MaxSearchMilliseconds);
            return Route.bSuccess || Reject(FString::Printf(TEXT("authored %s route rejected: %s"),*Label,*Route.Failure));
        };
        if(!ValidateLeg(Start,SafeA,FString::Printf(TEXT("start -> A (%s)"),*PatrolActorA->GetActorNameOrLabel())) ||
           !ValidateLeg(SafeA,SafeB,FString::Printf(TEXT("A (%s) -> B (%s)"),*PatrolActorA->GetActorNameOrLabel(),*PatrolActorB->GetActorNameOrLabel())) ||
           !ValidateLeg(SafeB,SafeA,FString::Printf(TEXT("B (%s) -> A (%s)"),*PatrolActorB->GetActorNameOrLabel(),*PatrolActorA->GetActorNameOrLabel())))return false;
    }
    PatrolLegsCompleted=0;CircuitLegsCompleted=0;PatrolCircuitsCompleted=0;
    SelectObjective(Start);SetDriveCommand(0,0);return true;
}
void UShipEnemyNavigationComponent::RequestReplan()
{
    bExplicitStop=false;
    NotifyDevelopmentReposition();
}
void UShipEnemyNavigationComponent::NotifyDevelopmentReposition()
{
    // Keep the selected endpoint and explicit geographic/tactical objective.
    // A new physical start invalidates every derived steering/recovery sample.
    ++RepositionReinitializations;bNeedsPlan=true;CurrentRoute.Reset();CurrentWaypointIndex=0;bPathfindingSucceeded=false;
    bBackwardRetreatActive=false;bBackwardRetreatBlocked=false;bBackwardRetreatCompleted=false;ReverseThreat.Reset();
    ArrivalTime=-1;LastPlanTime=-1000;RecoveryAttemptCount=0;RecoveryPhase=0;
    FailedCorridors.Reset();AvoidElapsed=0;AvoidSide=0;LastAvoidSide=0;AvoidSideChanges=0;
    TimeSinceProgress=0;SuccessfulProgressSinceRecovery=0;BestRemainingDistance=DBL_MAX;bStuck=false;
    SensorDistances.Reset();SensorAngles.Reset();bHasObservedPosition=false;
    auto& Track=DestinationState(this);
    Track.bHasSnappedGoal=false;Track.ConsecutivePlanFailures=0;Track.RetryAt=0;
    Track.TrafficRadius=0;Track.YieldSince=-1;Track.PriorityUntil=0;
    bYieldingToShip=false;TrafficWaitSeconds=0;
    LastNavigationFailureReason.Empty();NavigationState=bExplicitStop?EShipNavigationState::Idle:EShipNavigationState::Replan;
    SetDriveCommand(0,0);
}
FString UShipEnemyNavigationComponent::GetNavigationDiagnostics() const
{
    auto J=MakeShared<FJsonObject>();
    J->SetBoolField(TEXT("operational_chart"),bUsingOperationalChart);
    J->SetBoolField(TEXT("keep_authored_patrol"),bKeepAuthoredPatrol);
    if(Map.IsValid())J->SetNumberField(TEXT("navigation_cell_size_m"),Map->CellSize);
    auto Vector=[&](const TCHAR* Key,const FVector& V)
    {J->SetArrayField(Key,{MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)});};
    J->SetStringField(TEXT("actor"),GetNameSafe(GetOwner()));
    J->SetStringField(TEXT("label"),GetOwner()->GetActorNameOrLabel());
    const auto* Pawn=Cast<APawn>(GetOwner());
    J->SetStringField(TEXT("controller"),Pawn?GetNameSafe(Pawn->GetController()):TEXT("none"));
    const auto* Combat=GetOwner()->FindComponentByClass<UShipCombatComponent>();
    const auto* Movement=GetOwner()->FindComponentByClass<UShipMovementComponent>();
    J->SetNumberField(TEXT("team"),Combat?Combat->Faction:-1);
    J->SetStringField(TEXT("class"),Movement?UEnum::GetValueAsString(Movement->HullClass):TEXT("unknown"));
    J->SetStringField(TEXT("tactical_state"),Combat?UEnum::GetValueAsString(Combat->TacticalState):TEXT("none"));
    J->SetStringField(TEXT("navigation_state"),UEnum::GetValueAsString(NavigationState));
    J->SetStringField(TEXT("patrol_a"),GetNameSafe(PatrolActorA));
    J->SetStringField(TEXT("patrol_b"),GetNameSafe(PatrolActorB));
    J->SetStringField(TEXT("active_target"),GetNameSafe(DestinationActor(this)));
    J->SetNumberField(TEXT("legs"),PatrolLegsCompleted);
    J->SetNumberField(TEXT("circuit_targets"),PatrolCircuitActors.Num());J->SetNumberField(TEXT("circuit_index"),PatrolCircuitIndex);
    J->SetNumberField(TEXT("circuits_completed"),PatrolCircuitsCompleted);
    J->SetNumberField(TEXT("waypoint_index"),CurrentWaypointIndex);
    J->SetNumberField(TEXT("waypoint_count"),CurrentRoute.Num());
    J->SetBoolField(TEXT("path_success"),bPathfindingSucceeded);
    J->SetStringField(TEXT("failure"),LastNavigationFailureReason);
    J->SetNumberField(TEXT("throttle"),DesiredThrottle);J->SetNumberField(TEXT("rudder"),DesiredRudder);
    J->SetNumberField(TEXT("time_without_route_progress_s"),TimeSinceProgress);
    J->SetNumberField(TEXT("recovery_attempts"),RecoveryAttemptCount);
    J->SetNumberField(TEXT("reposition_reinitializations"),RepositionReinitializations);
    J->SetBoolField(TEXT("explicit_stop"),bExplicitStop);
    J->SetBoolField(TEXT("can_act"),UNavalBattleSubsystem::CanShipAct(GetOwner()));
    if(Movement)
    {
        Vector(TEXT("blueprint_propulsion_force"),Movement->LastPropulsionForce);
        Vector(TEXT("blueprint_rudder_force_local"),Movement->LastRudderForce);
    }
    Vector(TEXT("goal_llh"),CurrentFinalDestination);
    if(Map && IsValid(Hull) && IsValid(Georeference))
    {
        const auto P=PositionMetric();Vector(TEXT("chart_en_m"),FVector(P,0));
        J->SetNumberField(TEXT("distance_to_goal_m"),FVector2D::Distance(P,LLHToMetric(CurrentFinalDestination)));
        J->SetNumberField(TEXT("distance_to_approach_m"),FVector2D::Distance(P,GoalMetric));
        Vector(TEXT("approach_en_m"),FVector(GoalMetric,0));
        J->SetBoolField(TEXT("authored_standoff"),bKeepAuthoredPatrol && !GoalMetric.Equals(LLHToMetric(CurrentFinalDestination),1));
        J->SetBoolField(TEXT("chart_water"),Map->IsWater(P));
        J->SetBoolField(TEXT("physics"),Hull->IsSimulatingPhysics());
        J->SetNumberField(TEXT("speed_mps"),Hull->GetPhysicsLinearVelocity().Size()/100);
    }
    if(const auto* Track=DestinationTracking.Find(const_cast<UShipEnemyNavigationComponent*>(this)))
    {
        J->SetNumberField(TEXT("plans"),Track->PlanCount);
        J->SetNumberField(TEXT("recovery_retries"),Track->RecoveryRetries);
        J->SetNumberField(TEXT("recovery_retry_at"),Track->RecoveryRetryAt);
        J->SetStringField(TEXT("last_sweep_actor"),GetNameSafe(Track->SweepActor.Get()));
    }
    FString Out;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Out));return Out;
}
void UShipEnemyNavigationComponent::RefreshHullGeometry()
{bInitialized=false;bNeedsPlan=true;}
void UShipEnemyNavigationComponent::StopNavigation()
{bBackwardRetreatActive=false;ReverseThreat.Reset();bHasGoal=false;bNeedsPlan=false;bExplicitStop=true;CurrentRoute.Reset();NavigationState=EShipNavigationState::Idle;DesiredSpeed=0;}
bool UShipEnemyNavigationComponent::RearPathClear(float DistanceM) const
{
    if(!Map.IsValid() || !Hull || !Heading)return false;
    const FVector Start=Hull->GetComponentLocation(),End=Start-Heading->GetForwardVector()*DistanceM*100;
    const FVector2D P=PositionMetric(),Back=Map->ToMetric(WorldToECEF(End));
    const FVector2D D=(Back-P).GetSafeNormal(),Side(-D.Y,D.X);
    const float Width=HullHalfWidthM+10;
    for(float Sign:{-1.f,0.f,1.f})if(!Map->SegmentClear(P+Side*Width*Sign,Back+Side*Width*Sign))return false;
    if(!SweepClear(Start,End,Width))return false;
    for(TActorIterator<APawn> Other(GetWorld());Other;++Other)
    {
        if(*Other==GetOwner() || Other->IsActorBeingDestroyed())continue;
        if(auto* OtherHull=UShipCombatComponent::PhysicalHull(*Other))
            if(FMath::PointDistToSegment(OtherHull->Bounds.Origin,Start,End)<(HullRadiusM+OtherHull->Bounds.SphereRadius/100+10)*100)return false;
    }
    return true;
}
bool UShipEnemyNavigationComponent::WaterLineClear(const FVector& Start,const FVector& End) const
{
    const auto Coast=OriginalCoastMap.IsValid()?OriginalCoastMap:Map;
    return Coast.IsValid() && Coast->SegmentClear(Coast->ToMetric(WorldToECEF(Start)),Coast->ToMetric(WorldToECEF(End)));
}
bool UShipEnemyNavigationComponent::UseOperationalChart(TSharedPtr<const FSeaNavigationMap> Chart)
{
    if(!Chart.IsValid() || (!bInitialized && !InitializeNavigation()))return false;
    if(!OriginalCoastMap.IsValid())OriginalCoastMap=Map;
    if(!OriginalCoastMap.IsValid() || Chart->OriginECEF!=OriginalCoastMap->OriginECEF)return false;
    Map=MoveTemp(Chart);bUsingOperationalChart=true;NotifyDevelopmentReposition();return true;
}
bool UShipEnemyNavigationComponent::BeginBackwardRetreat(AActor* Threat,float DistanceM)
{
    if(!IsValid(Threat) || (!bInitialized && !InitializeNavigation()) || bDead || bExplicitStop)return false;
    auto* ThreatHull=UShipCombatComponent::PhysicalHull(Threat);
    const FVector Toward=(ThreatHull?ThreatHull->GetComponentLocation():Threat->GetActorLocation())-Hull->GetComponentLocation();
    if(FVector::DotProduct(Heading->GetForwardVector(),Toward.GetSafeNormal())<.42)return false;
    const float Distance=FMath::Clamp(DistanceM,80.f,500.f);
    if(!RearPathClear(Distance))return false;
    ReverseThreat=Threat;ReverseStart=PositionMetric();ReverseDistanceM=Distance;
    bBackwardRetreatActive=true;bBackwardRetreatBlocked=false;bBackwardRetreatCompleted=false;
    bTacticalOverride=true;bNeedsPlan=false;RecoveryPhase=0;CurrentRoute.Reset();bHasGoal=true;
    const FVector End=Hull->GetComponentLocation()-Heading->GetForwardVector()*Distance*100;
    TacticalDestination=WorldToGeographic(End);CurrentFinalDestination=TacticalDestination;GoalMetric=LLHToMetric(TacticalDestination);
    return true;
}
bool UShipEnemyNavigationComponent::UpdateBackwardRetreat(float Dt)
{
    const FVector Forward=Heading->GetForwardVector(),Up=Hull->GetUpVector();
    const float Speed=FVector::DotProduct(Hull->GetPhysicsLinearVelocity(),Forward)/100;
    const float Travel=FVector2D::Distance(PositionMetric(),ReverseStart);
    const bool Completed=Travel>=ReverseDistanceM;
    if(!ReverseThreat.IsValid() || Completed || !RearPathClear(FMath::Max(50.f,FMath::Abs(Speed)*6)))
    {
        bBackwardRetreatActive=false;bBackwardRetreatCompleted=Completed;bBackwardRetreatBlocked=!Completed && ReverseThreat.IsValid();
        SetDriveCommand(Speed<-.5f?.12f:Speed>.5f?-.12f:0,0);return true;
    }
    auto* ThreatHull=UShipCombatComponent::PhysicalHull(ReverseThreat.Get());
    const FVector Toward=(ThreatHull?ThreatHull->GetComponentLocation():ReverseThreat->GetActorLocation())-Hull->GetComponentLocation();
    const FVector F=FVector::VectorPlaneProject(Forward,Up).GetSafeNormal(),D=FVector::VectorPlaneProject(Toward,Up).GetSafeNormal();
    const float Error=FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Up,FVector::CrossProduct(F,D)),FVector::DotProduct(F,D)));
    const float YawRate=FVector::DotProduct(Hull->GetPhysicsAngularVelocityInDegrees(),Up);
    // Existing local side-force rudder produces the same torque sign in either
    // direction. Do not invert it simply because surge velocity is negative.
    const float Rudder=FMath::Clamp(RudderSign*Error/45-YawRateDamping*YawRate,-.65f,.65f);
    const float Target=-FMath::Clamp(BackwardRetreatSpeedMps,1.f,7.f);
    float Throttle=FMath::Clamp((Target-Speed)*.06f,-.3f,.12f);
    if(FMath::Abs(Error)>75)Throttle=Speed<-.5f?.12f:0;
    DesiredSpeed=Target;NavigationState=EShipNavigationState::Flee;
    SetDriveCommand(FMath::FInterpConstantTo(DesiredThrottle,Throttle,Dt,ThrottleRatePerSecond),FMath::FInterpConstantTo(DesiredRudder,Rudder,Dt,RudderRatePerSecond));
    return true;
}
bool UShipEnemyNavigationComponent::BeginRetreatFrom(AActor* Threat,float Distance)
{
    bBackwardRetreatActive=false;bBackwardRetreatBlocked=false;bBackwardRetreatCompleted=false;ReverseThreat.Reset();
    if(!IsValid(Threat) || (!bInitialized && !InitializeNavigation()) || bDead)return false;
    const FVector2D P=PositionMetric();
    UPrimitiveComponent* ThreatHull=UShipCombatComponent::PhysicalHull(Threat);
    const FVector2D Enemy=Map->ToMetric(WorldToECEF(ThreatHull?ThreatHull->GetComponentLocation():Threat->GetActorLocation()));
    const FVector2D Away=(P-Enemy).GetSafeNormal();
    const double Radius=FMath::Clamp(double(Distance),300.,2000.);
    int32 Searches=0;
    for(double Angle:{0.,30.,-30.,60.,-60.,90.,-90.,120.,-120.})
    {
        const FVector2D Candidate=P+Rotate2D(Away.IsNearlyZero()?FVector2D(1,0):Away,Angle)*Radius;
        if(!Map->SameWaterRegion(P,Candidate) || FVector2D::Distance(Candidate,Enemy)<FVector2D::Distance(P,Enemy)+100)continue;
        const auto Route=Map->FindPath(P,Candidate,MaxSearchExpansions,MaxSearchMilliseconds);
        if(!Route.bSuccess){if(++Searches>=4)break;continue;}
        TacticalDestination=Georeference->GetEllipsoid()->EllipsoidCenteredEllipsoidFixedToLongitudeLatitudeHeight(Map->ToECEF(Candidate));
        bTacticalOverride=true;CurrentFinalDestination=TacticalDestination;GoalMetric=Candidate;bHasGoal=true;bNeedsPlan=true;bExplicitStop=false;
        CurrentRoute.Reset();ArrivalTime=-1;FailedCorridors.Reset();RecoveryPhase=0;RecoveryAttemptCount=0;AvoidSide=0;AvoidElapsed=0;TimeSinceProgress=0;
        DestinationState(this).ConsecutivePlanFailures=0;
        return PlanRoute(P,Candidate);
    }
    return false;
}
bool UShipEnemyNavigationComponent::BeginPursuitOf(AActor* Threat,float StandOffDistanceM)
{
    bBackwardRetreatActive=false;bBackwardRetreatBlocked=false;bBackwardRetreatCompleted=false;ReverseThreat.Reset();
    if(!IsValid(Threat) || (!bInitialized && !InitializeNavigation()) || bDead)return false;
    const FVector2D P=PositionMetric();auto* ThreatHull=UShipCombatComponent::PhysicalHull(Threat);
    const FVector2D Enemy=Map->ToMetric(WorldToECEF(ThreatHull?ThreatHull->GetComponentLocation():Threat->GetActorLocation()));
    const FVector2D Away=(P-Enemy).GetSafeNormal();int32 Searches=0;
    for(double Angle:{0.,30.,-30.,60.,-60.,90.,-90.})
    {
        const FVector2D Candidate=Enemy+Rotate2D(Away.IsNearlyZero()?FVector2D(1,0):Away,Angle)*FMath::Max(200.f,StandOffDistanceM);
        if(!Map->SameWaterRegion(P,Candidate) || FVector2D::Distance(P,Candidate)<=ArrivalRadiusM)continue;
        const auto Route=Map->FindPath(P,Candidate,MaxSearchExpansions,MaxSearchMilliseconds);
        if(!Route.bSuccess){if(++Searches>=4)break;continue;}
        TacticalDestination=Georeference->GetEllipsoid()->EllipsoidCenteredEllipsoidFixedToLongitudeLatitudeHeight(Map->ToECEF(Candidate));
        TacticalDestination.Z=WorldToGeographic(Hull->GetComponentLocation()).Z;
        bTacticalOverride=true;CurrentFinalDestination=TacticalDestination;GoalMetric=Candidate;bHasGoal=true;bNeedsPlan=true;bExplicitStop=false;
        CurrentRoute.Reset();ArrivalTime=-1;FailedCorridors.Reset();RecoveryPhase=0;RecoveryAttemptCount=0;AvoidElapsed=0;AvoidSide=0;TimeSinceProgress=0;
        DestinationState(this).ConsecutivePlanFailures=0;
        return PlanRoute(P,Candidate);
    }
    // An unreachable pursuit must not fail the valid original patrol.
    return false;
}
bool UShipEnemyNavigationComponent::BeginTacticalMoveTo(FVector LLH)
{
    if(LLH.ContainsNaN() || (!bInitialized && !InitializeNavigation()) || bDead)return false;
    const auto Position=PositionMetric(),Goal=LLHToMetric(LLH);
    const auto Route=Map->FindPath(Position,Goal,MaxSearchExpansions,MaxSearchMilliseconds);
    if(!Route.bSuccess || Route.Points.Num()<2)return false;
    bBackwardRetreatActive=false;bBackwardRetreatBlocked=false;bBackwardRetreatCompleted=false;ReverseThreat.Reset();
    TacticalDestination=LLH;bTacticalOverride=true;bHasGoal=true;bNeedsPlan=true;bExplicitStop=false;
    CurrentFinalDestination=LLH;GoalMetric=Goal;CurrentRoute.Reset();ArrivalTime=-1;
    RecoveryPhase=0;RecoveryAttemptCount=0;FailedCorridors.Reset();TimeSinceProgress=0;AvoidElapsed=0;AvoidSide=0;
    DestinationState(this).ConsecutivePlanFailures=0;return PlanRoute(Position,Goal);
}
void UShipEnemyNavigationComponent::ReturnToPatrol()
{
    bBackwardRetreatActive=false;bBackwardRetreatBlocked=false;bBackwardRetreatCompleted=false;ReverseThreat.Reset();
    bTacticalOverride=false;bHasGoal=false;bNeedsPlan=true;bExplicitStop=false;
    CurrentRoute.Reset();ArrivalTime=-1;RecoveryPhase=0;RecoveryAttemptCount=0;FailedCorridors.Reset();
    NavigationState=EShipNavigationState::Replan;LastNavigationFailureReason.Empty();
    // bTargetB and the actor references deliberately survive the temporary override.
}
bool UShipEnemyNavigationComponent::PlanRoute(const FVector2D& Position,const FVector2D& Goal)
{
    auto& Track=DestinationState(this);Track.bHasSnappedGoal=false;++Track.PlanCount;
    auto R=Map->FindPath(Position,Goal,MaxSearchExpansions,MaxSearchMilliseconds,FailedCorridors,FMath::Max(150.,Map->CellSize*1.5));
    if(!R.bSuccess && !FailedCorridors.IsEmpty())
    {FailedCorridors.Reset();R=Map->FindPath(Position,Goal,MaxSearchExpansions,MaxSearchMilliseconds);}
    LastSearchExpansions=R.Expansions;LastSearchMilliseconds=R.Milliseconds;
    LastPlanTime=GetWorld()->GetTimeSeconds();bNeedsPlan=false;CurrentRoute.Reset();
    DestinationState(this).TrafficRadius=0;
    if(!R.bSuccess)
    {
        ++Track.ConsecutivePlanFailures;Track.RetryAt=GetWorld()->GetTimeSeconds()+ReplanIntervalSeconds;
        if(bDrawDebug)UE_LOG(LogTemp,Display,TEXT("ShipNavigation rejected actor=%s actorWorld=%s requested=%s goalChart=%s validGoalNode=none finalWaypoint=none"),
            *GetNameSafe(DestinationActor(this)),IsValid(DestinationActor(this))?*DestinationActor(this)->GetActorLocation().ToString():TEXT("none"),
            *GeographicToWorld(CurrentFinalDestination).ToString(),*Goal.ToString());
        Fail(R.Failure);return false;
    }
    for(const auto& P:R.Points)CurrentRoute.Add(FVector(P.X,P.Y,0));
    CurrentWaypointIndex=FMath::Min(1,CurrentRoute.Num()-1);LastGoal=Goal;
    Track.ConsecutivePlanFailures=0;
    Track.SnappedGoal=Map->Center(Map->CellAt(Goal));Track.bHasSnappedGoal=true;
    bPathfindingSucceeded=true;LastNavigationFailureReason.Empty();TimeSinceProgress=0;BestRemainingDistance=DBL_MAX;
    if(bDrawDebug)
    {
        const AActor* Target=DestinationActor(this);
        const FVector Requested=GeographicToWorld(CurrentFinalDestination),Grid=MetricToWorld(Track.SnappedGoal),Final=MetricToWorld(R.Points.Last());
        const FVector ActorWorld=IsValid(Target)?Target->GetActorLocation():Requested;
        UE_LOG(LogTemp,Display,TEXT("ShipNavigation plan=%d leg=%d destination actor=%s actorWorld=%s requested=%s goalNode=%s final=%s actor-request=%.6fm actor-goal=%.3fm actor-final=%.6fm request-goal=%.3fm request-final=%.6fm goal-final=%.3fm"),
            Track.PlanCount,PatrolLegsCompleted,
            *GetNameSafe(Target),*ActorWorld.ToString(),*Requested.ToString(),*Grid.ToString(),*Final.ToString(),
            FVector::Distance(ActorWorld,Requested)/100.,FVector::Distance(ActorWorld,Grid)/100.,FVector::Distance(ActorWorld,Final)/100.,
            FVector::Distance(Requested,Grid)/100.,FVector::Distance(Requested,Final)/100.,FVector::Distance(Grid,Final)/100.);
    }
    NavigationState=EShipNavigationState::FollowRoute;return true;
}
bool UShipEnemyNavigationComponent::ReadSensors()
{
    auto Read=[&](const TCHAR* Name,TArray<float>& Values)
    {
        const auto* P=FindFProperty<FArrayProperty>(GetOwner()->GetClass(),Name);
        const FNumericProperty* Inner=P?CastField<FNumericProperty>(P->Inner):nullptr;
        if(!P || !Inner || !Inner->IsFloatingPoint())return false;
        FScriptArrayHelper A(P,P->ContainerPtrToValuePtr<void>(GetOwner()));
        if(A.Num()!=12)return false;
        Values.Reset();for(int32 I=0;I<A.Num();++I){const double V=Inner->GetFloatingPointPropertyValue(A.GetRawPtr(I));if(!FMath::IsFinite(V))return false;Values.Add(V);}
        return true;
    };
    return Read(TEXT("ScanAngles"),SensorAngles) && Read(TEXT("ScanDistances"),SensorDistances);
}
float UShipEnemyNavigationComponent::SensorDistance(float Angle) const
{
    int32 Best=INDEX_NONE;float Difference=FLT_MAX;
    for(int32 I=0;I<SensorAngles.Num();++I){const float D=FMath::Abs(FMath::FindDeltaAngleDegrees(Angle,SensorAngles[I]));if(D<Difference){Difference=D;Best=I;}}
    return SensorDistances.IsValidIndex(Best)?FMath::Max(0.f,SensorDistances[Best]/100.f-HullRadiusM):0;
}
bool UShipEnemyNavigationComponent::SweepClear(const FVector& A,const FVector& B,float Radius) const
{
    FCollisionQueryParams Q(SCENE_QUERY_STAT(EnemyNavigationSweep),true,GetOwner());NavalWaterQueries::IgnoreWaterActors(GetWorld(),Q);
    // Horizontal clearance must not also become the vessel's draft. A 40 m sphere
    // falsely starts penetrating a 24 m-deep seabed beneath this ~10 m keel.
    // Preserve the full hull's vertical range, offset above/below its actual pivot.
    const FBox Bounds=Hull->GetLocalBounds().GetBox();
    const FVector LLH=WorldToGeographic(A);
    const FVector Up=(GeographicToWorld(LLH+FVector(0,0,1))-A).GetSafeNormal();
    double MinHeight=DBL_MAX,MaxHeight=-DBL_MAX;
    for(int32 I=0;I<8;++I)
    {
        const FVector Corner(I&1?Bounds.Max.X:Bounds.Min.X,I&2?Bounds.Max.Y:Bounds.Min.Y,I&4?Bounds.Max.Z:Bounds.Min.Z);
        const double Height=FVector::DotProduct(Hull->GetComponentTransform().TransformVector(Corner),Up);
        MinHeight=FMath::Min(MinHeight,Height);MaxHeight=FMath::Max(MaxHeight,Height);
    }
    const double HalfHeight=(MaxHeight-MinHeight)*0.5+200.;
    const FVector Offset=Up*((MinHeight+MaxHeight)*0.5);
    // Pitch/roll must affect the actual hull's draft, not tilt a 110m-wide
    // clearance box into the seabed. Keep the expanded query level to the sea.
    const FQuat Rotation=FRotationMatrix::MakeFromXZ(FVector::VectorPlaneProject(Hull->GetForwardVector(),Up).GetSafeNormal(),Up).ToQuat();
    const FVector Extent(FMath::Max(1.f,Radius*100.f),FMath::Max(1.f,Radius*100.f),HalfHeight);
    FHitResult Hit;
    const bool bHit=GetWorld()->SweepSingleByChannel(Hit,A+Offset,B+Offset,Rotation,ECC_Visibility,FCollisionShape::MakeBox(Extent),Q);
    auto& Track=DestinationState(const_cast<UShipEnemyNavigationComponent*>(this));
    Track.SweepActor=bHit?Hit.GetActor():nullptr;
    Track.SweepCenter=bHit && Hit.GetComponent()?Hit.GetComponent()->GetComponentLocation():FVector::ZeroVector;
    if(bHit && bDrawDebug && GetWorld()->GetTimeSeconds()-DestinationState(const_cast<UShipEnemyNavigationComponent*>(this)).LastTelemetry>=1.)
        UE_LOG(LogTemp,Display,TEXT("ShipNavigation SWEEP ship=%s radius=%.1fm actor=%s component=%s distance=%.1fm penetration=%d impact=%s heights=[%.1f,%.1f] up=%s"),
            *GetOwner()->GetActorNameOrLabel(),Radius,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),Hit.Distance/100.,Hit.bStartPenetrating,*Hit.ImpactPoint.ToString(),MinHeight/100.,MaxHeight/100.,*Up.ToString());
    return !bHit;
}
bool UShipEnemyNavigationComponent::CollisionCoverageAvailable() const
{
    for(TActorIterator<AActor> I(GetWorld());I;++I)
    {
        if(!I->GetClass()->GetName().Contains(TEXT("Cesium3DTileset")))continue;
        if(const auto* Filter=I->FindComponentByClass<USeaTerrainCollisionComponent>();Filter && Filter->HasCollisionCoverageAt(Hull->GetComponentLocation()))return true;
        TArray<UPrimitiveComponent*> Cs;I->GetComponents(Cs);
        for(auto* C:Cs)if(C->GetClass()->GetName().Contains(TEXT("CesiumGltf")) && C->IsQueryCollisionEnabled() &&
            C->Bounds.GetBox().ComputeSquaredDistanceToPoint(Hull->GetComponentLocation())<FMath::Square(80000.))return true;
    }
    return false;
}
void UShipEnemyNavigationComponent::UpdatePerception()
{
    bCollisionDataUncertain=!CollisionCoverageAvailable();
    // Installed shared combat owns all targeting and bounded visibility traces.
    if(GetOwner()->FindComponentByClass<UShipCombatComponent>()){DetectedPlayer=nullptr;return;}
    APawn* Candidate=UGameplayStatics::GetPlayerPawn(this,0);
    const double Now=GetWorld()->GetTimeSeconds();
    if(!Candidate || Candidate==GetOwner() || !Candidate->IsPlayerControlled() || (PlayerShipClass && !Candidate->IsA(PlayerShipClass)))
    {DetectedPlayer=nullptr;return;}
    const float D=FVector::Dist2D(Hull->GetComponentLocation(),Candidate->GetActorLocation())/100;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(EnemyShipPerception),true,GetOwner());NavalWaterQueries::IgnoreWaterActors(GetWorld(),Q);
    FHitResult Hit;const FVector A=Hull->GetComponentLocation()+FVector(0,0,1500),B=Candidate->GetActorLocation()+FVector(0,0,1500);
    const bool Visible=!GetWorld()->LineTraceSingleByChannel(Hit,A,B,ECC_Visibility,Q) || Hit.GetActor()==Candidate;
    if(D<=DetectionRadiusM && Visible){DetectedPlayer=Candidate;LastSeenTime=Now;}
    else if(D>FMath::Max(LoseTargetRadiusM,DetectionRadiusM+100) || Now-LastSeenTime>5 || !IsValid(DetectedPlayer))DetectedPlayer=nullptr;
}
void UShipEnemyNavigationComponent::SelectObjective(const FVector2D& P)
{
    if(bTacticalOverride)
    {GoalMetric=LLHToMetric(TacticalDestination);CurrentFinalDestination=TacticalDestination;return;}
    const auto* Combat=GetOwner()->FindComponentByClass<UShipCombatComponent>();
    const bool IndependentFighter=bKeepAuthoredPatrol || (Role==EShipEnemyRole::Fighter && Combat);
    if(IsValid(DetectedPlayer) && Role!=EShipEnemyRole::Passive && !IndependentFighter)
    {
        const FVector2D Player=Map->ToMetric(WorldToECEF(DetectedPlayer->GetActorLocation()));
        const FVector2D Away=(P-Player).GetSafeNormal();
        FVector2D Proposed=P;
        if(Role==EShipEnemyRole::Coward)
        {
            NavigationState=EShipNavigationState::Flee;
            if(bHasGoal && FVector2D::Distance(P,GoalMetric)>ArrivalRadiusM*2 && GetWorld()->GetTimeSeconds()-LastPlanTime<ReplanIntervalSeconds)return;
            bool Found=false;
            for(double A:{0.,30.,-30.,60.,-60.,90.,-90.,120.,-120.})
            {const FVector2D C=P+Rotate2D(Away.IsNearlyZero()?FVector2D(1,0):Away,A)*2000;if(Map->SameWaterRegion(P,C)){Proposed=C;Found=true;break;}}
            if(!Found){Fail(TEXT("No covered water escape destination"));return;}
        }
        else
        {
            NavigationState=Role==EShipEnemyRole::Fighter?EShipNavigationState::Attack:EShipNavigationState::Chase;
            const double Range=Role==EShipEnemyRole::Fighter?EngagementDistanceM:EngagementDistanceM*0.75;
            Proposed=Player+(Away.IsNearlyZero()?FVector2D(1,0):Away)*Range;
            if(!Map->SameWaterRegion(P,Proposed))
            {
                bool Found=false;for(double A:{30.,-30.,60.,-60.,90.,-90.,120.,-120.,180.})
                {const FVector2D C=Player+Rotate2D(Away,A)*Range;if(Map->SameWaterRegion(P,C)){Proposed=C;Found=true;break;}}
                if(!Found){Fail(TEXT("No safe covered engagement position"));return;}
            }
        }
        if(!bHasGoal || FVector2D::Distance(Proposed,GoalMetric)>150){GoalMetric=Proposed;bHasGoal=true;bNeedsPlan=true;}
        // A temporary engagement objective must not replace an explicit navigation command.
        // GoalMetric holds the combat objective; retain the requested final destination for resumption.
        if(NavigationMode!=EShipNavigationMode::Destination)
            CurrentFinalDestination=Georeference->GetEllipsoid()->EllipsoidCenteredEllipsoidFixedToLongitudeLatitudeHeight(Map->ToECEF(GoalMetric));
        return;
    }
    if(NavigationMode==EShipNavigationMode::Patrol)
    {
        const bool Circuit=!PatrolCircuitActors.IsEmpty();
        if(Circuit)
        {
            if(PatrolCircuitActors.Num()<3 || !PatrolCircuitActors.IsValidIndex(PatrolCircuitIndex))
            {CurrentRoute.Reset();bHasGoal=false;bNeedsPlan=false;Fail(TEXT("Patrol circuit requires at least three targets and a valid current index"));return;}
            for(int32 I=0;I<PatrolCircuitActors.Num();++I)
            {
                const auto* A=PatrolCircuitActors[I].Get();const auto* B=PatrolCircuitActors[(I+1)%PatrolCircuitActors.Num()].Get();
                if(!IsValid(A) || !IsValid(B) || A->GetWorld()!=GetWorld() || B->GetWorld()!=GetWorld() ||
                    FVector2D::Distance(LLHToMetric(WorldToGeographic(A->GetActorLocation())),LLHToMetric(WorldToGeographic(B->GetActorLocation())))<=ArrivalRadiusM*2)
                {CurrentRoute.Reset();bHasGoal=false;bNeedsPlan=false;Fail(TEXT("Patrol circuit has missing, wrong-world or overlapping targets"));return;}
            }
        }
        const bool bActorPatrol=Circuit || PatrolActorA || PatrolActorB;
        if(!Circuit && bActorPatrol && (!IsValid(PatrolActorA) || !IsValid(PatrolActorB) || PatrolActorA==PatrolActorB || PatrolActorA->GetWorld()!=GetWorld() || PatrolActorB->GetWorld()!=GetWorld()))
        {CurrentRoute.Reset();bHasGoal=false;bNeedsPlan=false;Fail(TEXT("Two-point patrol requires both valid same-world Target Point actors"));return;}
        if(!Circuit && bActorPatrol && FVector2D::Distance(LLHToMetric(WorldToGeographic(PatrolActorA->GetActorLocation())),LLHToMetric(WorldToGeographic(PatrolActorB->GetActorLocation())))<=ArrivalRadiusM*2)
        {CurrentRoute.Reset();bHasGoal=false;bNeedsPlan=false;Fail(TEXT("Two-point patrol targets have overlapping arrival zones"));return;}
        const AActor* Assigned=Circuit?PatrolCircuitActors[PatrolCircuitIndex].Get():(bTargetB?PatrolActorB.Get():PatrolActorA.Get());
        const FVector LLH=IsValid(Assigned)?WorldToGeographic(Assigned->GetActorLocation()):(bTargetB?PatrolPointB:PatrolPointA);
        FVector2D Target=LLHToMetric(LLH);
        const FVector Offset=Circuit?FVector::ZeroVector:(bTargetB?PatrolOffsetB:PatrolOffsetA);
        const FVector2D OffsetTarget=Target+FVector2D(Offset.X,Offset.Y);
        if(!Offset.IsNearlyZero() && Map->IsWater(OffsetTarget) && Map->SegmentClear(Target,OffsetTarget))Target=OffsetTarget;
        auto& Track=DestinationState(this);
        if(bKeepAuthoredPatrol && bUsingOperationalChart && bActorPatrol)
        {
            if(!Track.bApproachCached || Track.ApproachMap!=Map.Get() || !Track.ApproachRequested.Equals(Target,.01) || Track.ApproachRadius!=ArrivalRadiusM)
            {
                FVector2D Resolved;
                if(!Map->ResolveArrivalGoal(Target,ArrivalRadiusM,P,Resolved))
                {CurrentRoute.Reset();bHasGoal=false;bNeedsPlan=false;Fail(FString::Printf(TEXT("No safe approach within %.0f m of %s"),ArrivalRadiusM,*GetNameSafe(Assigned)));return;}
                Track.ApproachRequested=Target;Track.ApproachResolved=Resolved;Track.ApproachMap=Map.Get();Track.ApproachRadius=ArrivalRadiusM;Track.bApproachCached=true;
            }
            Target=Track.ApproachResolved;
        }
        const FVector ECEF=IsValid(Assigned)?WorldToECEF(Assigned->GetActorLocation()):FVector::ZeroVector;
        const bool bChanged=!bHasGoal || !Target.Equals(GoalMetric,1) ||
            (bActorPatrol && (Track.Actor.Get()!=Assigned || FVector::Distance(ECEF,Track.SampleECEF)>1.));
        if(bChanged)
        {
            GoalMetric=Target;
            // Keep the actual anchor's height as well as its horizontal position.
            // ENU projection alone loses ellipsoid height and makes debug/final
            // world endpoints differ from the actor even with zero formation offset.
            CurrentFinalDestination=(bKeepAuthoredPatrol || Target.Equals(LLHToMetric(LLH),1.e-5))?LLH:
                Georeference->GetEllipsoid()->EllipsoidCenteredEllipsoidFixedToLongitudeLatitudeHeight(Map->ToECEF(Target));
            CurrentFinalDestination.Z=LLH.Z;bNeedsPlan=true;bHasGoal=true;ArrivalTime=-1;
            CurrentRoute.Reset();bPathfindingSucceeded=false;RecoveryPhase=0;RecoveryAttemptCount=0;FailedCorridors.Reset();
            Track.Actor=const_cast<AActor*>(Assigned);Track.SampleECEF=ECEF;Track.bExplicitCommand=false;Track.bHasSnappedGoal=false;Track.ConsecutivePlanFailures=0;
            Track.YieldSince=-1;TrafficWaitSeconds=0;
            NavigationState=EShipNavigationState::Replan;LastNavigationFailureReason.Empty();
            if(bActorPatrol)
            {
                if(auto* Property=FindFProperty<FObjectPropertyBase>(GetOwner()->GetClass(),TEXT("DestinationActor")))
                    Property->SetObjectPropertyValue_InContainer(GetOwner(),const_cast<AActor*>(Assigned));
            }
        }
    }
    else if(bHasGoal)
    {
        const FVector2D Target=LLHToMetric(CurrentFinalDestination);
        if(!Target.Equals(GoalMetric,1)){GoalMetric=Target;bNeedsPlan=true;ArrivalTime=-1;}
    }
}
bool UShipEnemyNavigationComponent::TryRecovery(const FVector2D& P,float Speed,float Dt,float& T,float& R)
{
    if(RecoveryPhase==0)return false;
    NavigationState=EShipNavigationState::Recovery;bStuck=true;RecoveryElapsed+=Dt;
    const FVector F=Heading->GetForwardVector();
    if(RecoveryPhase==1)
    {
        const float ForwardSpeed=FVector::DotProduct(Hull->GetPhysicsLinearVelocity(),F)/100;
        T=ForwardSpeed>0.5f?-0.12f:ForwardSpeed<-.5f?0.12f:0;R=0;
        if(Speed<0.5f || RecoveryElapsed>8){RecoveryPhase=2;RecoveryElapsed=0;RecoveryStart=P;}
    }
    else if(RecoveryPhase==2)
    {
        const FVector End=Hull->GetComponentLocation()-F*MaxReverseDistanceM*100;
        const FVector2D Back=Map->ToMetric(WorldToECEF(End));
        if(SensorDistance(180)<MaxReverseDistanceM+LocalClearanceMarginM || !Map->SegmentClear(P,Back) || !SweepClear(Hull->GetComponentLocation(),End,HullHalfWidthM+LocalClearanceMarginM))
        {T=0;R=0;RecoveryPhase=4;RecoveryElapsed=0;}
        else{RecoveryPhase=3;RecoveryElapsed=0;T=0;R=0;}
    }
    else if(RecoveryPhase==3)
    {
        const FVector RearEnd=Hull->GetComponentLocation()-F*3000;
        const bool RearSafe=SensorDistance(180)>HullRadiusM+20 && Map->SegmentClear(P,Map->ToMetric(WorldToECEF(RearEnd))) && SweepClear(Hull->GetComponentLocation(),RearEnd,HullHalfWidthM);
        T=RearSafe?-0.15f:0;R=0;
        if(!RearSafe || RecoveryElapsed>MaxReverseSeconds || FVector2D::Distance(P,RecoveryStart)>MaxReverseDistanceM)
        {RecoveryPhase=4;RecoveryElapsed=0;}
    }
    else if(RecoveryPhase==4)
    {
        T=0;R=0;
        if(RecoveryElapsed>12){RecoveryPhase=0;Fail(TEXT("Recovery could not stabilize velocity within its time budget"));T=0;R=0;return true;}
        if(RecoveryElapsed>2 && FMath::Abs(Speed)<0.5)
        {
            if(RecoveryAttemptCount>=MaxRecoveryAttempts){RecoveryPhase=0;Fail(TEXT("Bounded recovery attempts exhausted"));return true;}
            ++RecoveryAttemptCount;RecoveryPhase=0;bStuck=false;bNeedsPlan=true;TimeSinceProgress=0;
            if(!PlanRoute(P,GoalMetric))Fail(TEXT("Recovery replan failed: ")+LastNavigationFailureReason);
        }
    }
    return true;
}

bool UShipEnemyNavigationComponent::UpdateNavigation(float Dt)
{
    if(const auto* W=GetOwner()->FindComponentByClass<UShipWeaponComponent>())if(W->bIsDestroyed){bDead=true;NavigationState=EShipNavigationState::Dead;SetDriveCommand(0,0);return true;}
    if(!bNewNavigationEnabled && !bDead){bOwnsCommands=false;return false;}
    bOwnsCommands=true;
    if(!UNavalBattleSubsystem::CanShipAct(GetOwner()))
    {
        SetDriveCommand(0,0);TimeSinceProgress=0;bHasObservedPosition=false;
        if(ArrivalTime>=0)ArrivalTime+=FMath::Max(0.f,Dt);
        return true;
    }
    // A cached boolean alone cannot certify non-reflected shared data after a
    // streaming lifecycle transition or editor component reconstruction.
    if(!bInitialized || !Map.IsValid() || !IsValid(Hull) || !IsValid(Heading) || !IsValid(GlobeAnchor) || !IsValid(Georeference) || GlobeAnchor->ResolveGeoreference()!=Georeference)
    {
        bNeedsPlan=true;
        if(!InitializeNavigation()){SetDriveCommand(0,0);return true;}
    }
    if(const auto* Weapon=GetOwner()->FindComponentByClass<UShipWeaponComponent>())if(Weapon->bIsDestroyed)bDead=true;
    if(bDead){NavigationState=EShipNavigationState::Dead;SetDriveCommand(0,0);return true;}
    if(GetWorld()->GetTimeSeconds()<PatrolReadyAt){bHasObservedPosition=false;SetDriveCommand(0,0);return true;}
    if(bExplicitStop){bHasObservedPosition=false;NavigationState=EShipNavigationState::Idle;SetDriveCommand(0,0);return true;}
    if(!FMath::IsFinite(Dt) || Dt<=0){SetDriveCommand(0,0);return true;}
    Dt=FMath::Min(Dt,0.2f);
    // Read the placed Blueprint's single DestinationActor every update. Compare
    // ECEF meters so rebasing alone does not trigger repeated A* searches.
    if(NavigationMode==EShipNavigationMode::Destination)
    {
        auto& Track=DestinationState(this);
        if(!bHasGoal)Track.bExplicitCommand=false;
        if(!Track.bExplicitCommand)
        {
            AActor* Target=DestinationActor(this);
            if(!IsValid(Target) || Target->GetWorld()!=GetWorld())
            {
                CurrentRoute.Reset();bHasGoal=false;bNeedsPlan=false;Track.bHasSnappedGoal=false;
                Fail(TEXT("DestinationActor is missing, destroyed, or belongs to another world"));SetDriveCommand(0,0);return true;
            }
            const FVector ECEF=WorldToECEF(Target->GetActorLocation());
            if(!bHasGoal || Track.Actor.Get()!=Target || FVector::Distance(ECEF,Track.SampleECEF)>1.)
            {
                Track.Actor=Target;Track.SampleECEF=ECEF;Track.bHasSnappedGoal=false;
                CurrentFinalDestination=WorldToGeographic(Target->GetActorLocation());
                GoalMetric=LLHToMetric(CurrentFinalDestination);bHasGoal=true;bNeedsPlan=true;
                CurrentRoute.Reset();bPathfindingSucceeded=false;ArrivalTime=-1;
                RecoveryAttemptCount=0;RecoveryPhase=0;FailedCorridors.Reset();
                NavigationState=EShipNavigationState::Replan;LastNavigationFailureReason.Empty();
            }
        }
    }
    const FVector2D P=PositionMetric();
    if(bOriginChanged || LastWorldOrigin!=GetWorld()->OriginLocation)
    {bOriginChanged=false;LastWorldOrigin=GetWorld()->OriginLocation;TimeSinceProgress=0;ProgressPosition=P;BestRemainingDistance=DBL_MAX;bHasObservedPosition=false;SetDriveCommand(0,0);return true;}
    const float ObservedSpeed=Hull->GetPhysicsLinearVelocity().Size()/100;
    // ECEF-derived EN is invariant under Cesium/world-origin shifts. A jump
    // beyond the actual velocity envelope is a development placement, not a
    // normal physics tick. Never continually replan during ordinary movement.
    const double PhysicalEnvelope=FMath::Max(30.,double(FMath::Max(ObservedSpeed,LastObservedSpeedMps))*GetWorld()->GetDeltaSeconds()*3.+10.);
    if(bHasObservedPosition && FVector2D::Distance(P,LastObservedPosition)>PhysicalEnvelope)
        NotifyDevelopmentReposition();
    LastObservedPosition=P;LastObservedSpeedMps=ObservedSpeed;bHasObservedPosition=true;
    if(bBackwardRetreatActive)return UpdateBackwardRetreat(Dt);
    if(bTacticalOverride && (bBackwardRetreatBlocked || bBackwardRetreatCompleted))
    {const float S=FVector::DotProduct(Hull->GetPhysicsLinearVelocity(),Heading->GetForwardVector())/100;SetDriveCommand(S<-.5f?.12f:S>.5f?-.12f:0,0);return true;}
    PerceptionClock-=Dt;if(PerceptionClock<=0){PerceptionClock=0.5;UpdatePerception();}
    SelectObjective(P);
    if(NavigationState==EShipNavigationState::Failed)
    {
        auto& Track=DestinationState(this);
        if(bHasGoal && Track.RecoveryRetryAt>0 && GetWorld()->GetTimeSeconds()>=Track.RecoveryRetryAt && Map->IsWater(P))
        {
            Track.RecoveryRetryAt=0;++Track.RecoveryRetries;Track.TrafficRadius=0;
            RecoveryPhase=0;RecoveryAttemptCount=0;TimeSinceProgress=0;BestRemainingDistance=DBL_MAX;
            FailedCorridors.Reset();AvoidElapsed=0;AvoidSide=0;AvoidSideChanges=0;
            bNeedsPlan=true;NavigationState=EShipNavigationState::Replan;
        }
        else if(NavigationMode==EShipNavigationMode::Patrol && bHasGoal && Track.ConsecutivePlanFailures>0 && Track.ConsecutivePlanFailures<=3 && GetWorld()->GetTimeSeconds()>=Track.RetryAt)
        {bNeedsPlan=true;NavigationState=EShipNavigationState::Replan;}
        else{SetDriveCommand(0,0);return true;}
    }
    if(!bHasGoal){NavigationState=EShipNavigationState::Idle;SetDriveCommand(0,0);return true;}
    const double Now=GetWorld()->GetTimeSeconds();
    {
        auto& Track=DestinationState(this);
        TrafficWaitSeconds=Track.YieldSince>=0?float(Now-Track.YieldSince):0;
        if(TrafficWaitSeconds>=TrafficPriorityWaitSeconds && !Track.bYieldingToPlayer && Now>=Track.PriorityAvailableAt)
        {
            bool CanTakePriority=true;
            for(TActorIterator<APawn> Other(GetWorld());Other;++Other)
            {
                if(*Other==GetOwner() || Other->IsPlayerControlled() || Other->ActorHasTag(TEXT("Dead")))continue;
                const auto* OtherNav=Other->FindComponentByClass<UShipEnemyNavigationComponent>();
                const auto* OtherTrack=OtherNav?DestinationTracking.Find(const_cast<UShipEnemyNavigationComponent*>(OtherNav)):nullptr;
                if(!OtherTrack || FVector::Dist(Other->GetActorLocation(),Hull->GetComponentLocation())/100>SafeShipSeparationM*2.5)continue;
                const double OtherWait=OtherTrack->YieldSince>=0?Now-OtherTrack->YieldSince:0;
                const bool OtherEligible=!OtherTrack->bYieldingToPlayer && Now>=OtherTrack->PriorityAvailableAt;
                if(OtherTrack->PriorityUntil>Now || (OtherEligible && (OtherWait>TrafficWaitSeconds+2 ||
                    (OtherWait>=TrafficPriorityWaitSeconds && FMath::Abs(OtherWait-TrafficWaitSeconds)<=2 && Other->GetFName().LexicalLess(GetOwner()->GetFName())))))
                {CanTakePriority=false;break;}
            }
            if(CanTakePriority)
            {
                Track.PriorityUntil=Now+TrafficPriorityHoldSeconds;Track.PriorityAvailableAt=Track.PriorityUntil+25;
                Track.YieldSince=-1;Track.TrafficRadius=0;FailedCorridors.Reset();bNeedsPlan=true;
                if(bDrawDebug)UE_LOG(LogTemp,Display,TEXT("ShipNavigation traffic priority ship=%s wait=%.1fs hold=%.1fs"),*GetNameSafe(GetOwner()),TrafficWaitSeconds,TrafficPriorityHoldSeconds);
            }
        }
        bTemporaryTrafficPriority=Track.PriorityUntil>Now;
    }
    if(bNeedsPlan && (Now-LastPlanTime>=ReplanIntervalSeconds || CurrentRoute.IsEmpty()))
    {
        if(Now-LastPlanTime<ReplanIntervalSeconds){NavigationState=EShipNavigationState::Replan;SetDriveCommand(0,0);return true;}
        if(!PlanRoute(P,GoalMetric)){SetDriveCommand(0,0);return true;}
    }
    if(!Map->IsWater(P)){Fail(TEXT("Ship entered blocked land/buffer or left navigation coverage"));SetDriveCommand(0,0);return true;}
    if(!ReadSensors()){LastNavigationFailureReason=TEXT("Waiting for 12 existing Blueprint sensor outputs");SetDriveCommand(0,0);return true;}
    LastNavigationFailureReason.Empty();
    const FVector Forward=Heading->GetForwardVector();
    const FVector Velocity=Hull->GetPhysicsLinearVelocity();
    const float SignedSpeed=FVector::DotProduct(Velocity,Forward)/100.f;
    const float Speed=FVector::VectorPlaneProject(Velocity,Hull->GetUpVector()).Size()/100.f;
    const float YawRate=FVector::DotProduct(Hull->GetPhysicsAngularVelocityInDegrees(),Hull->GetUpVector());
    float T=0,R=0;bAvoidingObstacle=false;
    if(TryRecovery(P,Speed,Dt,T,R))
    {
        if(T<0 && (SensorDistance(180)<LocalClearanceMarginM+50 || !SweepClear(Hull->GetComponentLocation(),Hull->GetComponentLocation()-Forward*3000,HullHalfWidthM)))T=0;
        float Out=FMath::FInterpConstantTo(DesiredThrottle,T,Dt,ThrottleRatePerSecond);
        if(T<=0)Out=FMath::Min(Out,0.f);
        if(NavigationState==EShipNavigationState::Failed){Out=0;R=0;}
        SetDriveCommand(Out,FMath::FInterpConstantTo(DesiredRudder,R,Dt,RudderRatePerSecond));return true;
    }
    // Even sub-meter motion below the replan threshold must satisfy acceptance
    // against the live actor, rather than the previously planned sample.
    FVector2D AcceptanceGoal=GoalMetric;
    if(PatrolOffsetA.IsNearlyZero() && PatrolOffsetB.IsNearlyZero() && !DestinationState(this).bExplicitCommand && GoalMetric.Equals(LLHToMetric(CurrentFinalDestination),1.e-5))
        if(const AActor* Target=DestinationActor(this);IsValid(Target))AcceptanceGoal=Map->ToMetric(WorldToECEF(Target->GetActorLocation()));
    const double GoalDistance=FVector2D::Distance(P,AcceptanceGoal);
    const bool AuthoredStandoff=bKeepAuthoredPatrol && !GoalMetric.Equals(LLHToMetric(CurrentFinalDestination),1);
    const double RouteArrivalRadius=AuthoredStandoff?FMath::Min(10.,ArrivalRadiusM*.2):ArrivalRadiusM;
    const bool WithinAuthoredZone=!AuthoredStandoff || FVector2D::Distance(P,LLHToMetric(CurrentFinalDestination))<=ArrivalRadiusM;
    if(GoalDistance<=RouteArrivalRadius && WithinAuthoredZone && Map->SegmentClear(P,AcceptanceGoal))
    {
        DestinationState(this).YieldSince=-1;TrafficWaitSeconds=0;
        DesiredSpeed=0;T=SignedSpeed>0.5f?-0.12f:SignedSpeed<-.5f?0.12f:0;
        NavigationState=EShipNavigationState::Arrived;
        if(Speed<0.5f)
        {
            if(ArrivalTime<0)ArrivalTime=Now;
            const auto* Combat=GetOwner()->FindComponentByClass<UShipCombatComponent>();
            const bool IndependentFighter=bKeepAuthoredPatrol || (Role==EShipEnemyRole::Fighter && Combat);
            if(!bTacticalOverride && NavigationMode==EShipNavigationMode::Patrol && (IndependentFighter || Role==EShipEnemyRole::Passive || !IsValid(DetectedPlayer)) && Now-ArrivalTime>=PatrolWaitSeconds)
            {
                if(bDrawDebug)UE_LOG(LogTemp,Display,TEXT("ShipNavigation ARRIVAL leg=%d target=%s distance=%.3fm speed=%.3fm/s switching=%s"),
                    PatrolLegsCompleted,*GetNameSafe(DestinationActor(this)),GoalDistance,Speed,bTargetB?TEXT("A"):TEXT("B"));
                if(!PatrolCircuitActors.IsEmpty())
                {
                    PatrolCircuitIndex=(PatrolCircuitIndex+1)%PatrolCircuitActors.Num();++CircuitLegsCompleted;
                    PatrolCircuitsCompleted=CircuitLegsCompleted/PatrolCircuitActors.Num();
                }
                else bTargetB=!bTargetB;
                ++PatrolLegsCompleted;bHasGoal=false;bNeedsPlan=true;
                CurrentRoute.Reset();bPathfindingSucceeded=false;ArrivalTime=-1;RecoveryAttemptCount=0;FailedCorridors.Reset();
            }
        }
        else ArrivalTime=-1;
        SetDriveCommand(FMath::FInterpConstantTo(DesiredThrottle,T,Dt,ThrottleRatePerSecond),FMath::FInterpConstantTo(DesiredRudder,0,Dt,RudderRatePerSecond));UpdateWeapon(Dt);return true;
    }
    ArrivalTime=-1;
    if(!CurrentRoute.IsValidIndex(CurrentWaypointIndex)){Fail(TEXT("Missing current route waypoint"));SetDriveCommand(0,0);return true;}
    auto WP=[&](int32 I){return FVector2D(CurrentRoute[I].X,CurrentRoute[I].Y);};
    auto& Traffic=DestinationState(this);
    if(Now>Traffic.TrafficUntil)Traffic.TrafficRadius=0;
    if(Traffic.TrafficRadius>0)
    {
        // Once a vessel has passed the old obstacle, release its commitment so
        // the next obstruction (including a stationary player) can be planned
        // around before entering its braking distance.
        if(FVector2D::DotProduct(P-Traffic.TrafficCenter,Traffic.TrafficTravelDirection)>HullRadiusM &&
            FVector2D::Distance(P,Traffic.TrafficCenter)>Traffic.TrafficRadius+HullRadiusM)
            Traffic.TrafficRadius=0;
    }
    auto OutsideCircle=[](const FVector2D& A,const FVector2D& B,const FVector2D& C,double Radius)
    {
        const FVector2D Segment=B-A;
        const double Along=FMath::Clamp(FVector2D::DotProduct(C-A,Segment)/FMath::Max(1.e-9,Segment.SizeSquared()),0.,1.);
        return FVector2D::DistSquared(C,A+Along*Segment)>=FMath::Square(Radius);
    };
    // Resolve traffic early enough for a real A* detour. Retain that detour
    // through the turn; do not flip its first waypoint every frame as ships move.
    if(SafeShipSeparationM>0 && Now>=Traffic.TrafficCheckAt)
    {
        Traffic.TrafficCheckAt=Now+1;
        for(TActorIterator<APawn> Other(GetWorld());Other;++Other)
        {
            const auto* OtherWeapon=Other->FindComponentByClass<UShipWeaponComponent>();
            if(*Other==GetOwner() || !OtherWeapon || OtherWeapon->bIsDestroyed)continue;
            auto* OtherHull=UShipCombatComponent::PhysicalHull(*Other);if(!OtherHull)continue;
            const FVector2D Block=Map->ToMetric(WorldToECEF(OtherHull->GetComponentLocation()));
            if(Traffic.TrafficRadius>0)
            {
                // A stationary/manual player is a new hard obstruction even
                // while an earlier moving-AI detour remains committed.
                if(!Other->IsPlayerControlled())continue;
                if(Traffic.TrafficActor.Get()==*Other && FVector2D::Distance(Block,Traffic.TrafficCenter)<Traffic.TrafficRadius*.5)continue;
            }
            const double Distance=FVector2D::Distance(P,Block);
            if(Distance>SafeShipSeparationM*1.75 || Distance<200)continue;
            // Moving ships use local swept avoidance and bounded right of way;
            // treating each moving hull as a long-lived circular island causes starvation.
            if(!Other->IsPlayerControlled() && (Now<20 || OtherHull->GetPhysicsLinearVelocity().Size()>100 || HasShipRightOfWay(this,*Other,Now)))continue;
            const double Radius=FMath::Min(double(SafeShipSeparationM)*.75,Distance-100);
            if(OutsideCircle(P,WP(CurrentWaypointIndex),Block,Radius+30) || FVector2D::Distance(GoalMetric,Block)<Radius+100)continue;
            const FSeaRouteResult Detour=Map->FindPath(P,GoalMetric,75000,20,{Block},Radius);
            if(!Detour.bSuccess || Detour.Points.Num()<2)continue;
            CurrentRoute.Reset();for(const auto& Point:Detour.Points)CurrentRoute.Add(FVector(Point,0));
            CurrentWaypointIndex=1;bPathfindingSucceeded=true;bNeedsPlan=false;LastPlanTime=Now;
            Traffic.TrafficCenter=Block;Traffic.TrafficRadius=Radius;Traffic.TrafficUntil=Now+240;
            Traffic.TrafficActor=*Other;
            Traffic.TrafficTravelDirection=(GoalMetric-P).GetSafeNormal();
            BestRemainingDistance=DBL_MAX;TimeSinceProgress=0;AvoidElapsed=0;AvoidSide=0;
            if(bDrawDebug)UE_LOG(LogTemp,Display,TEXT("ShipNavigation traffic detour ship=%s blocker=%s radius=%.1fm"),*GetNameSafe(GetOwner()),*GetNameSafe(*Other),Radius);
            break;
        }
    }
    auto RouteSegmentClear=[&](const FVector2D& A,const FVector2D& B)
    {
        if(!Map->SegmentClear(A,B))return false;
        if(Traffic.TrafficRadius>0 && !OutsideCircle(A,B,Traffic.TrafficCenter,Traffic.TrafficRadius))return false;
        for(const auto& Block:FailedCorridors)if(!OutsideCircle(A,B,Block,FMath::Max(150.,Map->CellSize*1.5)))return false;
        return true;
    };
    const double LookAhead=FMath::Clamp(double(Speed)*12.,80.,300.);
    while(CurrentWaypointIndex<CurrentRoute.Num()-1)
    {
        const FVector2D Current=WP(CurrentWaypointIndex),Next=WP(CurrentWaypointIndex+1);
        if(FVector2D::Distance(P,Current)<LookAhead && RouteSegmentClear(P,Next))++CurrentWaypointIndex;else break;
    }
    const FVector2D W=WP(CurrentWaypointIndex);
    if(!Map->SegmentClear(P,W)){bNeedsPlan=true;NavigationState=EShipNavigationState::Replan;SetDriveCommand(0,0);return true;}
    const FVector TargetWorld=MetricToWorld(W);
    const FVector Up=Hull->GetUpVector();
    const FVector F=FVector::VectorPlaneProject(Forward,Up).GetSafeNormal(),D=FVector::VectorPlaneProject(TargetWorld-Hull->GetComponentLocation(),Up).GetSafeNormal();
    const float Error=FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Up,FVector::CrossProduct(F,D)),FVector::DotProduct(F,D)));
    DesiredHeading=(TargetWorld-Hull->GetComponentLocation()).Rotation().Yaw;
    R=FMath::Clamp(RudderSign*Error/45.f-YawRateDamping*YawRate,-1.f,1.f);
    double Remaining=FVector2D::Distance(P,W);
    for(int32 I=CurrentWaypointIndex;I<CurrentRoute.Num()-1;++I)Remaining+=FVector2D::Distance(WP(I),WP(I+1));
    double TargetSpeed=CruiseSpeedMps;
    TargetSpeed=FMath::Min(TargetSpeed,FMath::Sqrt(2.*FMath::Max(0.01f,BrakingDecelerationMps2)*FMath::Max(0.,Remaining-RouteArrivalRadius)));
    // Upcoming corner limits velocity BEFORE entering the turn, including minimum practical radius.
    if(CurrentWaypointIndex<CurrentRoute.Num()-1)
    {
        const FVector2D In=(W-P).GetSafeNormal(),Out=(WP(CurrentWaypointIndex+1)-W).GetSafeNormal();
        const double Angle=FMath::Acos(FMath::Clamp(FVector2D::DotProduct(In,Out),-1.,1.));
        if(Angle>0.1)
        {
            const double Radius=FMath::Max(20.,double(MinimumTurnRadiusM));
            const double VTurn=FMath::Sqrt(FMath::Max(0.01f,MaxLateralAccelerationMps2)*Radius);
            const double BeginTurn=Radius*FMath::Tan(FMath::Min(Angle/2.,1.3));
            TargetSpeed=FMath::Min(TargetSpeed,FMath::Sqrt(VTurn*VTurn+2*BrakingDecelerationMps2*FMath::Max(0.,FVector2D::Distance(P,W)-BeginTurn)));
        }
    }
    TargetSpeed*=FMath::Clamp(1.-FMath::Abs(Error)/150.,0.15,1.);
    if(bCollisionDataUncertain)TargetSpeed=FMath::Min(TargetSpeed,4.);
    float Front=FMath::Min3(SensorDistance(0),SensorDistance(30),SensorDistance(-30));
    const double StopDistance=Speed*Speed/(2*FMath::Max(0.01f,BrakingDecelerationMps2))+Speed*2+LocalClearanceMarginM;
    const double FullHorizon=FMath::Clamp(StopDistance+HullRadiusM,80.,600.);
    // A standoff endpoint already has a checked full-hull footprint. Looking
    // through it into the excluded shore made avoidance reject the final safe
    // approach. Keep the independent emergency sensors/braking active.
    const double Horizon=AuthoredStandoff && CurrentWaypointIndex==CurrentRoute.Num()-1?
        FMath::Min(FullHorizon,FMath::Max(RouteArrivalRadius,FVector2D::Distance(P,W))):FullHorizon;
    const FVector Predicted=Hull->GetComponentLocation()+F*Horizon*100;
    const bool SweepBlocked=bUseHullSweep && !SweepClear(Hull->GetComponentLocation(),Predicted,HullHalfWidthM+LocalClearanceMarginM);
    AActor* ForwardObstacle=SweepBlocked?DestinationState(this).SweepActor.Get():nullptr;
    const FVector ObstacleCenter=DestinationState(this).SweepCenter;
    const bool ChartBlocked=!Map->SegmentClear(P,Map->ToMetric(WorldToECEF(Predicted)));
    if(ChartBlocked)
    {
        double Clear=0,Blocked=Horizon;
        for(int32 I=0;I<10;++I){const double M=(Clear+Blocked)/2;const FVector2D Q=Map->ToMetric(WorldToECEF(Hull->GetComponentLocation()+F*M*100));if(Map->SegmentClear(P,Q))Clear=M;else Blocked=M;}
        Front=FMath::Min(Front,float(Clear));
    }
    // A real ship can block otherwise valid chart water. Route around the
    // detected physical hull using the existing temporary-exclusion A*, rather
    // than repeatedly aiming through it after each local steering correction.
    APawn* BlockingPawn=Cast<APawn>(ForwardObstacle);
    auto* BlockingHull=BlockingPawn?UShipCombatComponent::PhysicalHull(BlockingPawn):nullptr;
    const bool PlanForStoppedShip=BlockingPawn && (BlockingPawn->IsPlayerControlled() ||
        (Now>20 && BlockingHull && BlockingHull->GetPhysicsLinearVelocity().Size()<100 && !HasShipRightOfWay(this,BlockingPawn,Now)));
    if(Traffic.TrafficRadius==0 && FMath::Abs(Error)<45 && PlanForStoppedShip)
    {
        const FVector2D Block=Map->ToMetric(WorldToECEF(ObstacleCenter));
        const double Radius=FMath::Max(150.,Map->CellSize*1.5);
        const double StartConnector=FVector2D::Distance(P,Map->Center(Map->CellAt(P)));
        const double GoalConnector=FVector2D::Distance(GoalMetric,Map->Center(Map->CellAt(GoalMetric)));
        const bool AlreadyKnown=FailedCorridors.ContainsByPredicate([&](const FVector2D& E){return FVector2D::Distance(E,Block)<Map->CellSize*.5;});
        if(!AlreadyKnown && FVector2D::Distance(P,Block)>Radius+StartConnector && FVector2D::Distance(GoalMetric,Block)>Radius+GoalConnector)
        {
            FailedCorridors.Add(Block);if(FailedCorridors.Num()>8)FailedCorridors.RemoveAt(0);
            bNeedsPlan=true;NavigationState=EShipNavigationState::Replan;
            if(bDrawDebug)UE_LOG(LogTemp,Display,TEXT("ShipNavigation dynamic obstacle=%s chart=%s; requesting validated A* detour"),*GetNameSafe(ForwardObstacle),*Block.ToString());
            SetDriveCommand(SignedSpeed>.5f && SensorDistance(180)>HullRadiusM+LocalClearanceMarginM?-.12f:0,0);
            return true;
        }
    }
    // The inherited rudder rotates this large hull slowly even with zero forward thrust.
    // Align with the safe route before driving into a blocked heading; don't blacklist a
    // valid corridor just because the bow needs time to turn toward its first waypoint.
    // The inherited rudder also induces lateral drift. Reserve its measured
    // stopping envelope, rather than dropping a safe turn whenever speed passes
    // the arrival threshold. The chart and collision must both cover that disk.
    const double TurnDrift=Speed*Speed/(2*FMath::Max(0.01f,BrakingDecelerationMps2))+Speed*2;
    const double TurnEnvelope=HullRadiusM+LocalClearanceMarginM+TurnDrift;
    const float RouteTurnSide=Error<0?-1.f:1.f;
    auto CanRotateSafely=[&](float Side)
    {
        const FBox Local=Hull->GetLocalBounds().GetBox();
        const FVector Center=Hull->GetComponentTransform().TransformPosition(Local.GetCenter());
        const FVector COM=Hull->GetCenterOfMass();
        const FVector Extent=Local.GetExtent()*Hull->GetComponentScale().GetAbs()+FVector(200);
        const FQuat Delta(Up,FMath::DegreesToRadians(Side*4.f));
        const FVector NextCenter=COM+Delta.RotateVector(Center-COM);
        FCollisionQueryParams Q(SCENE_QUERY_STAT(ShipStationaryTurn),true,GetOwner());NavalWaterQueries::IgnoreWaterActors(GetWorld(),Q);
        if(GetWorld()->OverlapBlockingTestByChannel(NextCenter,Delta*Hull->GetComponentQuat(),ECC_Visibility,FCollisionShape::MakeBox(Extent),Q))return false;
        const FVector2D Drift=Map->ToMetric(WorldToECEF(Hull->GetComponentLocation()+Hull->GetRightVector()*Side*500));
        return Map->SegmentClear(P,Drift);
    };
    // Near a coast the whole turning disk can overlap land while the narrow
    // hull's next rotation is safe. Keep that checked route-directed rotation
    // in the intentional-turn state instead of alternating avoidance/recovery.
    const bool bSafeSlowTurn=((Map->SafetyMargin>=TurnEnvelope || bUsingOperationalChart) &&
        SweepClear(Hull->GetComponentLocation(),Hull->GetComponentLocation(),TurnEnvelope)) ||
        (Speed<1.5 && FMath::Abs(Error)>15 && Map->SafetyMargin>HullRadiusM+5 && CanRotateSafely(RouteTurnSide));
    const bool bPreTurning=FMath::Abs(Error)>15 && (ChartBlocked || SweepBlocked || FMath::Abs(Error)>60) &&
        (Front>StopDistance+10 || bSafeSlowTurn);
    if(bPreTurning)
    {
        // Limit the existing lateral rudder force during low-speed alignment;
        // full rudder otherwise accelerates drift beyond the reserved envelope.
        R=FMath::Clamp(R,-0.6f,0.6f);
        TargetSpeed=0;AvoidElapsed=0;AvoidSide=0;NavigationState=EShipNavigationState::FollowRoute;
    }
    else if(Front<Horizon || SweepBlocked || ChartBlocked)
    {
        bAvoidingObstacle=true;NavigationState=EShipNavigationState::Avoid;AvoidElapsed+=Dt;
        if(AvoidSide==0)
        {
            // Score candidate trajectories against route, chart and sensor; latch chosen side.
            double Best=-DBL_MAX;int32 Side=0;float SelectedAngle=45;
            for(int32 S:{-1,1})for(double A:{30.,60.})
            {
                const FVector Direction=F.RotateAngleAxis(A*S,Up);
                const FVector End=Hull->GetComponentLocation()+Direction*FMath::Max(100.,Horizon)*100;
                const FVector2D EndMetric=Map->ToMetric(WorldToECEF(End));
                const float Clear=SensorDistance(A*S);
                if(Clear<FMath::Max(100.,Horizon) || !Map->SegmentClear(P,EndMetric) ||
                    !SweepClear(Hull->GetComponentLocation(),End,HullHalfWidthM+LocalClearanceMarginM))continue;
                const double Score=FMath::Min(double(Clear),Horizon)-FMath::Abs(FMath::FindDeltaAngleDegrees(Error,float(A*S)))*2;
                if(Score>Best){Best=Score;Side=S;SelectedAngle=A*S;}
            }
            if(Side==0){TimeSinceProgress+=Dt;TargetSpeed=0;R=0;}
            else{AvoidSide=Side;DestinationState(this).AvoidAngle=SelectedAngle;if(LastAvoidSide && LastAvoidSide!=Side)++AvoidSideChanges;LastAvoidSide=Side;}
        }
        if(AvoidSide!=0)
        {
            const float Angle=DestinationState(this).AvoidAngle;
            const FVector Direction=F.RotateAngleAxis(Angle,Up);
            const FVector SafeWorld=Hull->GetComponentLocation()+Direction*FMath::Max(100.,Horizon)*100;
            const FVector2D SafePoint=Map->ToMetric(WorldToECEF(SafeWorld));
            if(Map->SegmentClear(P,SafePoint) && SensorDistance(Angle)>FMath::Max(100.,Horizon) && SweepClear(Hull->GetComponentLocation(),SafeWorld,HullHalfWidthM+LocalClearanceMarginM))
            {R=0.65f*AvoidSide*RudderSign;TargetSpeed=FMath::Min(TargetSpeed,3.);}
            else{TargetSpeed=0;R=0;AvoidSide=0;}
        }
        if((AvoidElapsed>12 && TimeSinceProgress>StuckWindowSeconds) || AvoidSideChanges>=4)
        {
            const FVector2D Block=Map->ToMetric(WorldToECEF(Hull->GetComponentLocation()+F*FMath::Max(50.f,Front)*100));
            // Never exclude the planner's own endpoints (or their grid
            // connectors). Close bow obstacles are a steering/braking problem,
            // not evidence that this valid starting area is impassable.
            const double EndpointClearance=Map->CellSize*(1.5+UE_SQRT_2);
            if(FVector2D::Distance(Block,P)>EndpointClearance && FVector2D::Distance(Block,GoalMetric)>EndpointClearance)
            {FailedCorridors.Add(Block);if(FailedCorridors.Num()>8)FailedCorridors.RemoveAt(0);}
            bNeedsPlan=true;AvoidElapsed=0;AvoidSide=0;AvoidSideChanges=0;LastAvoidSide=0;++RecoveryAttemptCount;
            if(RecoveryAttemptCount>MaxRecoveryAttempts)Fail(TEXT("Local avoidance/replan budget exhausted"));
        }
    }
    else
    {if(AvoidElapsed>0)AvoidElapsed=FMath::Max(0.f,AvoidElapsed-Dt);if(AvoidElapsed==0){AvoidSide=0;AvoidSideChanges=0;LastAvoidSide=0;}NavigationState=EShipNavigationState::FollowRoute;}
    DesiredSpeed=TargetSpeed;
    DesiredSpeed=FMath::Min(DesiredSpeed,float(FMath::Sqrt(2.*BrakingDecelerationMps2*FMath::Max(0.f,Front-LocalClearanceMarginM))));
    TargetSpeed=DesiredSpeed;
    // Yield with braking only. A* and the existing swept, water-validated local
    // avoidance remain the sole steering authority; separation never steers into land.
    bYieldingToShip=false;
    bool bYieldingToPlayer=false;
    if(SafeShipSeparationM>0)
    {
        for(TActorIterator<APawn> Other(GetWorld());Other;++Other)
        {
            if(*Other==GetOwner() || !Other->FindComponentByClass<UShipWeaponComponent>() || Other->FindComponentByClass<UShipWeaponComponent>()->bIsDestroyed)continue;
            auto* OtherHull=UShipCombatComponent::PhysicalHull(*Other);if(!OtherHull)continue;
            const FVector Delta=OtherHull->GetComponentLocation()-Hull->GetComponentLocation();
            const FVector Flat=FVector::VectorPlaneProject(Delta,Hull->GetUpVector());
            const double Distance=Flat.Size()/100;
            if(Distance>=SafeShipSeparationM || FVector::DotProduct(Flat.GetSafeNormal(),Forward)<.25)continue;
            // Stable right of way prevents reciprocal soft-braking deadlocks.
            // The other vessel yields; the priority vessel still uses every
            // existing hull sweep, land check and emergency brake below.
            const auto* OtherNav=Other->FindComponentByClass<UShipEnemyNavigationComponent>();
            const bool Following=OtherNav && IsValid(OtherNav->Heading) &&
                FVector::DotProduct(Forward,OtherNav->Heading->GetForwardVector())>.5f;
            if(!Following && HasShipRightOfWay(this,*Other,Now))continue;
            const double Closing=FVector::DotProduct(Velocity-OtherHull->GetPhysicsLinearVelocity(),Flat.GetSafeNormal())/100;
            if(Closing>.2 || Distance<SafeShipSeparationM*.85)
            {
                TargetSpeed=FMath::Min(TargetSpeed,FMath::Max(0.,(Distance-FMath::Max(180.,double(SafeShipSeparationM)*.65))*.04));bYieldingToShip=true;
                bYieldingToPlayer|=Other->IsPlayerControlled();
                // Brake without replacing the validated route's rudder command.
                // Re-selecting a relative +/-60 degree heading here countersteered
                // against the A* detour and trapped ships beside a stationary pawn.
                // A safe tangential pass must not be clamped to zero merely
                // because center distance is inside the soft separation circle.
                const FVector Ahead=Hull->GetComponentLocation()+F*22000;
                if(FVector::DotProduct(F,Flat.GetSafeNormal())<.75 && Map->SegmentClear(P,Map->ToMetric(WorldToECEF(Ahead))) &&
                    SweepClear(Hull->GetComponentLocation(),Ahead,HullHalfWidthM+LocalClearanceMarginM))
                    TargetSpeed=FMath::Max(TargetSpeed,FMath::Min(3.,double(CruiseSpeedMps)));
            }
        }
        DesiredSpeed=TargetSpeed;
        if(bYieldingToShip){TimeSinceProgress=0;BestRemainingDistance=Remaining;}
    }
    {
        auto& Track=DestinationState(this);Track.bYieldingToPlayer=bYieldingToPlayer;
        if(bYieldingToShip && TargetSpeed<=3.01 && Track.PriorityUntil<=Now)
        {if(Track.YieldSince<0)Track.YieldSince=Now;}
        else Track.YieldSince=-1;
    }
    // Existing thrust coefficient remains in BP. This bounded speed controller only requests commands.
    T=FMath::Clamp(float((TargetSpeed-SignedSpeed)*0.08+TargetSpeed/FMath::Max(CruiseSpeedMps,1.f)*0.12),-0.2f,MaxForwardThrottle);
    if(TargetSpeed<=0.01)T=SignedSpeed>0.5f?-0.12f:0;
    if(Remaining<BestRemainingDistance-5)
    {
        if(BestRemainingDistance!=DBL_MAX)SuccessfulProgressSinceRecovery+=BestRemainingDistance-Remaining;
        BestRemainingDistance=Remaining;TimeSinceProgress=0;
        if(SuccessfulProgressSinceRecovery>FMath::Max(100.f,ArrivalRadiusM*2)){RecoveryAttemptCount=0;AvoidSideChanges=0;SuccessfulProgressSinceRecovery=0;}
    }
    else if(TargetSpeed>0.5 || bAvoidingObstacle || bPreTurning)TimeSinceProgress+=Dt;
    // Allow an intentional slow physical turn; still bound turns that never produce route progress.
    const float ProgressTimeout=bPreTurning?FMath::Clamp(StuckWindowSeconds*20,120.f,300.f):StuckWindowSeconds;
    if(TimeSinceProgress>ProgressTimeout && (FMath::Abs(YawRate)<(bPreTurning?0.05:0.5) || TimeSinceProgress>ProgressTimeout*2))
    {bStuck=true;RecoveryPhase=1;RecoveryElapsed=0;RecoveryStart=P;SuccessfulProgressSinceRecovery=0;T=0;R=0;}
    // Emergency brake overrides all lower-priority forward movement. Rear sensor guards reverse thrust.
    const bool bEmergency=Front<LocalClearanceMarginM+Speed*2;
    if(bDrawDebug && Now-DestinationState(this).LastTelemetry>=1.)
        UE_LOG(LogTemp,Display,TEXT("ShipNavigation STEER ship=%s error=%.1f front=%.1fm horizon=%.1fm chart=%d sweep=%d turn=%d envelope=%.1fm speed=%.2f signed=%.2f avoidSide=%d avoidTime=%.1f recovery=%d"),
            *GetOwner()->GetActorNameOrLabel(),Error,Front,Horizon,ChartBlocked,SweepBlocked,bPreTurning,TurnEnvelope,Speed,SignedSpeed,AvoidSide,AvoidElapsed,RecoveryAttemptCount);
    if(bEmergency || NavigationState==EShipNavigationState::Failed)
    {
        T=SignedSpeed>0.5f && SensorDistance(180)>HullRadiusM+LocalClearanceMarginM?-0.15f:0;
        if(!(bPreTurning && bSafeSlowTurn))R=0;
        if(!(bPreTurning && bSafeSlowTurn) && Speed<1.5f && NavigationState!=EShipNavigationState::Failed && Map->SafetyMargin>HullRadiusM+5)
        {
            // A full-turn inflated square can overlap a nearby ship even when
            // a small rotation of the actual narrow hull is safe. Check that
            // next rotation explicitly instead of locking both rudders forever.
            for(float Side:{RouteTurnSide,-RouteTurnSide})
            {
                if(!CanRotateSafely(Side))continue;
                T=0;R=.45f*Side*RudderSign;break;
            }
        }
    }
    if(T<0 && SensorDistance(180)<HullRadiusM+LocalClearanceMarginM)T=0;
    if(NavigationState==EShipNavigationState::Failed){T=0;R=0;}
    float OutputThrottle=FMath::FInterpConstantTo(DesiredThrottle,T,Dt,ThrottleRatePerSecond);
    if(bEmergency)OutputThrottle=FMath::Min(OutputThrottle,0.f);
    SetDriveCommand(OutputThrottle,FMath::FInterpConstantTo(DesiredRudder,R,Dt,RudderRatePerSecond));
    UpdateWeapon(Dt);
    return true;
}

void UShipEnemyNavigationComponent::OnOwnerDamaged(AActor*,float Damage,const UDamageType*,AController*,AActor* Causer)
{
    if(GetOwner()->FindComponentByClass<UShipWeaponComponent>())return; // Shared health applies team validation once.
    if(bDead || Damage<=0 || (Causer && (Causer==GetOwner() || Causer->GetOwner()==GetOwner())))return;
    Health=FMath::Max(0.f,Health-Damage);
    if(Health<=0){bDead=true;NavigationState=EShipNavigationState::Dead;CurrentRoute.Reset();bNeedsPlan=false;DetectedPlayer=nullptr;RecoveryPhase=0;bOwnsCommands=true;SetDriveCommand(0,0);}
}

void UShipEnemyNavigationComponent::UpdateWeapon(float)
{
    // The shared BP weapon and independent animated turret own combat when installed.
    // Preserve the legacy fallback only for actors without the new component.
    if(GetOwner()->FindComponentByClass<UShipCombatComponent>())return;
    if(bDead || Role==EShipEnemyRole::Passive || Role==EShipEnemyRole::Coward || !IsValid(DetectedPlayer)){WeaponStatus=TEXT("No engagement");return;}
    if(!VisualMesh || !VisualMesh->DoesSocketExist(MuzzleSocket)){WeaponStatus=TEXT("Missing verified muzzle socket");return;}
    APawn* Pawn=Cast<APawn>(GetOwner());if(!Pawn)return;
    if(!Pawn->GetController())
    {
        // Demonstrated need: inherited ABP_Ship aims from GetControlRotation. No new controller class.
        OwnedAimController=GetWorld()->SpawnActor<AAIController>();if(OwnedAimController)OwnedAimController->Possess(Pawn);
    }
    const FVector Start=VisualMesh->GetSocketLocation(MuzzleSocket);
    const FVector Aim=DetectedPlayer->GetActorLocation()+DetectedPlayer->GetVelocity()*0.5;
    FVector Launch;
    UGameplayStatics::FSuggestProjectileVelocityParameters Ballistic(this,Start,Aim,MuzzleSpeedCmps);
    Ballistic.CollisionRadius=12.5f;Ballistic.TraceOption=ESuggestProjVelocityTraceOption::DoNotTrace;
    if(!UGameplayStatics::SuggestProjectileVelocity(Ballistic,Launch))
    {WeaponStatus=TEXT("No ballistic solution");return;}
    const FRotator AimRotation=Launch.Rotation();
    if(Pawn->GetController())Pawn->GetController()->SetControlRotation(AimRotation);
    const float RelativePitch=FMath::FindDeltaAngleDegrees(Hull->GetComponentRotation().Pitch,AimRotation.Pitch);
    if(RelativePitch< -10 || RelativePitch>40){WeaponStatus=TEXT("Outside inherited gun elevation limits");return;}
    if(FVector::DotProduct(VisualMesh->GetSocketRotation(MuzzleSocket).Vector(),Launch.GetSafeNormal())<0.995)
    {WeaponStatus=TEXT("Aiming inherited animation");return;}
    if(GetWorld()->GetTimeSeconds()-LastShotTime<FireCooldownSeconds){WeaponStatus=TEXT("Cooldown");return;}
    if(bCollisionDataUncertain){WeaponStatus=TEXT("Collision coverage uncertain; fire withheld");return;}
    // Validate the entire ballistic arc, not merely a straight visibility ray.
    FPredictProjectilePathParams Params(12.5f,Start,Launch,15,ECC_Visibility,GetOwner());Params.bTraceComplex=true;Params.SimFrequency=30;
    FPredictProjectilePathResult Result;UGameplayStatics::PredictProjectilePath(this,Params,Result);
    if(!Result.HitResult.bBlockingHit || Result.HitResult.GetActor()!=DetectedPlayer)
    {WeaponStatus=TEXT("Ballistic path obstructed or misses target");return;}
    if(FVector::Dist(Result.HitResult.ImpactPoint,Hull->GetComponentLocation())<HullRadiusM*100+1500)
    {WeaponStatus=TEXT("Unsafe self blast proximity");return;}
    for(TActorIterator<APawn> I(GetWorld());I;++I)
        if(*I!=Pawn && *I!=DetectedPlayer && I->FindComponentByClass<UShipEnemyNavigationComponent>() &&
           FVector::Dist(I->GetActorLocation(),Result.HitResult.ImpactPoint)<1500)
        {WeaponStatus=TEXT("Friendly in blast radius");return;}
    if(!ProjectileClass){WeaponStatus=TEXT("No enemy projectile class");return;}
    const FTransform Spawn(Launch.Rotation(),Start);
    AShipEnemyProjectile* Projectile=GetWorld()->SpawnActorDeferred<AShipEnemyProjectile>(ProjectileClass,Spawn,GetOwner(),Pawn,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if(Projectile){Projectile->LaunchVelocity=Launch;UGameplayStatics::FinishSpawningActor(Projectile,Spawn);LastShotTime=GetWorld()->GetTimeSeconds();WeaponStatus=TEXT("Fired verified ballistic trajectory");}
}
