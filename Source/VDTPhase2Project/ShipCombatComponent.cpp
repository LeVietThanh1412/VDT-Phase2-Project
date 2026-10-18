#include "ShipCombatComponent.h"
#include "ShipEnemyNavigationComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
void IgnoreWater(UWorld* World,FCollisionQueryParams& Query)
{
    for(TActorIterator<AActor> It(World);It;++It)
        if(It->GetClass()->GetName().StartsWith(TEXT("WaterBody")) || It->GetClass()->GetName()==TEXT("WaterZone"))Query.AddIgnoredActor(*It);
}
bool CanShoot(AActor* Ship)
{
    const auto* P=FindFProperty<FBoolProperty>(Ship->GetClass(),TEXT("CanShoot"));
    return P && P->GetPropertyValue_InContainer(Ship);
}
void SetCanShoot(AActor* Ship,bool Value)
{
    if(auto* P=FindFProperty<FBoolProperty>(Ship->GetClass(),TEXT("CanShoot")))P->SetPropertyValue_InContainer(Ship,Value);
}
FVector VelocityOf(AActor* Ship)
{
    if(auto* Hull=UShipCombatComponent::PhysicalHull(Ship))return Hull->GetPhysicsLinearVelocity();
    return Ship?Ship->GetVelocity():FVector::ZeroVector;
}
}

