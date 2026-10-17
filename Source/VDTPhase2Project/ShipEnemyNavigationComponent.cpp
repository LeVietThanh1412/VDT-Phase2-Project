#include "ShipEnemyNavigationComponent.h"
#include "ShipEnemyProjectile.h"
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
template<class T> T* NamedComponent(AActor* A,const TCHAR* Name)
{TArray<T*> Cs;A->GetComponents<T>(Cs);for(T* C:Cs)if(C->GetFName()==FName(Name))return C;return nullptr;}
FNumericProperty* Numeric(UObject* O,const TCHAR* N)
{return CastField<FNumericProperty>(O->GetClass()->FindPropertyByName(FName(N)));}
FVector2D Rotate2D(const FVector2D& V,double Degrees)
{const double A=FMath::DegreesToRadians(Degrees);return FVector2D(V.X*FMath::Cos(A)-V.Y*FMath::Sin(A),V.X*FMath::Sin(A)+V.Y*FMath::Cos(A));}
void IgnoreWater(UWorld* W,FCollisionQueryParams& Q)
{for(TActorIterator<AActor> I(W);I;++I){const FString N=I->GetClass()->GetName();if(N.StartsWith(TEXT("WaterBody")) || N==TEXT("WaterZone"))Q.AddIgnoredActor(*I);}}
}

UShipEnemyNavigationComponent::UShipEnemyNavigationComponent()
{PrimaryComponentTick.bCanEverTick=false;ProjectileClass=AShipEnemyProjectile::StaticClass();}

