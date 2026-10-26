#include "ShipCombatComponent.h"
#include "ShipProjectileDamageComponent.h"
#include "NavalBattleUI.h"
#include "Components/WidgetComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraDataSetAccessor.h"
#include "NiagaraSpriteRendererProperties.h"
#include "CesiumGlobeAnchorComponent.h"
#include "CesiumGeoreference.h"
#include "BuoyancyComponent.h"
#include "Camera/CameraActor.h"
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
#include "Misc/Crc.h"

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

FString UShipIdentityComponent::GetDisplayName() const
{
    if(!ShipName.IsEmpty())return ShipName;
    const AActor* Owner=GetOwner();
    if(!Owner)return TEXT("Ship");
    if(const APawn* Pawn=Cast<APawn>(Owner))
        if(Pawn->IsPlayerControlled())return TEXT("Player Ship");
    const FString Label=Owner->GetActorNameOrLabel();
    return Label.IsEmpty()?Owner->GetName():Label;
}
FString UShipIdentityComponent::GetTeamName() const
{
    if(!TeamName.IsEmpty())return TeamName;
    const AActor* Owner=GetOwner();
    if(Owner)
        if(const auto* Weapon=Owner->FindComponentByClass<UShipWeaponComponent>())
            return Weapon->Team()==0?TEXT("Blue"):TEXT("Red");
    return TEXT("Blue");
}
void UShipIdentityComponent::BeginPlay()
{
    Super::BeginPlay();
    if(!TeamName.IsEmpty())return;
    const AActor* Owner=GetOwner();
    const auto* Weapon=Owner?Owner->FindComponentByClass<UShipWeaponComponent>():nullptr;
    TeamName=Weapon && Weapon->Team()!=0?TEXT("Red"):TEXT("Blue");
}
UShipWeaponComponent::UShipWeaponComponent(){PrimaryComponentTick.bCanEverTick=false;}
void UShipWeaponComponent::BeginPlay()
{
    Super::BeginPlay();CurrentHealth=FMath::Max(1.f,MaxHealth);bIsDestroyed=false;
    GetOwner()->OnTakeAnyDamage.AddUniqueDynamic(this,&UShipWeaponComponent::OnDamaged);
    GetWorld()->GetTimerManager().SetTimer(PresentationTimer,this,&UShipWeaponComponent::UpdatePresentation,.5f,true);
    LastDestructionUpdate=GetWorld()->GetTimeSeconds();
    GetWorld()->GetTimerManager().SetTimer(DestructionTimer,this,&UShipWeaponComponent::UpdateDestruction,.1f,true);
}
void UShipWeaponComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorld()->GetTimerManager().ClearTimer(PresentationTimer);
    GetWorld()->GetTimerManager().ClearTimer(DestructionTimer);
    if(DestructionFX){DestructionFX->DeactivateImmediate();DestructionFX->DestroyComponent();DestructionFX=nullptr;}
    GetOwner()->OnTakeAnyDamage.RemoveAll(this);Super::EndPlay(Reason);
}
int32 UShipWeaponComponent::Team() const
{if(const auto* C=GetOwner()->FindComponentByClass<UShipCombatComponent>())return C->Faction;return Faction;}
FString UShipWeaponComponent::GetShipName() const
{
    const AActor* Owner=GetOwner();
    if(!Owner)return TEXT("Ship");
    if(const auto* Identity=Owner->FindComponentByClass<UShipIdentityComponent>())
        return Identity->GetDisplayName();
    if(const APawn* Pawn=Cast<APawn>(Owner))
        if(Pawn->IsPlayerControlled())return TEXT("Player Ship");
    const FString Label=Owner->GetActorNameOrLabel();
    return Label.IsEmpty()?Owner->GetName():Label;
}
FString UShipWeaponComponent::GetTeamName() const
{
    const AActor* Owner=GetOwner();
    if(Owner)
        if(const auto* Identity=Owner->FindComponentByClass<UShipIdentityComponent>())
            return Identity->GetTeamName();
    return Team()==0?TEXT("Blue"):TEXT("Red");
}
FString UShipCombatLibrary::GetShipNameForObject(UObject* Object)
{
    const AActor* Ship=Cast<AActor>(Object);
    if(!Ship)return TEXT("Ship");
    if(const auto* Identity=Ship->FindComponentByClass<UShipIdentityComponent>())
        return Identity->GetDisplayName();
    if(const auto* Weapon=Ship->FindComponentByClass<UShipWeaponComponent>())
        return Weapon->GetShipName();
    const FString Label=Ship->GetActorNameOrLabel();
    return Label.IsEmpty()?Ship->GetName():Label;
}
void UShipWeaponComponent::OnDamaged(AActor*,float Damage,const UDamageType*,AController*,AActor* Causer)
{ReceiveShipDamage(Damage,Causer);}
bool UShipWeaponComponent::ReceiveHullImpact(AActor* Projectile,FVector ImpactWorld)
{
    const auto* Round=IsValid(Projectile)?Projectile->FindComponentByClass<UShipProjectileDamageComponent>():nullptr;
    const auto* Hull=UShipCombatComponent::PhysicalHull(GetOwner());
    if(!Round || !Hull || ImpactWorld.ContainsNaN())return false;
    // ShipSM's verified local +X matches Arrow (dot 1.0), and its length is
    // 157.59 m versus width 23.01 m. Use that physical hull, not visual rig bounds.
    const FVector Local=Hull->GetComponentTransform().InverseTransformPosition(ImpactWorld);
    const FBox Bounds=Hull->GetLocalBounds().GetBox();
    const float X=FMath::Clamp((Local.X-Bounds.Min.X)/FMath::Max(1.,Bounds.Max.X-Bounds.Min.X),0.,1.);
    const EShipHitRegion Region=X>=1-FMath::Clamp(BowLengthFraction,0.f,.45f)?EShipHitRegion::Bow:
        X<=FMath::Clamp(SternLengthFraction,0.f,.45f)?EShipHitRegion::Stern:EShipHitRegion::Midship;
    const float Multiplier=FMath::Clamp(Region==EShipHitRegion::Bow?BowDamageMultiplier:Region==EShipHitRegion::Stern?SternDamageMultiplier:MidshipDamageMultiplier,0.f,3.f);
    const float Damage=Round->ProjectileDamage*Multiplier;
    // The existing final authority performs team/self/dead/duplicate guards.
    // Rejected impacts do not publish an accepted region or health event.
    if(!ReceiveShipDamage(Damage,Projectile))return false;
    LastImpactRegion=Region;LastImpactWorld=ImpactWorld;LastImpactLocal=Local;LastNormalizedImpactX=X;LastAppliedImpactDamage=Damage;
    if(bDebugDamage)UE_LOG(LogTemp,Display,TEXT("ShipBattle hullHit shooter=%s victim=%s projectile=%s world=%s local=%s normalizedX=%.3f region=%s multiplier=%.2f damage=%.1f hp=%.1f"),
        *GetNameSafe(Round->SourceShip.Get()),*GetNameSafe(GetOwner()),*GetNameSafe(Projectile),*ImpactWorld.ToString(),*Local.ToString(),X,*UEnum::GetValueAsString(Region),Multiplier,Damage,CurrentHealth);
    return true;
}
bool UShipWeaponComponent::ReceiveShipDamage(float Damage,AActor* Causer)
{
    if(bIsDestroyed || !GetOwner()->CanBeDamaged() || !FMath::IsFinite(Damage) || Damage<=0 || !IsValid(Causer))return false;
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
    if(CurrentHealth>0)if(auto* C=GetOwner()->FindComponentByClass<UShipCombatComponent>())C->OnHostileHit(Source);
    UE_LOG(LogTemp,Display,TEXT("ShipBattle damage ship=%s team=%d sourceTeam=%d hp=%.1f damage=%.1f projectile=%s"),*GetNameSafe(GetOwner()),Team(),SourceTeam,CurrentHealth,Damage,*GetNameSafe(Causer));
    if(CurrentHealth<=0)return Die(EShipDeathReason::HealthDepleted,Source);
    OnHealthChanged.Broadcast(this,CurrentHealth,MaxHealth);RefreshHealthUI();return true;
}
bool UShipWeaponComponent::Die(EShipDeathReason Reason,AActor* Attacker)
{
    if(bIsDestroyed)return false;
    bIsDestroyed=true;CurrentHealth=0;DeathReason=Reason;DestructionStage=EShipDestructionStage::Dying;++DestructionEvents;
    DeathStartedAt=GetWorld()->GetTimeSeconds();GetOwner()->Tags.AddUnique(TEXT("Dead"));SetCanShoot(GetOwner(),false);
    GetOwner()->SetLifeSpan(0); // Staged presentation owns cleanup, including player wrecks.
    if(auto* N=GetOwner()->FindComponentByClass<UShipEnemyNavigationComponent>()){N->Health=0;N->StopNavigation();N->SetDriveCommand(0,0);}
    if(auto* C=GetOwner()->FindComponentByClass<UShipCombatComponent>()){C->bEnableCombat=false;C->Target=nullptr;C->TacticalState=EShipTacticalState::Dead;C->SetComponentTickEnabled(false);}
    if(auto* Pawn=Cast<APawn>(GetOwner()))if(auto* PC=Cast<APlayerController>(Pawn->GetController()))
    {GetOwner()->DisableInput(PC);PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);}
    GetOwner()->SetActorTickEnabled(false);
    // Explicitly leave buoyancy and rigid-body simulation BEFORE the wreck's
    // geographic kinematic animation. No transform fights an active solver.
    if(auto* B=GetOwner()->FindComponentByClass<UBuoyancyComponent>()){B->SetCanBeActive(false);B->SetComponentTickEnabled(false);B->Deactivate();}
    auto* Hull=UShipCombatComponent::PhysicalHull(GetOwner());
    if(Hull){Hull->SetSimulatePhysics(false);Hull->SetEnableGravity(false);}
    WreckAnchor=GetOwner()->FindComponentByClass<UCesiumGlobeAnchorComponent>();
    if(WreckAnchor){WreckAnchor->Sync();WreckStartLLH=WreckAnchor->GetLongitudeLatitudeHeight(ECesiumHeightReference::Ellipsoid);WreckESURotation=WreckAnchor->GetEastSouthUpRotation();}
    if(HealthWidget)HealthWidget->SetVisibility(false);
    if(Hull)
    {
        if(auto* FX=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Ship/Assets/MsvFx_Niagara_Explosion_Pack_01/Prefabs/Niagara_Dust_Explosion_01.Niagara_Dust_Explosion_01")))
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(this,FX,Hull->Bounds.Origin,FRotator::ZeroRotator,FVector(12),true,true,ENCPoolMethod::None,false);
        // Existing ship effect contains AftVentFire and BodySmoke emitters.
        auto* Fire=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/NavalBattle/FX/NS_ShipBurn.NS_ShipBurn"));
        if(!Fire)Fire=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Ship/Assets/VigilanteContent/Vehicles/West_Ship_DDG102/FX/NS_West_Ship_DDG102_Damaged.NS_West_Ship_DDG102_Damaged"));
        if(auto* FX=Fire)
        {
            DestructionFX=UNiagaraFunctionLibrary::SpawnSystemAttached(FX,Hull,NAME_None,FVector::ZeroVector,FRotator::ZeroRotator,EAttachLocation::KeepRelativeOffset,false,true,ENCPoolMethod::None,false);
            if(DestructionFX)DestructionFX->SetVariableVec3(TEXT("User.BoxSize"),FVector(6000,1500,800));
        }
    }
    OnShipDestroyed.Broadcast(this,Attacker);OnHealthChanged.Broadcast(this,CurrentHealth,MaxHealth);RefreshHealthUI();
    UE_LOG(LogTemp,Display,TEXT("ShipBattle destroyed ship=%s team=%d reason=%s attacker=%s event=%d"),*GetNameSafe(GetOwner()),Team(),*UEnum::GetValueAsString(Reason),*GetNameSafe(Attacker),DestructionEvents);
    return true;
}
void UShipWeaponComponent::ProtectWreckCamera()
{
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!PC || (PC->GetViewTarget()!=GetOwner() && PC->GetPawn()!=GetOwner()))return;
    if(WreckAnchor)if(auto* G=WreckAnchor->ResolveGeoreference())
    {
        const FVector Focus=G->TransformLongitudeLatitudeHeightPositionToUnreal(WreckStartLLH);
        const FVector Eye=G->TransformLongitudeLatitudeHeightPositionToUnreal(WreckStartLLH+FVector(0,0,800));
        if(auto* Camera=GetWorld()->SpawnActor<ACameraActor>(Eye,(Focus-Eye).Rotation()))PC->SetViewTarget(Camera);
    }
}
FString UShipWeaponComponent::DescribeDestructionFX() const
{
    if(!DestructionFX || !DestructionFX->GetAsset())return TEXT("No destruction FX");
    FString R=FString::Printf(TEXT("active=%d rendering=%d solo=%d visible=%d scale=%s\n"),DestructionFX->IsActive(),DestructionFX->GetRenderingEnabled(),DestructionFX->GetForceSolo(),DestructionFX->IsVisible(),*DestructionFX->GetComponentScale().ToString());
    for(const auto& H:DestructionFX->GetAsset()->GetEmitterHandles())
    {
        R+=FString::Printf(TEXT("asset emitter=%s enabled=%d allowed=%d\n"),*H.GetName().ToString(),H.GetIsEnabled(),H.IsAllowedByScalability());
        if(const auto* Data=H.GetEmitterData())for(auto* Renderer:Data->GetRenderers())
        {
            R+=FString::Printf(TEXT(" renderer=%s enabled=%d\n"),*GetNameSafe(Renderer),Renderer->GetIsEnabled());
            if(const auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(Renderer))
                R+=FString::Printf(TEXT(" material=%s distanceCull=%d range=%.0f..%.0f visibility=%u sizeBinding=%s colorBinding=%s\n"),*GetNameSafe(Sprite->Material),Sprite->bEnableCameraDistanceCulling,Sprite->MinCameraDistance,Sprite->MaxCameraDistance,Sprite->RendererVisibility,*Sprite->SpriteSizeBinding.GetDataSetBindableVariable().GetName().ToString(),*Sprite->ColorBinding.GetDataSetBindableVariable().GetName().ToString());
        }
    }
    const auto Controller=DestructionFX->GetSystemInstanceController();
    if(Controller.IsValid() && Controller->IsValid() && DestructionFX->GetForceSolo())
        if(auto* Instance=Controller->GetSoloSystemInstance())for(const auto& E:Instance->GetEmitters())
        {
            R+=FString::Printf(TEXT("sim emitter=%s particles=%d state=%d target=%d\n"),*E->GetEmitterHandle().GetName().ToString(),E->GetNumParticles(),int32(E->GetExecutionState()),int32(E->GetCachedEmitterData()->SimTarget));
            if(E->GetCachedEmitterData()->SimTarget==ENiagaraSimTarget::CPUSim && E->GetData().GetCurrentData() && E->GetData().GetCurrentData()->GetNumInstances()>0)
            {
                const auto& Data=E->GetData();
                const auto Size=FNiagaraDataSetAccessor<FVector2f>(Data,TEXT("SpriteSize")).GetReader(Data).GetSafe(0,FVector2f(-1));
                const auto Color=FNiagaraDataSetAccessor<FLinearColor>(Data,TEXT("Color")).GetReader(Data).GetSafe(0,FLinearColor(-1,-1,-1,-1));
                const auto Position=FNiagaraDataSetAccessor<FNiagaraPosition>(Data,TEXT("Position")).GetReader(Data).GetSafe(0,FNiagaraPosition());
                const int32 Visibility=FNiagaraDataSetAccessor<int32>(Data,TEXT("VisibilityTag")).GetReader(Data).GetSafe(0,-999);
                const float CameraOffset=FNiagaraDataSetAccessor<float>(Data,TEXT("CameraOffset")).GetReader(Data).GetSafe(0,-999);
                R+=FString::Printf(TEXT(" first size=%s color=%s position=%s local=%d tag=%d cameraOffset=%.2f\n"),*Size.ToString(),*Color.ToString(),*Position.ToString(),E->GetCachedEmitterData()->bLocalSpace,Visibility,CameraOffset);
            }
        }
    return R;
}
void UShipWeaponComponent::UpdateDestruction()
{
    const double Now=GetWorld()->GetTimeSeconds(),Dt=FMath::Max(0.,Now-LastDestructionUpdate);LastDestructionUpdate=Now;
    auto* Hull=UShipCombatComponent::PhysicalHull(GetOwner());if(!Hull)return;
    if(!bIsDestroyed)
    {
        if(!bEnableCapsizeDeath || !Hull->IsSimulatingPhysics())return;
        auto* Anchor=GetOwner()->FindComponentByClass<UCesiumGlobeAnchorComponent>();auto* G=Anchor?Anchor->ResolveGeoreference():nullptr;if(!G)return;
        const FVector LLH=G->TransformUnrealPositionToLongitudeLatitudeHeight(Hull->GetComponentLocation());
        const FVector Up=(G->TransformLongitudeLatitudeHeightPositionToUnreal(LLH+FVector(0,0,1))-Hull->GetComponentLocation()).GetSafeNormal();
        CurrentCapsizeAngle=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Up,Hull->GetUpVector()),-1.,1.)));
        CapsizeElapsed=CurrentCapsizeAngle>=FMath::Clamp(CapsizeAngleDegrees,90.f,175.f)?CapsizeElapsed+Dt:0;
        if(CapsizeElapsed>=FMath::Max(.1f,CapsizeConfirmSeconds))Die(EShipDeathReason::Capsized,nullptr);
        return;
    }
    const double Elapsed=Now-DeathStartedAt,Burn=FMath::Max(1.f,BurningSeconds),Sink=FMath::Max(3.f,SinkingSeconds);
    if(Elapsed<Burn){DestructionStage=EShipDestructionStage::Burning;return;}
    const double Progress=FMath::Clamp((Elapsed-Burn)/Sink,0.,1.);
    DestructionStage=Progress<1?EShipDestructionStage::Sinking:EShipDestructionStage::Submerged;
    SunkDistanceM=FMath::Max(20.f,SinkDepthM)*FMath::SmoothStep(0.,1.,Progress);
    if(WreckAnchor)
    {
        WreckAnchor->MoveToLongitudeLatitudeHeight(WreckStartLLH-FVector(0,0,SunkDistanceM),ECesiumHeightReference::Ellipsoid);
        // Preserve a capsized orientation in the geographic frame through rebases.
        WreckAnchor->SetEastSouthUpRotation(WreckESURotation);
    }
    if(Progress>=1)
    {
        if(DestructionFX && DestructionFX->IsActive())DestructionFX->Deactivate();
        Hull->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if(Elapsed>=Burn+Sink+FMath::Max(1.f,DestructionDelaySeconds))
        {DestructionStage=EShipDestructionStage::Cleanup;ProtectWreckCamera();GetOwner()->Destroy();}
    }
}
void UShipWeaponComponent::RefreshHealthUI()
{
    if(HealthWidget)if(auto* Widget=Cast<UShipHealthBarWidget>(HealthWidget->GetUserWidgetObject()))Widget->Refresh(this);
}
void UShipWeaponComponent::UpdatePresentation()
{
    auto* Pawn=Cast<APawn>(GetOwner());if(!Pawn)return;
    if(Pawn->IsPlayerControlled())
    {
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
    if(TargetActor)if(const auto* Combat=GetOwner()->FindComponentByClass<UShipCombatComponent>())
    {
        const auto* Hull=UShipCombatComponent::PhysicalHull(TargetActor);
        if(!Combat->IsHostile(TargetActor) || FVector::Distance(Start,Hull?Hull->Bounds.Origin:TargetActor->GetActorLocation())>Combat->EngagementRadiusM*100)
        {RejectionReason=TEXT("Invalid faction or target beyond engagement limit");return false;}
    }
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
    SetShootingSkill(ShootingSkill);
    EngagementRadiusM=FMath::Max(0.f,EngagementRadiusM);DetectionRadiusM=FMath::Max(DetectionRadiusM,EngagementRadiusM);
    SafeReengagementDistanceM=FMath::Max(SafeReengagementDistanceM,MinimumRetreatDistanceM+100.f);
    Navigation=GetOwner()->FindComponentByClass<UShipEnemyNavigationComponent>();Visual=Weapon?Weapon->Mesh():nullptr;
    HunterMaxPursuitDistanceM=FMath::Max(HunterMaxPursuitDistanceM,EngagementRadiusM);
    if(Navigation && Navigation->Role==EShipEnemyRole::Hunter)HunterSince=GetWorld()->GetTimeSeconds();
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
    if(Weapon && Weapon->bIsDestroyed)return false;
    if(!IsValid(Candidate) || Candidate==GetOwner() || Candidate->IsActorBeingDestroyed() || Candidate->ActorHasTag(TEXT("Dead")))return false;
    if(Faction==2)return false;
    if(const auto* W=Candidate->FindComponentByClass<UShipWeaponComponent>())return !W->bIsDestroyed && W->Team()!=Faction && W->Team()!=2;
    return false;
}
bool UShipCombatComponent::Visible(AActor* Candidate) const
{
    if(!Visual || !Candidate)return false;
    if(Navigation && !Navigation->WaterLineClear(Visual->GetSocketLocation(Weapon->MuzzleSocket),AimPoint(Candidate)))return false;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ShipCombatLOS),true,GetOwner());IgnoreWater(GetWorld(),Q);
    const FVector Start=Visual->GetSocketLocation(Weapon->MuzzleSocket),Delta=AimPoint(Candidate)-Start;
    const float Limit=FMath::Max(DetectionRadiusM,EngagementRadiusM)*100;
    if(Delta.Size()>Limit)return false;
    FHitResult Hit;return !GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+Delta.GetClampedToMaxSize(Limit),ECC_Visibility,Q) || Hit.GetActor()==Candidate;
}
void UShipCombatComponent::SetShootingSkill(EShipAISkill Skill)
{
    ShootingSkill=Skill;
    if(Skill==EShipAISkill::Easy)
    {AIAccuracy=.35f;AimErrorDegrees=3;TargetTrackingDelay=.65f;AimUpdateInterval=.65f;TargetAcquisitionSeconds=1;DistanceAccuracyPenalty=.9f;MovementAccuracyPenalty=.7f;}
    else if(Skill==EShipAISkill::Normal)
    {AIAccuracy=.6f;AimErrorDegrees=2.2f;TargetTrackingDelay=.35f;AimUpdateInterval=.4f;TargetAcquisitionSeconds=.6f;DistanceAccuracyPenalty=.7f;MovementAccuracyPenalty=.5f;}
    else if(Skill==EShipAISkill::Hard)
    {AIAccuracy=.9f;AimErrorDegrees=.35f;TargetTrackingDelay=.08f;AimUpdateInterval=.12f;TargetAcquisitionSeconds=.15f;DistanceAccuracyPenalty=.35f;MovementAccuracyPenalty=.2f;}
    // Unique deterministic streams prevent the fleet from sharing identical
    // misses. The editable seed still makes a fixed fixture reproducible.
    AimRandom.Initialize(AccuracySeed+FCrc::StrCrc32(*GetOwner()->GetName())%100000);
    AimHistory.Reset();ObservedTarget.Reset();LastAimUpdate=-1000;bCachedAimSolution=false;CurrentAimErrorDegrees=FVector2D::ZeroVector;
}
bool UShipCombatComponent::UpdateAimSolution(float Dt,FVector Start)
{
    const double Now=GetWorld()->GetTimeSeconds();
    if(ObservedTarget.Get()!=Target)
    {ObservedTarget=Target;TargetObservedAt=Now;LastAimUpdate=-1000;AimHistory.Reset();bCachedAimSolution=false;}
    AimHistory.Add({Now,AimPoint(Target),VelocityOf(Target)});
    while(AimHistory.Num()>128 || (AimHistory.Num()>1 && AimHistory[1].Time<Now-FMath::Clamp(TargetTrackingDelay,0.f,2.f)-.5))AimHistory.RemoveAt(0,1,EAllowShrinking::No);
    if(Now-TargetObservedAt<FMath::Max(0.f,TargetAcquisitionSeconds)){FiringStatus=TEXT("Acquiring target");return false;}
    if(Now-LastAimUpdate<FMath::Clamp(AimUpdateInterval,.05f,2.f))return bCachedAimSolution;
    LastAimUpdate=Now;const double SampleAt=Now-FMath::Clamp(TargetTrackingDelay,0.f,2.f);
    FAimObservation Observation=AimHistory[0];
    for(const auto& Sample:AimHistory){if(Sample.Time>SampleAt)break;Observation=Sample;}
    // Delayed position and motion estimate are used consistently. No correction
    // of projectile hits, target teleportation or inherited ship speed is added.
    bCachedAimSolution=Solve(Start,Observation.Position,Observation.Velocity,LaunchVelocity,PredictedIntercept);
    if(!bCachedAimSolution)return false;
    const float Distance=FVector::Distance(Start,Observation.Position)/100;
    const float Motion=(Observation.Velocity.Size()+VelocityOf(GetOwner()).Size())/4000;
    const float Error=FMath::Clamp(AimErrorDegrees,0.f,10.f)*(1-FMath::Clamp(AIAccuracy,0.f,1.f))*(1+FMath::Max(0.f,DistanceAccuracyPenalty)*Distance/FMath::Max(1.f,EngagementRadiusM)+FMath::Max(0.f,MovementAccuracyPenalty)*Motion+FMath::Max(0.f,OptionalSuppressionPenalty));
    // Uniform bounded disk; error is latched for an aim update, allowing the real
    // joints to settle. Both turret aim AND projectile launch use this vector.
    const float Angle=AimRandom.FRandRange(0,2*PI),Radius=FMath::Sqrt(AimRandom.FRand())*FMath::Min(Error,10.f);
    CurrentAimErrorDegrees=FVector2D(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius);++AimSamples;
    const FVector Up=Visual->GetUpVector();const FVector Right=FVector::CrossProduct(Up,LaunchVelocity.GetSafeNormal()).GetSafeNormal();
    LaunchVelocity=LaunchVelocity.RotateAngleAxis(CurrentAimErrorDegrees.X,Up).RotateAngleAxis(-CurrentAimErrorDegrees.Y,Right);
    return true;
}
void UShipCombatComponent::ScanTargets()
{
    if(!Weapon || !Visual || Weapon->bIsDestroyed){Target=nullptr;bTargetVisible=false;return;}
    const FVector Origin=PhysicalHull(GetOwner())?PhysicalHull(GetOwner())->GetComponentLocation():GetOwner()->GetActorLocation();
    AActor* Previous=Target;
    const bool Retain=IsHostile(Target) && FVector::Distance(Origin,AimPoint(Target))<=FMath::Max(LoseTargetRadiusM,DetectionRadiusM)*100;
    if(Retain && Visible(Target)){bTargetVisible=true;return;}
    Target=nullptr;bTargetVisible=false;double Best=DBL_MAX;
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
void UShipCombatComponent::ChangeRole(EShipEnemyRole NewRole,const TCHAR* Reason)
{
    if(!Navigation || !Weapon || Weapon->bIsDestroyed || Navigation->Role==NewRole)return;
    const auto Previous=Navigation->Role;const bool HadPursuit=bHunterPursuitActive;
    if(Navigation->HasTacticalDestination()){Navigation->ReturnToPatrol();++PatrolsResumed;}
    Navigation->Role=NewRole;bHunterPursuitActive=false;HunterLostSince=-1;LastHunterPlan=-1000;
    HunterSince=NewRole==EShipEnemyRole::Hunter?GetWorld()->GetTimeSeconds():-1;
    TacticalState=NewRole==EShipEnemyRole::Passive?EShipTacticalState::Passive:HadPursuit?EShipTacticalState::ReturnToPatrol:EShipTacticalState::Patrol;
    StateSince=GetWorld()->GetTimeSeconds();Target=nullptr;bTargetVisible=false;ScanClock=1000;
    ++RoleTransitions;LastRoleTransition=Reason;
    UE_LOG(LogTemp,Display,TEXT("ShipBattle role ship=%s from=%s to=%s reason=%s"),*GetNameSafe(GetOwner()),*UEnum::GetValueAsString(Previous),*UEnum::GetValueAsString(NewRole),Reason);
}
void UShipCombatComponent::SetCombatRole(EShipEnemyRole NewRole){ChangeRole(NewRole,TEXT("Tactical command"));}
void UShipCombatComponent::OnHostileHit(AActor* Source)
{
    if(Navigation && Navigation->Role==EShipEnemyRole::Passive && bPromotePassiveOnDamage)
    {ChangeRole(EShipEnemyRole::Fighter,TEXT("Hostile damage"));if(IsHostile(Source))Target=Source;}
}
bool UShipCombatComponent::RaiseThreatAlert(AActor* Threat)
{
    if(!Navigation || Navigation->Role!=EShipEnemyRole::Passive || !bPromotePassiveOnThreatAlert || !IsHostile(Threat) ||
        FVector::Distance(AimPoint(GetOwner()),AimPoint(Threat))>FMath::Max(0.f,ThreatAlertRadiusM)*100)return false;
    ChangeRole(EShipEnemyRole::Fighter,TEXT("Configured nearby threat alert"));Target=Threat;return true;
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
    if(Weapon->bIsDestroyed){Change(EShipTacticalState::Dead);return;}
    if(!bEnableCombat || Navigation->Role==EShipEnemyRole::Passive)
    {if(Navigation->HasTacticalDestination())Navigation->ReturnToPatrol();bHunterPursuitActive=false;Change(Navigation->Role==EShipEnemyRole::Passive?EShipTacticalState::Passive:EShipTacticalState::Patrol);return;}
    if(TacticalState==EShipTacticalState::Retreat || TacticalState==EShipTacticalState::Escape || TacticalState==EShipTacticalState::BackwardRetreat)
    {
        if(TacticalState==EShipTacticalState::BackwardRetreat &&
            (Navigation->bBackwardRetreatBlocked || Navigation->bBackwardRetreatCompleted))
        {
            if(Valid && DistanceToTargetM<SafeReengagementDistanceM && Navigation->BeginRetreatFrom(Target,SafeReengagementDistanceM))
            {++UnsafeReverseFallbacks;Change(EShipTacticalState::Escape);}
            else{Navigation->ReturnToPatrol();++PatrolsResumed;LastRetreatEnded=Now;Change(EShipTacticalState::ReturnToPatrol);}
            return;
        }
        if((Now-StateSince>=MinimumRetreatSeconds && (!Valid || DistanceToTargetM>=FMath::Max(SafeReengagementDistanceM,MinimumRetreatDistanceM+100))) || Now-StateSince>=FMath::Max(MaximumRetreatSeconds,MinimumRetreatSeconds))
        {Navigation->ReturnToPatrol();++PatrolsResumed;LastRetreatEnded=Now;Change(EShipTacticalState::ReturnToPatrol);}
        return;
    }
    if(Navigation->Role==EShipEnemyRole::Hunter)
    {
        if(HunterSince<0)HunterSince=Now;
        if(!Valid){if(HunterLostSince<0)HunterLostSince=Now;}
        else HunterLostSince=-1;
        const TCHAR* ExitReason=nullptr;
        if(Weapon->CurrentHealth<=Weapon->MaxHealth*FMath::Clamp(HunterToFighterHealthRatio,0.f,1.f))ExitReason=TEXT("Hunter low health");
        else if(Valid && DistanceToTargetM>HunterMaxPursuitDistanceM)ExitReason=TEXT("Hunter distance limit");
        else if(Now-HunterSince>=FMath::Max(3.f,HunterMaxPursuitSeconds))ExitReason=TEXT("Hunter pursuit timeout");
        else if(!Valid && HunterLostSince>=0 && Now-HunterLostSince>=FMath::Max(1.f,HunterLostTargetSeconds))ExitReason=TEXT("Hunter target lost");
        else if(bHunterPursuitActive && Engage && Now-HunterSince>=3)ExitReason=TEXT("Hunter reached engagement range");
        if(ExitReason)ChangeRole(EShipEnemyRole::Fighter,ExitReason);
        else if(Valid && !Engage)
        {
            if(Now-LastHunterPlan>=FMath::Max(1.f,HunterReplanSeconds))
            {
                LastHunterPlan=Now;
                if(Navigation->BeginPursuitOf(Target,FMath::Max(200.f,EngagementRadiusM*.75f)))
                {bHunterPursuitActive=true;++HunterRoutesStarted;}
                else ChangeRole(EShipEnemyRole::Fighter,TEXT("Hunter has no safe pursuit route"));
            }
            if(bHunterPursuitActive){Change(EShipTacticalState::Pursuit);return;}
        }
        else if(!Valid && bHunterPursuitActive){Change(EShipTacticalState::Pursuit);return;}
    }
    const bool Critical=Weapon->CurrentHealth<=FMath::Min(LowHealthRetreatThreshold,CriticalHealthThreshold);
    if(bEnableHitAndRun && IsHostile(Target) && (DistanceToTargetM<MinimumRetreatDistanceM || (Weapon->CurrentHealth<=LowHealthRetreatThreshold && DistanceToTargetM<=EngagementRadiusM)) &&
        Now-LastRetreatEnded>=(Critical?FMath::Min(5.f,RetreatCooldownSeconds):RetreatCooldownSeconds))
    {
        if(bPreferBackwardRetreat && Navigation->BeginBackwardRetreat(Target,BackwardRetreatDistanceM))
        {++RetreatsStarted;++BackwardRetreatsStarted;Change(EShipTacticalState::BackwardRetreat);return;}
        if(Navigation->BeginRetreatFrom(Target,FMath::Max(SafeReengagementDistanceM,MinimumRetreatDistanceM+100)))
        {++RetreatsStarted;if(bPreferBackwardRetreat)++UnsafeReverseFallbacks;Change(EShipTacticalState::Escape);return;}
    }
    if(TacticalState==EShipTacticalState::ReturnToPatrol)
    {if(Now-StateSince>=3)Change(IsHostile(Target) && DistanceToTargetM<=EngagementRadiusM?EShipTacticalState::Engage:EShipTacticalState::Patrol);return;}
    if(Now-StateSince>=3)Change(IsHostile(Target) && DistanceToTargetM<=EngagementRadiusM?EShipTacticalState::Engage:EShipTacticalState::Patrol);
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
        if(Navigation && !Navigation->WaterLineClear(Previous,Next)){Reason=TEXT("Trajectory crosses chart land/coastal buffer");return false;}
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
        Solution=UpdateAimSolution(Dt,Start);
        if(Solution)
        {
            // Full inverse component rotation compensates yaw, pitch AND roll.
            const FRotator Local=Visual->GetComponentTransform().InverseTransformVectorNoScale(LaunchVelocity).Rotation();
            DesiredTurretYaw=FRotator::NormalizeAxis(Local.Yaw);DesiredBarrelPitch=FRotator::NormalizeAxis(Local.Pitch);
            InArc=DesiredTurretYaw>=MinTurretYaw && DesiredTurretYaw<=MaxTurretYaw && DesiredBarrelPitch>=MinBarrelElevation && DesiredBarrelPitch<=MaxBarrelElevation;
        }
        else FiringStatus=TEXT("No ballistic intercept at original projectile speed");
    }
    else{Target=nullptr;ObservedTarget.Reset();AimHistory.Reset();bCachedAimSolution=false;DistanceToTargetM=0;DesiredTurretYaw=0;DesiredBarrelPitch=0;if(CanEngage)FiringStatus=TEXT("No hostile target");}
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
    if(auto* Hit=Impact.GetActor())if(auto* W=Hit->FindComponentByClass<UShipWeaponComponent>())W->ReceiveHullImpact(Projectile,Impact.ImpactPoint);
    for(TActorIterator<APawn> I(Projectile->GetWorld());I;++I)
    {
        if(*I==Impact.GetActor())continue;auto* W=I->FindComponentByClass<UShipWeaponComponent>();auto* Hull=UShipCombatComponent::PhysicalHull(*I);
        if(!W || !Hull || FVector::Distance(Hull->Bounds.Origin,Impact.ImpactPoint)>Round->SplashRadiusCm+Hull->Bounds.SphereRadius)continue;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(ShipSplash),true,Projectile);Q.AddIgnoredActor(Projectile->GetOwner());FHitResult Hit;
        if(!Projectile->GetWorld()->LineTraceSingleByChannel(Hit,Impact.ImpactPoint,Hull->Bounds.Origin,ECC_Visibility,Q) || Hit.GetActor()==*I)
            W->ReceiveShipDamage(Round->ProjectileDamage,Projectile);
    }
}