UShipWeaponComponent::UShipWeaponComponent(){PrimaryComponentTick.bCanEverTick=false;}
USkeletalMeshComponent* UShipWeaponComponent::Mesh() const
{
    TArray<USkeletalMeshComponent*> Components;GetOwner()->GetComponents(Components);
    for(auto* C:Components)if(C->GetFName()==TEXT("Ship"))return C;
    return nullptr;
}
float UShipWeaponComponent::CooldownRemaining() const
{return GetWorld()?FMath::Max(0.,LastShotTime+ShotCooldownSeconds-GetWorld()->GetTimeSeconds()):0;}
bool UShipWeaponComponent::FirePlayer()
{
    bool Hit=false;FVector Location=FVector::ZeroVector;
    if(const auto* P=FindFProperty<FBoolProperty>(GetOwner()->GetClass(),TEXT("Is Hit")))Hit=P->GetPropertyValue_InContainer(GetOwner());
    if(const auto* P=FindFProperty<FStructProperty>(GetOwner()->GetClass(),TEXT("Impact Point")))Location=*P->ContainerPtrToValuePtr<FVector>(GetOwner());
    return TryFireWeapon(Location,nullptr,FVector::ZeroVector,Hit);
}
bool UShipWeaponComponent::TryFireWeapon(FVector Location,AActor* TargetActor,FVector OptionalLaunchVelocity,bool HasLaunch)
{
    RejectionReason.Empty();
    USkeletalMeshComponent* Visual=Mesh();
    if(!Visual || !Visual->DoesSocketExist(MuzzleSocket)){RejectionReason=TEXT("Missing muzzle");return false;}
    if(bDispatching || CooldownRemaining()>0 || !CanShoot(GetOwner())){RejectionReason=TEXT("Cooldown/recoil");return false;}
    FVector Launch=OptionalLaunchVelocity;
    const FVector Start=Visual->GetSocketLocation(MuzzleSocket);
    if(HasLaunch && Launch.IsNearlyZero())
    {
        UGameplayStatics::FSuggestProjectileVelocityParameters P(this,Start,Location,MuzzleSpeedCmps);
        P.TraceOption=ESuggestProjVelocityTraceOption::DoNotTrace;
        if(!UGameplayStatics::SuggestProjectileVelocity(P,Launch)){RejectionReason=TEXT("No ballistic solution");return false;}
    }
    if(!HasLaunch)Launch=Visual->GetSocketRotation(MuzzleSocket).Vector()*MuzzleSpeedCmps;
    if(Launch.ContainsNaN() || Launch.IsNearlyZero()){RejectionReason=TEXT("Invalid launch velocity");return false;}
    if(TargetActor && FVector::DotProduct(Visual->GetSocketRotation(MuzzleSocket).Vector(),Launch.GetSafeNormal())<FMath::Cos(FMath::DegreesToRadians(1.5)))
    {RejectionReason=TEXT("Muzzle not aligned");return false;}
    UFunction* Event=GetOwner()->FindFunction(TEXT("ExecuteWeaponShot"));
    if(!Event){RejectionReason=TEXT("Shared Blueprint firing event missing");return false;}
    FStructOnScope Parameters(Event);
    auto* Velocity=FindFProperty<FStructProperty>(Event,TEXT("LaunchVelocity"));
    auto* Ballistic=FindFProperty<FBoolProperty>(Event,TEXT("bBallistic"));
    if(!Velocity || !Ballistic){RejectionReason=TEXT("Shared firing signature mismatch");return false;}
    *Velocity->ContainerPtrToValuePtr<FVector>(Parameters.GetStructMemory())=Launch;
    Ballistic->SetPropertyValue_InContainer(Parameters.GetStructMemory(),HasLaunch);
    LastMuzzle=Start;LastLaunchVelocity=Launch;
    const int32 Before=ShotsFired;
    bDispatching=true;SetCanShoot(GetOwner(),false);
    GetOwner()->ProcessEvent(Event,Parameters.GetStructMemory());
    bDispatching=false;
    if(ShotsFired==Before){SetCanShoot(GetOwner(),true);RejectionReason=TEXT("Original Blueprint projectile spawn failed");return false;}
    return true;
}
void UShipWeaponComponent::FinishShot(AActor* Projectile)
{
    if(!IsValid(Projectile))return;
    Projectile->SetOwner(GetOwner());Projectile->SetInstigator(Cast<APawn>(GetOwner()));
    TArray<UPrimitiveComponent*> Own,Shell;GetOwner()->GetComponents(Own);Projectile->GetComponents(Shell);
    for(auto* C:Shell){C->IgnoreActorWhenMoving(GetOwner(),true);for(auto* O:Own)C->IgnoreComponentWhenMoving(O,true);}
    if(auto* Movement=Projectile->FindComponentByClass<UProjectileMovementComponent>())
    {
        // The existing WithSpeed BeginPlay already sets this. Explicit assignment also
        // makes both original classes follow the exact same documented launch model.
        Movement->Velocity=LastLaunchVelocity;Movement->UpdateComponentVelocity();
        Movement->OnProjectileStop.AddUniqueDynamic(this,&UShipWeaponComponent::OnProjectileStop);
    }
    Projectile->SetLifeSpan(45);LastShotTime=GetWorld()->GetTimeSeconds();LastProjectile=Projectile;++ShotsFired;
    UE_LOG(LogTemp,Display,TEXT("ShipWeapon shot=%d ship=%s projectile=%s muzzle=%s launch=%s owner=%s instigator=%s"),
        ShotsFired,*GetNameSafe(GetOwner()),*GetNameSafe(Projectile),*LastMuzzle.ToString(),*LastLaunchVelocity.ToString(),*GetNameSafe(Projectile->GetOwner()),*GetNameSafe(Projectile->GetInstigator()));
}
void UShipWeaponComponent::OnProjectileStop(const FHitResult& Hit)
{
    LastHitActor=Hit.GetActor();
    if(Hit.GetActor() && Hit.GetActor()!=GetOwner() && Cast<APawn>(Hit.GetActor()))++Hits;
    UE_LOG(LogTemp,Display,TEXT("ShipWeapon impact ship=%s hit=%s location=%s self=%d shipHits=%d"),*GetNameSafe(GetOwner()),*GetNameSafe(Hit.GetActor()),*Hit.ImpactPoint.ToString(),Hit.GetActor()==GetOwner(),Hits);
}