void UShipEnemyNavigationComponent::BeginPlay()
{
    Super::BeginPlay();Health=FMath::Max(MaxHealth,1.f);
    GetOwner()->OnTakeAnyDamage.AddUniqueDynamic(this,&UShipEnemyNavigationComponent::OnOwnerDamaged);
    // No force application and no actor transforms in this component.
}
void UShipEnemyNavigationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Georeference)Georeference->OnGeoreferenceUpdated.RemoveAll(this);
    if(GetOwner())GetOwner()->OnTakeAnyDamage.RemoveAll(this);
    if(OwnedAimController)OwnedAimController->Destroy();
    Map.Reset();Super::EndPlay(Reason);
}
bool UShipEnemyNavigationComponent::InitializeNavigation()
{
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
{LastNavigationFailureReason=S;NavigationState=EShipNavigationState::Failed;bPathfindingSucceeded=false;DesiredSpeed=0;}
void UShipEnemyNavigationComponent::SetDriveCommand(float T,float R)
{
    if(!bOwnsCommands)return;
    DesiredThrottle=FMath::IsFinite(T)?FMath::Clamp(T,-1.f,1.f):0;
    DesiredRudder=FMath::IsFinite(R)?FMath::Clamp(R,-1.f,1.f):0;
    if(ThrottleProperty)ThrottleProperty->SetFloatingPointPropertyValue(ThrottleProperty->ContainerPtrToValuePtr<void>(GetOwner()),DesiredThrottle);
    if(RudderProperty)RudderProperty->SetFloatingPointPropertyValue(RudderProperty->ContainerPtrToValuePtr<void>(GetOwner()),DesiredRudder);
}
void UShipEnemyNavigationComponent::SetGeographicDestination(FVector LLH)
{
    CurrentFinalDestination=LLH;NavigationMode=EShipNavigationMode::Destination;bHasGoal=true;bExplicitStop=false;
    bNeedsPlan=true;LastGoal=FVector2D(DBL_MAX,DBL_MAX);CurrentRoute.Reset();RecoveryAttemptCount=0;RecoveryPhase=0;
    NavigationState=EShipNavigationState::Replan;LastNavigationFailureReason.Empty();
}
void UShipEnemyNavigationComponent::StartPatrol()
{NavigationMode=EShipNavigationMode::Patrol;bHasGoal=false;bNeedsPlan=true;bExplicitStop=false;RecoveryAttemptCount=0;RecoveryPhase=0;NavigationState=EShipNavigationState::Patrol;}
void UShipEnemyNavigationComponent::RequestReplan()
{bNeedsPlan=true;bExplicitStop=false;RecoveryAttemptCount=0;RecoveryPhase=0;FailedCorridors.Reset();NavigationState=EShipNavigationState::Replan;LastNavigationFailureReason.Empty();}
void UShipEnemyNavigationComponent::StopNavigation()
{bHasGoal=false;bNeedsPlan=false;bExplicitStop=true;CurrentRoute.Reset();NavigationState=EShipNavigationState::Idle;DesiredSpeed=0;}
bool UShipEnemyNavigationComponent::PlanRoute(const FVector2D& Position,const FVector2D& Goal)
{
    const auto R=Map->FindPath(Position,Goal,MaxSearchExpansions,MaxSearchMilliseconds,FailedCorridors);
    LastSearchExpansions=R.Expansions;LastSearchMilliseconds=R.Milliseconds;
    LastPlanTime=GetWorld()->GetTimeSeconds();bNeedsPlan=false;CurrentRoute.Reset();
    if(!R.bSuccess){Fail(R.Failure);return false;}
    for(const auto& P:R.Points)CurrentRoute.Add(FVector(P.X,P.Y,0));
    CurrentWaypointIndex=FMath::Min(1,CurrentRoute.Num()-1);LastGoal=Goal;
    bPathfindingSucceeded=true;LastNavigationFailureReason.Empty();TimeSinceProgress=0;BestRemainingDistance=DBL_MAX;
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
    FCollisionQueryParams Q(SCENE_QUERY_STAT(EnemyNavigationSweep),true,GetOwner());IgnoreWater(GetWorld(),Q);
    FHitResult Hit;return !GetWorld()->SweepSingleByChannel(Hit,A,B,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(FMath::Max(1.f,Radius*100.f)),Q);
}
bool UShipEnemyNavigationComponent::CollisionCoverageAvailable() const
{
    for(TActorIterator<AActor> I(GetWorld());I;++I)
    {
        if(!I->GetClass()->GetName().Contains(TEXT("Cesium3DTileset")))continue;
        TArray<UPrimitiveComponent*> Cs;I->GetComponents(Cs);
        for(auto* C:Cs)if(C->GetClass()->GetName().Contains(TEXT("CesiumGltf")) && C->IsQueryCollisionEnabled() &&
            C->Bounds.GetBox().ComputeSquaredDistanceToPoint(Hull->GetComponentLocation())<FMath::Square(80000.))return true;
    }
    return false;
}
void UShipEnemyNavigationComponent::UpdatePerception()
{
    bCollisionDataUncertain=!CollisionCoverageAvailable();
    APawn* Candidate=UGameplayStatics::GetPlayerPawn(this,0);
    const double Now=GetWorld()->GetTimeSeconds();
    if(!Candidate || Candidate==GetOwner() || !Candidate->IsPlayerControlled() || (PlayerShipClass && !Candidate->IsA(PlayerShipClass)))
    {DetectedPlayer=nullptr;return;}
    const float D=FVector::Dist2D(Hull->GetComponentLocation(),Candidate->GetActorLocation())/100;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(EnemyShipPerception),true,GetOwner());IgnoreWater(GetWorld(),Q);
    FHitResult Hit;const FVector A=Hull->GetComponentLocation()+FVector(0,0,1500),B=Candidate->GetActorLocation()+FVector(0,0,1500);
    const bool Visible=!GetWorld()->LineTraceSingleByChannel(Hit,A,B,ECC_Visibility,Q) || Hit.GetActor()==Candidate;
    if(D<=DetectionRadiusM && Visible){DetectedPlayer=Candidate;LastSeenTime=Now;}
    else if(D>FMath::Max(LoseTargetRadiusM,DetectionRadiusM+100) || Now-LastSeenTime>5 || !IsValid(DetectedPlayer))DetectedPlayer=nullptr;
}
void UShipEnemyNavigationComponent::SelectObjective(const FVector2D& P)
{
    if(IsValid(DetectedPlayer) && Role!=EShipEnemyRole::Passive)
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
        CurrentFinalDestination=Georeference->GetEllipsoid()->EllipsoidCenteredEllipsoidFixedToLongitudeLatitudeHeight(Map->ToECEF(GoalMetric));
        return;
    }
    if(NavigationMode==EShipNavigationMode::Patrol)
    {
        const AActor* Assigned=bTargetB?PatrolActorB.Get():PatrolActorA.Get();
        const FVector LLH=IsValid(Assigned)?WorldToGeographic(Assigned->GetActorLocation()):(bTargetB?PatrolPointB:PatrolPointA);
        const FVector2D Target=LLHToMetric(LLH);
        if(!bHasGoal || !Target.Equals(GoalMetric,1)){GoalMetric=Target;CurrentFinalDestination=LLH;bNeedsPlan=true;bHasGoal=true;ArrivalTime=-1;}
    }
    else if(bHasGoal)GoalMetric=LLHToMetric(CurrentFinalDestination);
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
    if(!bNewNavigationEnabled && !bDead){bOwnsCommands=false;return false;}
    bOwnsCommands=true;
    if(!bInitialized && !InitializeNavigation()){SetDriveCommand(0,0);return true;}
    if(bDead){NavigationState=EShipNavigationState::Dead;SetDriveCommand(0,0);return true;}
    if(bExplicitStop){NavigationState=EShipNavigationState::Idle;SetDriveCommand(0,0);return true;}
    if(!FMath::IsFinite(Dt) || Dt<=0){SetDriveCommand(0,0);return true;}
    Dt=FMath::Min(Dt,0.2f);
    const FVector2D P=PositionMetric();
    if(bOriginChanged || LastWorldOrigin!=GetWorld()->OriginLocation)
    {bOriginChanged=false;LastWorldOrigin=GetWorld()->OriginLocation;TimeSinceProgress=0;ProgressPosition=P;BestRemainingDistance=DBL_MAX;SetDriveCommand(0,0);return true;}
    PerceptionClock-=Dt;if(PerceptionClock<=0){PerceptionClock=0.5;UpdatePerception();}
    if(NavigationState==EShipNavigationState::Failed && !bNeedsPlan){SetDriveCommand(0,0);return true;}
    SelectObjective(P);
    if(NavigationState==EShipNavigationState::Failed){SetDriveCommand(0,0);return true;}
    if(!bHasGoal){NavigationState=EShipNavigationState::Idle;SetDriveCommand(0,0);return true;}
    const double Now=GetWorld()->GetTimeSeconds();
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
    const double GoalDistance=FVector2D::Distance(P,GoalMetric);
    if(GoalDistance<=ArrivalRadiusM && Map->SegmentClear(P,GoalMetric))
    {
        DesiredSpeed=0;T=SignedSpeed>0.5f?-0.12f:SignedSpeed<-.5f?0.12f:0;
        NavigationState=EShipNavigationState::Arrived;
        if(Speed<0.5f)
        {
            if(ArrivalTime<0)ArrivalTime=Now;
            if(NavigationMode==EShipNavigationMode::Patrol && (Role==EShipEnemyRole::Passive || !IsValid(DetectedPlayer)) && Now-ArrivalTime>=PatrolWaitSeconds)
            {bTargetB=!bTargetB;++PatrolLegsCompleted;bHasGoal=false;bNeedsPlan=true;RecoveryAttemptCount=0;FailedCorridors.Reset();}
        }
        SetDriveCommand(FMath::FInterpConstantTo(DesiredThrottle,T,Dt,ThrottleRatePerSecond),FMath::FInterpConstantTo(DesiredRudder,0,Dt,RudderRatePerSecond));UpdateWeapon(Dt);return true;
    }
    if(!CurrentRoute.IsValidIndex(CurrentWaypointIndex)){Fail(TEXT("Missing current route waypoint"));SetDriveCommand(0,0);return true;}
    auto WP=[&](int32 I){return FVector2D(CurrentRoute[I].X,CurrentRoute[I].Y);};
    const double LookAhead=FMath::Clamp(double(Speed)*12.,80.,300.);
    while(CurrentWaypointIndex<CurrentRoute.Num()-1)
    {
        const FVector2D Current=WP(CurrentWaypointIndex),Next=WP(CurrentWaypointIndex+1);
        if(FVector2D::Distance(P,Current)<LookAhead && Map->SegmentClear(P,Next))++CurrentWaypointIndex;else break;
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
    TargetSpeed=FMath::Min(TargetSpeed,FMath::Sqrt(2.*FMath::Max(0.01f,BrakingDecelerationMps2)*FMath::Max(0.,Remaining-ArrivalRadiusM)));
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
    const double Horizon=FMath::Clamp(StopDistance+HullRadiusM,80.,600.);
    const FVector Predicted=Hull->GetComponentLocation()+F*Horizon*100;
    const bool SweepBlocked=bUseHullSweep && !SweepClear(Hull->GetComponentLocation(),Predicted,HullHalfWidthM+LocalClearanceMarginM);
    const bool ChartBlocked=!Map->SegmentClear(P,Map->ToMetric(WorldToECEF(Predicted)));
    if(ChartBlocked)
    {
        double Clear=0,Blocked=Horizon;
        for(int32 I=0;I<10;++I){const double M=(Clear+Blocked)/2;const FVector2D Q=Map->ToMetric(WorldToECEF(Hull->GetComponentLocation()+F*M*100));if(Map->SegmentClear(P,Q))Clear=M;else Blocked=M;}
        Front=FMath::Min(Front,float(Clear));
    }
    if(Front<Horizon || SweepBlocked || ChartBlocked)
    {
        bAvoidingObstacle=true;NavigationState=EShipNavigationState::Avoid;AvoidElapsed+=Dt;
        if(AvoidSide==0)
        {
            // Score candidate trajectories against route, chart and sensor; latch chosen side.
            double Best=-DBL_MAX;int32 Side=0;
            for(int32 S:{-1,1})for(double A:{30.,60.})
            {
                const FVector Direction=F.RotateAngleAxis(A*S,Up);
                const FVector End=Hull->GetComponentLocation()+Direction*FMath::Max(100.,Horizon)*100;
                const FVector2D EndMetric=Map->ToMetric(WorldToECEF(End));
                const float Clear=SensorDistance(A*S);
                if(Clear<80 || !Map->SegmentClear(P,EndMetric))continue;
                const double Score=FMath::Min(double(Clear),Horizon)-FMath::Abs(FMath::FindDeltaAngleDegrees(Error,float(A*S)))*2;
                if(Score>Best){Best=Score;Side=S;}
            }
            if(Side==0){TimeSinceProgress+=Dt;TargetSpeed=0;R=0;}
            else{AvoidSide=Side;if(LastAvoidSide && LastAvoidSide!=Side)++AvoidSideChanges;LastAvoidSide=Side;}
        }
        if(AvoidSide!=0)
        {
            const FVector Direction=F.RotateAngleAxis(45.f*AvoidSide,Up);
            const FVector2D SafePoint=Map->ToMetric(WorldToECEF(Hull->GetComponentLocation()+Direction*10000));
            if(Map->SegmentClear(P,SafePoint) && SensorDistance(45.f*AvoidSide)>100 && SweepClear(Hull->GetComponentLocation(),MetricToWorld(SafePoint),HullHalfWidthM))
            {R=0.65f*AvoidSide*RudderSign;TargetSpeed=FMath::Min(TargetSpeed,3.);}
            else{TargetSpeed=0;R=0;}
        }
        if(AvoidElapsed>12 || AvoidSideChanges>=4)
        {
            const FVector2D Block=Map->ToMetric(WorldToECEF(Hull->GetComponentLocation()+F*FMath::Max(50.f,Front)*100));
            FailedCorridors.Add(Block);if(FailedCorridors.Num()>8)FailedCorridors.RemoveAt(0);
            bNeedsPlan=true;AvoidElapsed=0;AvoidSide=0;++RecoveryAttemptCount;
            if(RecoveryAttemptCount>MaxRecoveryAttempts)Fail(TEXT("Local avoidance/replan budget exhausted"));
        }
    }
    else
    {if(AvoidElapsed>0)AvoidElapsed=FMath::Max(0.f,AvoidElapsed-Dt);if(AvoidElapsed==0)AvoidSide=0;NavigationState=EShipNavigationState::FollowRoute;}
    DesiredSpeed=TargetSpeed;
    DesiredSpeed=FMath::Min(DesiredSpeed,float(FMath::Sqrt(2.*BrakingDecelerationMps2*FMath::Max(0.f,Front-LocalClearanceMarginM))));
    TargetSpeed=DesiredSpeed;
    // Existing thrust coefficient remains in BP. This bounded speed controller only requests commands.
    T=FMath::Clamp(float((TargetSpeed-SignedSpeed)*0.08+TargetSpeed/FMath::Max(CruiseSpeedMps,1.f)*0.12),-0.2f,MaxForwardThrottle);
    if(TargetSpeed<=0.01)T=SignedSpeed>0.5f?-0.12f:0;
    if(Remaining<BestRemainingDistance-5)
    {
        if(BestRemainingDistance!=DBL_MAX)SuccessfulProgressSinceRecovery+=BestRemainingDistance-Remaining;
        BestRemainingDistance=Remaining;TimeSinceProgress=0;
        if(SuccessfulProgressSinceRecovery>FMath::Max(100.f,ArrivalRadiusM*2)){RecoveryAttemptCount=0;AvoidSideChanges=0;SuccessfulProgressSinceRecovery=0;}
    }
    else if(TargetSpeed>0.5 || bAvoidingObstacle)TimeSinceProgress+=Dt;
    // Allow an intentional slow physical turn; still bound turns that never produce route progress.
    if(TimeSinceProgress>StuckWindowSeconds && (FMath::Abs(YawRate)<0.5 || TimeSinceProgress>StuckWindowSeconds*2))
    {bStuck=true;RecoveryPhase=1;RecoveryElapsed=0;RecoveryStart=P;SuccessfulProgressSinceRecovery=0;T=0;R=0;}
    // Emergency brake overrides all lower-priority forward movement. Rear sensor guards reverse thrust.
    const bool bEmergency=Front<LocalClearanceMarginM+Speed*2;
    if(bEmergency || NavigationState==EShipNavigationState::Failed)
    {T=SignedSpeed>0.5f && SensorDistance(180)>HullRadiusM+LocalClearanceMarginM?-0.15f:0;R=0;}
    if(T<0 && SensorDistance(180)<HullRadiusM+LocalClearanceMarginM)T=0;
    if(NavigationState==EShipNavigationState::Failed){T=0;R=0;}
    float OutputThrottle=FMath::FInterpConstantTo(DesiredThrottle,T,Dt,ThrottleRatePerSecond);
    if(bEmergency)OutputThrottle=FMath::Min(OutputThrottle,0.f);
    SetDriveCommand(OutputThrottle,FMath::FInterpConstantTo(DesiredRudder,R,Dt,RudderRatePerSecond));
    UpdateWeapon(Dt);
    if(bDrawDebug)
    {
        for(int32 I=0;I<CurrentRoute.Num()-1;++I)DrawDebugLine(GetWorld(),MetricToWorld(WP(I))+FVector(0,0,300),MetricToWorld(WP(I+1))+FVector(0,0,300),FColor::Cyan,false,0.1,0,15);
        DrawDebugDirectionalArrow(GetWorld(),Hull->GetComponentLocation(),TargetWorld,FMath::Min(500.,(TargetWorld-Hull->GetComponentLocation()).Size()/5),bAvoidingObstacle?FColor::Orange:FColor::Green,false,0.1,0,10);
        if(bDrawLandGrid)
        {const int32 C=Map->CellAt(P);if(C!=INDEX_NONE)for(int32 Y=-10;Y<=10;++Y)for(int32 X=-10;X<=10;++X){const int32 CX=C%Map->Width+X,CY=C/Map->Width+Y;if(!Map->IsWater(CX,CY) && CX>=0 && CY>=0 && CX<Map->Width && CY<Map->Height)DrawDebugBox(GetWorld(),MetricToWorld(Map->Center(CY*Map->Width+CX)),FVector(Map->CellSize*45,Map->CellSize*45,100),FColor::Red,false,0.1);}}
    }
    return true;
}

void UShipEnemyNavigationComponent::OnOwnerDamaged(AActor*,float Damage,const UDamageType*,AController*,AActor* Causer)
{
    if(bDead || Damage<=0 || (Causer && (Causer==GetOwner() || Causer->GetOwner()==GetOwner())))return;
    Health=FMath::Max(0.f,Health-Damage);
    if(Health<=0){bDead=true;NavigationState=EShipNavigationState::Dead;CurrentRoute.Reset();bNeedsPlan=false;DetectedPlayer=nullptr;RecoveryPhase=0;bOwnsCommands=true;SetDriveCommand(0,0);}
}

void UShipEnemyNavigationComponent::UpdateWeapon(float)
{
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
