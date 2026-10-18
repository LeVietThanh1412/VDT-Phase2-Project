#include "ShipCombatComponent.h"
#include "ShipProjectileDamageComponent.h"
#include "NavalBattleUI.h"
#include "Components/WidgetComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "ShipEnemyNavigationComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
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
void UShipWeaponComponent::BeginPlay()
{
    Super::BeginPlay();CurrentHealth=FMath::Max(1.f,MaxHealth);bIsDestroyed=false;
    GetOwner()->OnTakeAnyDamage.AddUniqueDynamic(this,&UShipWeaponComponent::OnDamaged);
    if(auto* Battle=GetWorld()->GetSubsystem<UNavalBattleSubsystem>())Battle->RegisterShip(this);
    GetWorld()->GetTimerManager().SetTimer(PresentationTimer,this,&UShipWeaponComponent::UpdatePresentation,.5f,true);
}
void UShipWeaponComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorld()->GetTimerManager().ClearTimer(PresentationTimer);
    GetOwner()->OnTakeAnyDamage.RemoveAll(this);Super::EndPlay(Reason);
}
int32 UShipWeaponComponent::Team() const
{if(const auto* C=GetOwner()->FindComponentByClass<UShipCombatComponent>())return C->Faction;return Faction;}
void UShipWeaponComponent::OnDamaged(AActor*,float Damage,const UDamageType*,AController*,AActor* Causer)
{ReceiveShipDamage(Damage,Causer);}
bool UShipWeaponComponent::ReceiveShipDamage(float Damage,AActor* Causer)
{
    if(bIsDestroyed || !FMath::IsFinite(Damage) || Damage<=0 || !IsValid(Causer))return false;
    auto* Round=Causer->FindComponentByClass<UShipProjectileDamageComponent>();
    AActor* Source=Round?Round->SourceShip.Get():Causer;
    if(!Round && !Causer->FindComponentByClass<UShipWeaponComponent>())Source=Causer->GetOwner();
    auto* SourceWeapon=Source?Source->FindComponentByClass<UShipWeaponComponent>():nullptr;
    const int32 SourceTeam=Round?Round->SourceTeam:SourceWeapon?SourceWeapon->Team():2;
    // Final, non-bypassable faction guard. Neutral and unknown sources are inert.
    // Cached projectile affiliation survives source-ship cleanup, including splash.
    if(Source==GetOwner() || SourceTeam==Team() || SourceTeam<0 || SourceTeam>1 || Team()<0 || Team()>1)
    {++RejectedTeamHits;return false;}
    if(Round && !Round->ClaimVictim(GetOwner()))return false;
    CurrentHealth=FMath::Clamp(CurrentHealth-Damage,0.f,FMath::Max(1.f,MaxHealth));++AcceptedDamageEvents;
    if(auto* N=GetOwner()->FindComponentByClass<UShipEnemyNavigationComponent>())N->Health=CurrentHealth;
    UE_LOG(LogTemp,Display,TEXT("ShipBattle damage ship=%s team=%d sourceTeam=%d hp=%.1f damage=%.1f projectile=%s"),*GetNameSafe(GetOwner()),Team(),SourceTeam,CurrentHealth,Damage,*GetNameSafe(Causer));
    if(CurrentHealth<=0)
    {
        bIsDestroyed=true;++DestructionEvents;GetOwner()->Tags.AddUnique(TEXT("Dead"));SetCanShoot(GetOwner(),false);
        if(auto* N=GetOwner()->FindComponentByClass<UShipEnemyNavigationComponent>()){N->StopNavigation();N->SetDriveCommand(0,0);}
        if(auto* C=GetOwner()->FindComponentByClass<UShipCombatComponent>()){C->bEnableCombat=false;C->Target=nullptr;C->SetComponentTickEnabled(false);}
        if(auto* Pawn=Cast<APawn>(GetOwner()))if(auto* PC=Cast<APlayerController>(Pawn->GetController()))
        {GetOwner()->DisableInput(PC);PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);}
        GetOwner()->SetActorTickEnabled(false);
        // Stop buoyancy, then let the existing hull physics sink the wreck.
        TArray<UActorComponent*> Components;GetOwner()->GetComponents(Components);
        for(auto* C:Components)if(C->GetClass()->GetName().Contains(TEXT("Buoyancy"))){C->Deactivate();C->SetComponentTickEnabled(false);}
        if(auto* Hull=UShipCombatComponent::PhysicalHull(GetOwner()))
        {Hull->SetPhysicsLinearVelocity(FVector(0,0,-150));Hull->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);}
        if(HealthWidget)HealthWidget->SetVisibility(false);
        if(auto* FX=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Ship/Assets/MsvFx_Niagara_Explosion_Pack_01/Prefabs/Niagara_Dust_Explosion_01.Niagara_Dust_Explosion_01")))
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(this,FX,UShipCombatComponent::PhysicalHull(GetOwner())->Bounds.Origin,FRotator::ZeroRotator,FVector(12),true,true,ENCPoolMethod::None,false);
        // Keep the player's inert camera/pawn as a spectator anchor. No respawn.
        auto* Pawn=Cast<APawn>(GetOwner());if(!Pawn || !Pawn->IsPlayerControlled())GetOwner()->SetLifeSpan(FMath::Max(2.f,DestructionDelaySeconds));
        if(auto* Battle=GetWorld()->GetSubsystem<UNavalBattleSubsystem>())Battle->ShipDestroyed(this);
        OnShipDestroyed.Broadcast(this,Source);
        UE_LOG(LogTemp,Display,TEXT("ShipBattle destroyed ship=%s team=%d attacker=%s event=%d"),*GetNameSafe(GetOwner()),Team(),*GetNameSafe(Source),DestructionEvents);
    }
    OnHealthChanged.Broadcast(this,CurrentHealth,MaxHealth);RefreshHealthUI();return true;
}
void UShipWeaponComponent::RefreshHealthUI()
{
    if(HealthWidget)if(auto* Widget=Cast<UShipHealthBarWidget>(HealthWidget->GetUserWidgetObject()))Widget->Refresh(this);
    if(auto* Battle=GetWorld()->GetSubsystem<UNavalBattleSubsystem>())Battle->RefreshHUD();
}
void UShipWeaponComponent::UpdatePresentation()
{
    auto* Pawn=Cast<APawn>(GetOwner());if(!Pawn)return;
    if(Pawn->IsPlayerControlled())
    {
        if(auto* Battle=GetWorld()->GetSubsystem<UNavalBattleSubsystem>())Battle->EnsureHUD();
        GetWorld()->GetTimerManager().ClearTimer(PresentationTimer);return;
    }
    if(!HealthWidget && !bIsDestroyed)
    {
        auto* Hull=UShipCombatComponent::PhysicalHull(GetOwner());if(!Hull)return;
        HealthWidget=NewObject<UWidgetComponent>(GetOwner(),TEXT("BattleHealthBar"));GetOwner()->AddInstanceComponent(HealthWidget);
        HealthWidget->SetupAttachment(Hull);HealthWidget->SetWidgetSpace(EWidgetSpace::Screen);HealthWidget->SetDrawSize(FVector2D(180,48));
        UClass* Class=LoadClass<UShipHealthBarWidget>(nullptr,TEXT("/Game/NavalBattle/UI/WBP_ShipHealthBar.WBP_ShipHealthBar_C"));
        HealthWidget->SetWidgetClass(Class?Class:UShipHealthBarWidget::StaticClass());HealthWidget->RegisterComponent();
        HealthWidget->SetRelativeLocation(Hull->GetComponentTransform().InverseTransformPosition(Hull->Bounds.Origin+FVector(0,0,Hull->Bounds.BoxExtent.Z+1500)));
        HealthWidget->InitWidget();RefreshHealthUI();
    }
    if(HealthWidget)
    {
        auto* PC=UGameplayStatics::GetPlayerController(this,0);FVector Eye;FRotator Rotation;if(PC)PC->GetPlayerViewPoint(Eye,Rotation);
        HealthWidget->SetVisibility(!bIsDestroyed && PC && FVector::DistSquared(Eye,HealthWidget->GetComponentLocation())<FMath::Square(220000.f));
    }
}
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
    if(bIsDestroyed){RejectionReason=TEXT("Destroyed");return false;}
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
    if(auto* Round=Projectile->FindComponentByClass<UShipProjectileDamageComponent>())Round->InitializeSource(GetOwner());
    TArray<UPrimitiveComponent*> Own,Shell;GetOwner()->GetComponents(Own);Projectile->GetComponents(Shell);
    for(auto* C:Shell){C->IgnoreActorWhenMoving(GetOwner(),true);for(auto* O:Own)C->IgnoreComponentWhenMoving(O,true);}
    if(auto* Movement=Projectile->FindComponentByClass<UProjectileMovementComponent>())
    {
        // The existing WithSpeed BeginPlay already sets this. Explicit assignment also
        // makes both original classes follow the exact same documented launch model.
        Movement->Velocity=LastLaunchVelocity;Movement->UpdateComponentVelocity();
        Movement->OnProjectileStop.AddUniqueDynamic(this,&UShipWeaponComponent::OnProjectileStop);
    }
    // Bound AI rounds to the configured battle scale; keep player lifetime unchanged.
    if(const auto* Combat=GetOwner()->FindComponentByClass<UShipCombatComponent>())
        Projectile->SetLifeSpan(FMath::Clamp(Combat->EngagementRadiusM*100/FMath::Max(1.f,MuzzleSpeedCmps)*1.3f+.5f,1.f,30.f));
    LastShotTime=GetWorld()->GetTimeSeconds();LastProjectile=Projectile;++ShotsFired;
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
    if(Navigation && Weapon)Navigation->Health=Weapon->CurrentHealth;
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
    if(Faction==2)return false;
    if(const auto* W=Candidate->FindComponentByClass<UShipWeaponComponent>())return !W->bIsDestroyed && W->Team()!=Faction && W->Team()!=2;
    return false;
}
bool UShipCombatComponent::Visible(AActor* Candidate) const
{
    if(!Visual || !Candidate)return false;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ShipCombatLOS),true,GetOwner());IgnoreWater(GetWorld(),Q);
    const FVector Start=Visual->GetSocketLocation(Weapon->MuzzleSocket),Delta=AimPoint(Candidate)-Start;
    const float Limit=FMath::Max(DetectionRadiusM,EngagementRadiusM)*100;
    if(Delta.Size()>Limit)return false;
    FHitResult Hit;return !GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+Delta.GetClampedToMaxSize(Limit),ECC_Visibility,Q) || Hit.GetActor()==Candidate;
}
void UShipCombatComponent::ScanTargets()
{
    if(!Weapon || !Visual)return;
    const FVector Origin=PhysicalHull(GetOwner())?PhysicalHull(GetOwner())->GetComponentLocation():GetOwner()->GetActorLocation();
    AActor* Previous=Target;
    const bool Retain=IsHostile(Target) && FVector::Distance(Origin,AimPoint(Target))<=FMath::Max(LoseTargetRadiusM,DetectionRadiusM)*100;
    if(Retain && Visible(Target)){bTargetVisible=true;return;}
    Target=Retain?Target:nullptr;bTargetVisible=false;double Best=DBL_MAX;
    for(TActorIterator<APawn> I(GetWorld());I;++I)
    {
        if(!IsHostile(*I))continue;
        const double Distance=FVector::Distance(Origin,AimPoint(*I));
        if(Distance>DetectionRadiusM*100 || Distance>=Best)continue;
        if(bRequireLineOfSight && !Visible(*I))continue;
        Target=*I;Best=Distance;bTargetVisible=true;
    }
    if(Target!=Previous)UE_LOG(LogTemp,Display,TEXT("ShipBattle target ship=%s team=%d old=%s new=%s"),*GetNameSafe(GetOwner()),Faction,*GetNameSafe(Previous),*GetNameSafe(Target));
}
void UShipCombatComponent::UpdateTactics()
{
    const double Now=GetWorld()->GetTimeSeconds();
    const bool Valid=IsHostile(Target),Engage=Valid && DistanceToTargetM<=EngagementRadiusM;
    auto Change=[&](EShipTacticalState State)
    {
        if(State==TacticalState)return;
        UE_LOG(LogTemp,Display,TEXT("ShipBattle state ship=%s team=%d from=%s to=%s range=%.1f"),*GetNameSafe(GetOwner()),Faction,*UEnum::GetValueAsString(TacticalState),*UEnum::GetValueAsString(State),DistanceToTargetM);
        TacticalState=State;StateSince=Now;
    };
    if(!bEnableHitAndRun || !bEnableCombat || Weapon->bIsDestroyed || Navigation->Role==EShipEnemyRole::Passive)
    {if(Navigation->HasTacticalDestination())Navigation->ReturnToPatrol();Change(EShipTacticalState::Patrol);return;}
    if(TacticalState==EShipTacticalState::Retreat)
    {
        if((Now-StateSince>=MinimumRetreatSeconds && (!Valid || DistanceToTargetM>=FMath::Max(SafeReengagementDistanceM,MinimumRetreatDistanceM+100))) || Now-StateSince>=FMath::Max(MaximumRetreatSeconds,MinimumRetreatSeconds))
        {Navigation->ReturnToPatrol();++PatrolsResumed;LastRetreatEnded=Now;Change(EShipTacticalState::ReturnToPatrol);}
        return;
    }
    if(TacticalState==EShipTacticalState::ReturnToPatrol)
    {if(Now-StateSince>=3)Change(Engage?EShipTacticalState::Engage:EShipTacticalState::Patrol);return;}
    if(Valid && DistanceToTargetM<MinimumRetreatDistanceM && Now-LastRetreatEnded>=RetreatCooldownSeconds)
    {
        if(Navigation->BeginRetreatFrom(Target,FMath::Max(SafeReengagementDistanceM,MinimumRetreatDistanceM+100)))
        {++RetreatsStarted;Change(EShipTacticalState::Retreat);return;}
    }
    if(Now-StateSince>=3)Change(Engage?EShipTacticalState::Engage:EShipTacticalState::Patrol);
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
        if(bDrawDebug)DrawDebugLine(GetWorld(),Previous,Next,FColor::Cyan,false,.15f,0,3);
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
    const bool Alive=!Weapon->bIsDestroyed;
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
    UpdateTactics();
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
        const FColor TeamColor=Faction==0?FColor::Blue:FColor::Red;
        DrawDebugString(GetWorld(),Start+FVector(0,0,1200),FString::Printf(TEXT("Team %s | %s | HP %.0f | %s %.0fm | yaw %.1f/%.1f pitch %.1f/%.1f | shots %d hits %d | %s"),
            Faction==0?TEXT("BLUE"):TEXT("RED"),*UEnum::GetValueAsString(TacticalState),Weapon->CurrentHealth,*GetNameSafe(Target),DistanceToTargetM,TurretYaw,DesiredTurretYaw,BarrelPitch,DesiredBarrelPitch,Weapon->ShotsFired,Weapon->Hits,*FiringStatus),nullptr,TeamColor,0);
        if(GetWorld()->GetTimeSeconds()-LastLog>=2)
        {
            LastLog=GetWorld()->GetTimeSeconds();
            const FVector X=Visual->GetForwardVector(),Y=Visual->GetRightVector();
            DrawDebugCircle(GetWorld(),Start,DetectionRadiusM*100,64,TeamColor,false,2.05,0,4,X,Y,false);
            DrawDebugCircle(GetWorld(),Start,EngagementRadiusM*100,64,FColor::Orange,false,2.05,0,4,X,Y,false);
            DrawDebugCircle(GetWorld(),Start,MinimumRetreatDistanceM*100,32,FColor::Yellow,false,2.05,0,4,X,Y,false);
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

void UShipCombatLibrary::ApplyProjectileImpact(AActor* Projectile,FHitResult Impact)
{
    if(!IsValid(Projectile))return;
    auto* Round=Projectile->FindComponentByClass<UShipProjectileDamageComponent>();if(!Round || Round->bImpacted)return;
    Round->bImpacted=true;Round->InitializeSource(Projectile->GetOwner());
    // A direct hull hit always delivers one full hit, even if visibility/splash
    // traces start inside its collision. Nearby hulls share the same final guard.
    if(auto* Hit=Impact.GetActor())if(auto* W=Hit->FindComponentByClass<UShipWeaponComponent>())W->ReceiveShipDamage(Round->ProjectileDamage,Projectile);
    for(TActorIterator<APawn> I(Projectile->GetWorld());I;++I)
    {
        if(*I==Impact.GetActor())continue;auto* W=I->FindComponentByClass<UShipWeaponComponent>();auto* Hull=UShipCombatComponent::PhysicalHull(*I);
        if(!W || !Hull || FVector::Distance(Hull->Bounds.Origin,Impact.ImpactPoint)>Round->SplashRadiusCm+Hull->Bounds.SphereRadius)continue;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(ShipSplash),true,Projectile);Q.AddIgnoredActor(Projectile->GetOwner());FHitResult Hit;
        if(!Projectile->GetWorld()->LineTraceSingleByChannel(Hit,Impact.ImpactPoint,Hull->Bounds.Origin,ECC_Visibility,Q) || Hit.GetActor()==*I)
            W->ReceiveShipDamage(Round->ProjectileDamage,Projectile);
    }
}