UShipCombatComponent::UShipCombatComponent()
{PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PrePhysics;}
UPrimitiveComponent* UShipCombatComponent::PhysicalHull(AActor* Ship)
{
    if(!Ship)return nullptr;
    TArray<UPrimitiveComponent*> Components;Ship->GetComponents(Components);
    for(auto* C:Components)if(C->GetFName()==TEXT("ShipSM"))return C;
    return Cast<UPrimitiveComponent>(Ship->GetRootComponent());
}
void UShipCombatComponent::BeginPlay()
{
    Super::BeginPlay();Weapon=GetOwner()->FindComponentByClass<UShipWeaponComponent>();
    Navigation=GetOwner()->FindComponentByClass<UShipEnemyNavigationComponent>();Visual=Weapon?Weapon->Mesh():nullptr;
    if(Visual && Visual->GetSkeletalMeshAsset())
    {
        // Verified existing graph applies pitch about component Y, then parent yaw
        // about component Z, both additive. Confirm the reference forward axis.
        const FReferenceSkeleton& Ref=Visual->GetSkeletalMeshAsset()->GetRefSkeleton();
        int32 Index=Ref.FindBoneIndex(TEXT("front_gun_v_jnt"));FTransform Reference=FTransform::Identity;
        while(Index!=INDEX_NONE){Reference=Reference*Ref.GetRefBonePose()[Index];Index=Ref.GetParentIndex(Index);}
        bVerifiedAxes=Reference.GetRotation().GetForwardVector().Equals(FVector::ForwardVector,.001) && Visual->DoesSocketExist(Weapon->MuzzleSocket);
        Visual->AddTickPrerequisiteComponent(this);
        Visual->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    }
}
FVector UShipCombatComponent::AimPoint(AActor* Ship) const
{if(auto* Hull=PhysicalHull(Ship))return Hull->Bounds.Origin;return Ship->GetActorLocation();}
bool UShipCombatComponent::IsHostile(AActor* Candidate) const
{
    if(!IsValid(Candidate) || Candidate==GetOwner() || Candidate->IsActorBeingDestroyed() || Candidate->ActorHasTag(TEXT("Dead")))return false;
    if(const auto* N=Candidate->FindComponentByClass<UShipEnemyNavigationComponent>())if(N->Health<=0)return false;
    if(const auto* C=Candidate->FindComponentByClass<UShipCombatComponent>())return C->Faction!=Faction;
    if(const auto* W=Candidate->FindComponentByClass<UShipWeaponComponent>())return W->Faction!=Faction;
    return false;
}
bool UShipCombatComponent::Visible(AActor* Candidate) const
{
    if(!Visual || !Candidate)return false;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ShipCombatLOS),true,GetOwner());IgnoreWater(GetWorld(),Q);
    FHitResult Hit;return !GetWorld()->LineTraceSingleByChannel(Hit,Visual->GetSocketLocation(Weapon->MuzzleSocket),AimPoint(Candidate),ECC_Visibility,Q) || Hit.GetActor()==Candidate;
}
void UShipCombatComponent::ScanTargets()
{
    if(!Weapon || !Visual)return;
    const FVector Origin=PhysicalHull(GetOwner())?PhysicalHull(GetOwner())->GetComponentLocation():GetOwner()->GetActorLocation();
    if(IsHostile(Target) && FVector::Distance(Origin,AimPoint(Target))<=LoseTargetRadiusM*100)
    {bTargetVisible=Visible(Target);return;}
    Target=nullptr;bTargetVisible=false;double Best=DBL_MAX;
    for(TActorIterator<APawn> I(GetWorld());I;++I)
    {
        if(!IsHostile(*I))continue;
        const double Distance=FVector::Distance(Origin,AimPoint(*I));
        if(Distance>DetectionRadiusM*100 || Distance>=Best)continue;
        if(bRequireLineOfSight && !Visible(*I))continue;
        Target=*I;Best=Distance;bTargetVisible=true;
    }
}
bool UShipCombatComponent::Solve(FVector Start,FVector Aim,FVector Velocity,FVector& Launch,FVector& Intercept) const
{
    // Original projectiles use world velocity, with no inherited shooter velocity,
    // gravity scale 1 and 200m/s speed. Use that model rather than adding drift twice.
    double Flight=FVector::Distance(Start,Aim)/Weapon->MuzzleSpeedCmps;
    for(int32 I=0;I<10;++I)
    {
        Intercept=Aim+(bEnablePredictiveAiming?Velocity*Flight:FVector::ZeroVector);
        UGameplayStatics::FSuggestProjectileVelocityParameters P(this,Start,Intercept,Weapon->MuzzleSpeedCmps);
        P.TraceOption=ESuggestProjVelocityTraceOption::DoNotTrace;
        if(!UGameplayStatics::SuggestProjectileVelocity(P,Launch))return false;
        const double Next=FVector2D(Intercept-Start).Size()/FMath::Max(1.,FVector2D(Launch).Size());
        if(FMath::Abs(Next-Flight)<.001)return true;
        Flight=Next;
    }
    // Reject divergent interceptions instead of firing an unverified estimate.
    return FVector::Distance(Start+Launch*Flight+FVector(0,0,.5*GetWorld()->GetGravityZ()*Flight*Flight),Intercept)<100;
}
bool UShipCombatComponent::ClearArc(FVector Start,FVector Launch,float Flight,FString& Reason) const
{
    FCollisionQueryParams OwnQuery(SCENE_QUERY_STAT(ShipGunSelf),true);
    FHitResult SelfHit;TArray<UPrimitiveComponent*> Components;GetOwner()->GetComponents(Components);
    // Skip the first 2m of barrel; test the rest of our actual hull/superstructure.
    for(auto* C:Components)if((C->GetFName()==TEXT("ShipSM") || C->GetFName()==TEXT("Ship")) &&
        C->LineTraceComponent(SelfHit,Start+Launch.GetSafeNormal()*200,Start+Launch.GetSafeNormal()*20000,OwnQuery))
    {Reason=TEXT("Own hull/superstructure obstructs muzzle");return false;}
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ShipCombatArc),true,GetOwner());IgnoreWater(GetWorld(),Q);
    FVector Previous=Start;
    const int32 Steps=FMath::Clamp(FMath::CeilToInt(Flight*30),2,1800);
    for(int32 I=1;I<=Steps;++I)
    {
        const double T=Flight*I/Steps;const FVector Next=Start+Launch*T+FVector(0,0,.5*GetWorld()->GetGravityZ()*T*T);
        FHitResult Hit;
        if(GetWorld()->SweepSingleByChannel(Hit,Previous,Next,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(12.5),Q))
        {
            if(Hit.GetActor()==Target)return true;
            Reason=FString::Printf(TEXT("Trajectory blocked by %s"),*GetNameSafe(Hit.GetActor()));return false;
        }
        if(!bAllowFriendlyFire)
        {
            // Include friendly Pawns even if their Visibility channel is disabled.
            for(TActorIterator<APawn> It(GetWorld());It;++It)
                if(*It!=GetOwner() && *It!=Target && !IsHostile(*It))
                {
                    auto* Hull=PhysicalHull(*It);
                    if(Hull && FMath::PointDistToSegment(Hull->Bounds.Origin,Previous,Next)<Hull->Bounds.SphereRadius+1000)
                    {Reason=TEXT("Friendly ship in trajectory/blast corridor");return false;}
                }
        }
        Previous=Next;
    }
    // Moving targets need not intersect their CURRENT position along a future arc.
    return true;
}
void UShipCombatComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick)
{
    Super::TickComponent(Dt,Type,Tick);
    bHasFiringSolution=false;CooldownSecondsRemaining=FMath::Max(0.,LastFire+FireCooldownSeconds-GetWorld()->GetTimeSeconds());
    if(!Weapon || !Visual || !Navigation || !bVerifiedAxes){FiringStatus=TEXT("Missing verified shared weapon/animation binding");return;}
    const bool Alive=Navigation->Health>0;
    const bool CanEngage=bEnableCombat && Alive && Navigation->Role!=EShipEnemyRole::Passive;
    ScanClock+=Dt;if(CanEngage && ScanClock>=FMath::Max(.05f,TargetScanInterval)){ScanClock=0;ScanTargets();}
    if(!CanEngage){Target=nullptr;FiringStatus=Alive?TEXT("Disabled/passive"):TEXT("Dead");}
    const FVector Start=Visual->GetSocketLocation(Weapon->MuzzleSocket);
    bool Solution=false,InArc=false;
    if(CanEngage && IsHostile(Target))
    {
        DistanceToTargetM=FVector::Distance(Start,AimPoint(Target))/100;
        Solution=Solve(Start,AimPoint(Target),VelocityOf(Target),LaunchVelocity,PredictedIntercept);
        if(Solution)
        {
            // Full inverse component rotation compensates yaw, pitch AND roll.
            const FRotator Local=Visual->GetComponentTransform().InverseTransformVectorNoScale(LaunchVelocity).Rotation();
            DesiredTurretYaw=FRotator::NormalizeAxis(Local.Yaw);DesiredBarrelPitch=FRotator::NormalizeAxis(Local.Pitch);
            InArc=DesiredTurretYaw>=MinTurretYaw && DesiredTurretYaw<=MaxTurretYaw && DesiredBarrelPitch>=MinBarrelElevation && DesiredBarrelPitch<=MaxBarrelElevation;
        }
        else FiringStatus=TEXT("No ballistic intercept at original projectile speed");
    }
    else{Target=nullptr;DistanceToTargetM=0;DesiredTurretYaw=0;DesiredBarrelPitch=0;if(CanEngage)FiringStatus=TEXT("No hostile target");}
    const float YawGoal=FMath::Clamp(DesiredTurretYaw,MinTurretYaw,MaxTurretYaw),PitchGoal=FMath::Clamp(DesiredBarrelPitch,MinBarrelElevation,MaxBarrelElevation);
    // This bounded mechanical arc cannot wrap through the forbidden rear sector.
    TurretYaw=FMath::FInterpConstantTo(TurretYaw,YawGoal,Dt,FMath::Max(0.f,TurretYawSpeedDegPerSec));
    BarrelPitch=FMath::FInterpConstantTo(BarrelPitch,PitchGoal,Dt,FMath::Max(0.f,BarrelPitchSpeedDegPerSec));
    if(Solution && Target)
    {
        bHasFiringSolution=InArc;
        if(Navigation->Role==EShipEnemyRole::Coward && !bDefensiveFire)FiringStatus=TEXT("Coward: defensive fire disabled");
        else if(DistanceToTargetM<MinEngagementRadiusM || DistanceToTargetM>EngagementRadiusM)FiringStatus=TEXT("Outside engagement range");
        else if(!InArc)FiringStatus=TEXT("Outside mechanical firing arc");
        else if(bRequireLineOfSight && !bTargetVisible)FiringStatus=TEXT("Line of sight blocked");
        else if(FMath::Abs(FMath::FindDeltaAngleDegrees(TurretYaw,DesiredTurretYaw))>AimYawTolerance || FMath::Abs(BarrelPitch-DesiredBarrelPitch)>AimPitchTolerance ||
            FVector::DotProduct(Visual->GetSocketRotation(Weapon->MuzzleSocket).Vector(),LaunchVelocity.GetSafeNormal())<FMath::Cos(FMath::DegreesToRadians(FMath::Min(AimYawTolerance,AimPitchTolerance))))
            FiringStatus=TEXT("Tracking: waiting for actual animated muzzle alignment");
        else if(CooldownSecondsRemaining>0 || !CanShoot(GetOwner()))FiringStatus=TEXT("Cooldown/recoil");
        else
        {
            FireCheckClock+=Dt;
            if(FireCheckClock>=.1f)
            {
                FireCheckClock=0;bTargetVisible=Visible(Target);
                const float Flight=FVector2D(PredictedIntercept-Start).Size()/FMath::Max(1.,FVector2D(LaunchVelocity).Size());FString Reason;
                if(bRequireLineOfSight && !bTargetVisible)FiringStatus=TEXT("Line of sight blocked");
                else if(!ClearArc(Start,LaunchVelocity,Flight,Reason))FiringStatus=Reason;
                else if(Weapon->TryFireWeapon(PredictedIntercept,Target,LaunchVelocity,true)){LastFire=GetWorld()->GetTimeSeconds();FiringStatus=TEXT("Fired shared BP weapon");}
                else FiringStatus=Weapon->RejectionReason;
            }
        }
    }
    if(bDrawDebug)
    {
        DrawDebugLine(GetWorld(),Start,Start+Visual->GetSocketRotation(Weapon->MuzzleSocket).Vector()*10000,FColor::Green,false,0,0,15);
        if(Target){DrawDebugLine(GetWorld(),Start,PredictedIntercept,FColor::Orange,false,0,0,10);DrawDebugSphere(GetWorld(),PredictedIntercept,500,12,FColor::Red,false,0);}
        DrawDebugString(GetWorld(),Start+FVector(0,0,1200),FString::Printf(TEXT("Combat %s | %s %.0fm | yaw %.1f/%.1f pitch %.1f/%.1f | shots %d hits %d | %s"),
            *UEnum::GetValueAsString(Navigation->Role),*GetNameSafe(Target),DistanceToTargetM,TurretYaw,DesiredTurretYaw,BarrelPitch,DesiredBarrelPitch,Weapon->ShotsFired,Weapon->Hits,*FiringStatus),nullptr,FColor::White,0);
        if(GetWorld()->GetTimeSeconds()-LastLog>=2)
        {
            LastLog=GetWorld()->GetTimeSeconds();
            UE_LOG(LogTemp,Display,TEXT("ShipCombat role=%s nav=%s waypoint=%d target=%s range=%.1fm yaw=%.2f/%.2f pitch=%.2f/%.2f shots=%d hits=%d status=%s"),
                *UEnum::GetValueAsString(Navigation->Role),*UEnum::GetValueAsString(Navigation->NavigationState),Navigation->CurrentWaypointIndex,*GetNameSafe(Target),DistanceToTargetM,TurretYaw,DesiredTurretYaw,BarrelPitch,DesiredBarrelPitch,Weapon->ShotsFired,Weapon->Hits,*FiringStatus);
        }
    }
}
FRotator UShipCombatLibrary::SelectGunAim(APawn* Ship,FRotator ManualAim)
{
    if(Ship && !Ship->IsPlayerControlled())if(const auto* C=Ship->FindComponentByClass<UShipCombatComponent>())return FRotator(C->BarrelPitch,C->TurretYaw,0);
    return ManualAim;
}
