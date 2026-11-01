#include "ShipMovementComponent.h"
#include "ShipCombatComponent.h"
#include "NavalBattleUI.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "BuoyancyComponent.h"
#include "WaterBodyComponent.h"
#include "ShipEnemyNavigationComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Physics/SimpleSuspension.h"
#include "GameFramework/SpringArmComponent.h"

namespace
{
FVector Forward(AActor* Ship)
{
    TArray<USceneComponent*> Components;Ship->GetComponents(Components);
    for(auto* C:Components)if(C->GetFName()==TEXT("Arrow"))return C->GetForwardVector();
    return Ship->GetActorForwardVector();
}
}
UShipMovementComponent::UShipMovementComponent()
{PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PrePhysics;}
void UShipMovementComponent::BeginPlay()
{
    Super::BeginPlay();if(Profile)ApplyProfile(Profile);else ApplyShipClass(HullClass);
}
bool UShipMovementComponent::ApplyShipClass(EShipHullClass Type)
{
    const TCHAR* Path=Type==EShipHullClass::Large?TEXT("/Game/NavalBattle/Profiles/DA_LargeShip.DA_LargeShip"):Type==EShipHullClass::Medium?TEXT("/Game/NavalBattle/Profiles/DA_MediumShip.DA_MediumShip"):TEXT("/Game/NavalBattle/Profiles/DA_SmallShip.DA_SmallShip");
    auto* P=LoadObject<UShipPhysicsProfile>(nullptr,Path);
    if(!P){P=NewObject<UShipPhysicsProfile>(this);P->SetDefaults(Type);}
    return ApplyProfile(P);
}
bool UShipMovementComponent::ApplyProfile(UShipPhysicsProfile* P)
{
    auto* Hull=UShipCombatComponent::PhysicalHull(GetOwner());
    const auto* W=GetOwner()->FindComponentByClass<UShipWeaponComponent>();if(!P || !Hull || (W && W->bIsDestroyed))return false;
    DampingHull=Hull;DampingBuoyancy=GetOwner()->FindComponentByClass<UBuoyancyComponent>();
    DampingWeapon=GetOwner()->FindComponentByClass<UShipWeaponComponent>();
    ApplyEquipment(P);
    if(auto* Visual=GetOwner()->FindComponentByClass<USkeletalMeshComponent>())
    {
        if(P->VisualMesh)Visual->SetSkeletalMesh(P->VisualMesh);
        if(P->AnimationClass)Visual->SetAnimInstanceClass(P->AnimationClass);
        if(P->VisualPhysicsAsset)Visual->SetPhysicsAsset(P->VisualPhysicsAsset);
    }
    if(auto* Body=Hull->GetBodyInstance())
    {if(!bCachedBody){OriginalCOMOffset=Body->COMNudge;OriginalInertiaScale=Body->InertiaTensorScale;bCachedBody=true;}Body->InertiaTensorScale=OriginalInertiaScale*P->InertiaScale.ComponentMax(FVector(.1));}
    Hull->SetMassOverrideInKg(NAME_None,FMath::Max(10.f,P->MassKg));Hull->SetCenterOfMass(OriginalCOMOffset+P->CenterOfMassOffset);
    Hull->SetLinearDamping(FMath::Max(0.f,P->LinearDamping));Hull->SetAngularDamping(FMath::Max(0.f,P->AngularDamping));
    if(auto* Body=Hull->GetBodyInstance())Body->UpdateMassProperties();
    if(auto* B=GetOwner()->FindComponentByClass<UBuoyancyComponent>())
    {
        // Match setup selects hull classes after BeginPlay. The Water async aux
        // snapshots scalar settings only once per body; use its supported sync
        // path so the selected profile, not the initial Blueprint, supplies lift.
        B->bUseAsyncPath=false;
        if(!bCachedBuoyancy){OriginalBuoyancy=B->BuoyancyData;bCachedBuoyancy=true;}
        B->BuoyancyData=OriginalBuoyancy;
        B->BuoyancyData.BuoyancyCoefficient*=FMath::Max(.001f,P->DisplacementMultiplier);
        B->BuoyancyData.MaxBuoyantForce*=FMath::Max(.001f,P->DisplacementMultiplier);
        B->BuoyancyData.BuoyancyDamp*=FMath::Max(.1f,P->VerticalDampingMultiplier);
        B->BuoyancyData.BuoyancyDamp2*=FMath::Max(.1f,P->VerticalDampingMultiplier);
        if(!P->PontoonLocations.IsEmpty())
        {
            B->BuoyancyData.Pontoons.Reset();
            for(const auto& Location:P->PontoonLocations){FSphericalPontoon Pontoon;Pontoon.RelativeLocation=Location;Pontoon.Radius=100;B->BuoyancyData.Pontoons.Add(Pontoon);}
        }
        for(auto& Pontoon:B->BuoyancyData.Pontoons)Pontoon.Radius*=FMath::Clamp(P->PontoonRadiusMultiplier,.2f,5.f);
        B->UpdatePontoonCoefficients();
        // UE caches weights by enabled mask alone. A replaced 4->4 layout can
        // otherwise retain default weights of 1 (four times the intended lift).
        // Reuse the engine's sprung-mass solver with the actual hull-local COM.
        if(const auto* Body=Hull->GetBodyInstance())
        {
            TArray<FVector> Locations;TArray<float> Weights;
            for(const auto& Pontoon:B->BuoyancyData.Pontoons)if(Pontoon.bEnabled)
                Locations.Add(Pontoon.bUseCenterSocket?Hull->GetSocketTransform(Pontoon.CenterSocket,RTS_Component).GetLocation():Pontoon.RelativeLocation);
            Weights.SetNumZeroed(Locations.Num());
            if(!FSimpleSuspensionHelpers::ComputeSprungMasses(Locations,Body->GetMassSpaceLocal().GetLocation(),1.f,Weights))return false;
            float WeightSum=0;for(float Weight:Weights){if(!FMath::IsFinite(Weight) || Weight<0)return false;WeightSum+=Weight;}
            if(!FMath::IsNearlyEqual(WeightSum,1.f,.001f))return false;
            int32 Index=0;for(auto& Pontoon:B->BuoyancyData.Pontoons)if(Pontoon.bEnabled)Pontoon.PontoonCoefficient=Weights[Index++];
        }
    }
    Profile=P;HullClass=P->HullClass;AppliedPropulsionScale=FMath::Max(.1f,P->PropulsionScale);AppliedRudderScale=FMath::Max(.1f,P->RudderScale);
    ForwardMaxSpeed=FMath::Max(1.f,P->ForwardMaxSpeed);ReverseMaxSpeed=FMath::Max(1.f,P->ReverseMaxSpeed);
    if(auto* N=GetOwner()->FindComponentByClass<UShipEnemyNavigationComponent>())N->CruiseSpeedMps=FMath::Max(1.f,P->CruiseSpeed);
    if(auto* Weapon=GetOwner()->FindComponentByClass<UShipWeaponComponent>())
    {
        const bool Full=Weapon->CurrentHealth>=Weapon->MaxHealth;
        Weapon->MaxHealth=FMath::Max(1.f,P->MaxHealth);Weapon->CurrentHealth=Full?Weapon->MaxHealth:FMath::Min(Weapon->CurrentHealth,Weapon->MaxHealth);
        Weapon->BaseProjectileDamage=FMath::Max(0.f,P->BaseProjectileDamage);
        Weapon->MuzzleSpeedCmps=FMath::Max(1.f,P->ProjectileSpeedCmps);Weapon->ShotCooldownSeconds=FMath::Max(0.f,P->PlayerShotCooldownSeconds);
    }
    if(auto* Combat=GetOwner()->FindComponentByClass<UShipCombatComponent>())Combat->FireCooldownSeconds=FMath::Max(.1f,P->WeaponCooldownSeconds);
    if(auto* Arm=GetOwner()->FindComponentByClass<USpringArmComponent>())
    {if(!bCachedCamera){OriginalCameraSocketOffset=Arm->SocketOffset;bCachedCamera=true;}Arm->SocketOffset=OriginalCameraSocketOffset+P->CameraOffset;}
    if(auto* N=GetOwner()->FindComponentByClass<UShipEnemyNavigationComponent>())
    {N->MaxHealth=P->MaxHealth;if(auto* Weapon=GetOwner()->FindComponentByClass<UShipWeaponComponent>())N->Health=Weapon->CurrentHealth;N->RefreshHullGeometry();}
    if(auto* Weapon=GetOwner()->FindComponentByClass<UShipWeaponComponent>())Weapon->RefreshShipTags();
    bProfileApplied=true;return true;
}
FString UShipMovementComponent::GetBuoyancyDiagnostics() const
{
    if(!IsValid(DampingBuoyancy) || !IsValid(DampingHull))return TEXT("{}");
    float WeightSum=0;for(const auto& Pontoon:DampingBuoyancy->BuoyancyData.Pontoons)if(Pontoon.bEnabled)WeightSum+=Pontoon.PontoonCoefficient;
    return FString::Printf(TEXT("{\"async\":%s,\"pontoons\":%d,\"weight_sum\":%.6f,\"mass_kg\":%.3f}"),
        DampingBuoyancy->IsUsingAsyncPath()?TEXT("true"):TEXT("false"),DampingBuoyancy->BuoyancyData.Pontoons.Num(),WeightSum,DampingHull->GetMass());
}
void UShipMovementComponent::ApplyEquipment(UShipPhysicsProfile* P)
{
    auto* Hull=Cast<UStaticMeshComponent>(UShipCombatComponent::PhysicalHull(GetOwner()));
    auto* Driver=GetOwner()->FindComponentByClass<USkeletalMeshComponent>();if(!Hull || !Driver)return;
    if(!bCachedEquipment)
    {
        const bool AuthoredSmall=Driver->ComponentHasTag(TEXT("NavalSmallDriver"));
        if(AuthoredSmall)OriginalHull=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Ship/Assets/SM/untitled.untitled"));else OriginalHull=Hull->GetStaticMesh();
        OriginalVisualTransform=AuthoredSmall?FTransform::Identity:Driver->GetRelativeTransform();
        OriginalVisualCollision=AuthoredSmall?ECollisionEnabled::QueryAndPhysics:Driver->GetCollisionEnabled();bCachedEquipment=true;
    }
    const bool Small=P->StaticVisualMesh!=nullptr;
    SetComponentTickEnabled(Small || P->HeaveDampingPerSecond>0 || P->RollDampingPerSecond>0 || P->PitchDampingPerSecond>0);
    Hull->SetStaticMesh(P->HullCollisionMesh?P->HullCollisionMesh.Get():OriginalHull.Get());
    Driver->SetRelativeTransform(Small?P->WeaponDriverTransform:OriginalVisualTransform);
    Driver->SetHiddenInGame(Small);Driver->SetVisibility(!Small,false);
    Driver->SetCollisionEnabled(Small?ECollisionEnabled::NoCollision:OriginalVisualCollision);
    Driver->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    AddTickPrerequisiteComponent(Driver);
    auto Part=[&](TObjectPtr<UStaticMeshComponent>& Component,FName Name,UStaticMesh* Mesh)
    {
        if(!Component && Mesh)
        {
            TArray<UStaticMeshComponent*> Existing;GetOwner()->GetComponents(Existing);
            for(auto* Item:Existing)if(Item->GetFName()==Name){Component=Item;break;}
            if(!Component)
            {
                Component=NewObject<UStaticMeshComponent>(GetOwner(),Name);GetOwner()->AddInstanceComponent(Component);
                Component->SetupAttachment(Hull);Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Component->SetCanEverAffectNavigation(false);Component->RegisterComponent();
            }
        }
        if(Component){Component->SetStaticMesh(Mesh);Component->SetVisibility(Small,false);Component->SetHiddenInGame(!Small);}
    };
    Part(BoatVisual,TEXT("SmallBoatVisual"),P->StaticVisualMesh);
    Part(GunYaw,TEXT("SmallGunYaw"),P->GunYawMesh);Part(GunPitch,TEXT("SmallGunPitch"),P->GunPitchMesh);
    if(BoatVisual)BoatVisual->SetRelativeTransform(P->StaticVisualTransform);
    // Equipment can be selected between ticks. Put the visible parts at the
    // current firing bones immediately, including a shot in that same frame.
    if(Small)
    {
        if(GunYaw)GunYaw->SetWorldTransform(Driver->GetSocketTransform(TEXT("front_gun_h_jnt")));
        if(GunPitch)GunPitch->SetWorldTransform(Driver->GetSocketTransform(TEXT("front_gun_v_jnt")));
    }
    TArray<USpringArmComponent*> Arms;GetOwner()->GetComponents(Arms);
    for(auto* Arm:Arms)
    {
        float* Original=OriginalArmLengths.Find(Arm);
        if(!Original){Original=&OriginalArmLengths.Add(Arm,Arm->TargetArmLength);OriginalArmTransforms.Add(Arm,Arm->GetRelativeTransform());OriginalArmSockets.Add(Arm,Arm->GetAttachSocketName());}
        Arm->TargetArmLength=*Original*P->CameraDistanceScale;
        if(Arm->GetFName()==TEXT("SpringArm"))
        {
            Arm->SetRelativeTransform(OriginalArmTransforms[Arm]);
            if(Small){Arm->TargetArmLength=9000;Arm->SetRelativeLocation(FVector(0,0,2200));}
        }
        if(Arm->GetFName()==TEXT("GunSpringArm"))
        {
            Arm->AttachToComponent(Driver,FAttachmentTransformRules::KeepRelativeTransform,Small?FName(TEXT("front_gun_v_jnt")):OriginalArmSockets[Arm]);
            Arm->SetRelativeTransform(OriginalArmTransforms[Arm]);
            if(Small){Arm->TargetArmLength=0;Arm->SetRelativeLocation(FVector(-500,0,400));}
        }
    }
}
void UShipMovementComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime,TickType,TickFunction);
    ApplyWaterResistance(DeltaTime);
    if(!Profile || !Profile->StaticVisualMesh)return;
    if(auto* Driver=GetOwner()->FindComponentByClass<USkeletalMeshComponent>())
    {
        if(GunYaw)GunYaw->SetWorldTransform(Driver->GetSocketTransform(TEXT("front_gun_h_jnt")));
        if(GunPitch)GunPitch->SetWorldTransform(Driver->GetSocketTransform(TEXT("front_gun_v_jnt")));
    }
}
void UShipMovementComponent::ApplyWaterResistance(float DeltaTime)
{
    if(!IsValid(Profile) || !IsValid(DampingHull) || !IsValid(DampingBuoyancy) ||
       (IsValid(DampingWeapon) && DampingWeapon->bIsDestroyed) || !DampingHull->IsSimulatingPhysics() ||
       !FMath::IsFinite(DeltaTime) || DeltaTime<=0)return;
    bool Immersed=DampingBuoyancy->IsInWaterBody();
    // All spherical supports can briefly clear a trough while the keel is still
    // submerged. Dropping resistance in that frame caused a free-fall/rebound.
    // Query only that uncommon case, and never retain water drag on a dry hull.
    if(!Immersed && DampingBuoyancy->IsOverlappingWaterBody())
    {
        for(UWaterBodyComponent* Water:DampingBuoyancy->GetCurrentWaterBodyComponents())
        {
            if(!IsValid(Water))continue;
            FVector Surface,Normal,Flow;float Depth;
            Water->GetWaterSurfaceInfoAtLocation(DampingHull->GetComponentLocation(),Surface,Normal,Flow,Depth);
            if(DampingHull->Bounds.Origin.Z-DampingHull->Bounds.BoxExtent.Z<Surface.Z){Immersed=true;break;}
        }
    }
    if(!Immersed)return;
    const FVector Velocity=DampingHull->GetPhysicsLinearVelocity();
    const FVector AngularVelocity=DampingHull->GetPhysicsAngularVelocityInRadians();
    if(Velocity.ContainsNaN() || AngularVelocity.ContainsNaN())return;
    // Exponential drag expressed as acceleration: frame-safe at low FPS, with no
    // velocity assignment. Chaos integrates force/torque; do not multiply by dt.
    const auto Rate=[DeltaTime](float PerSecond)
    {return FMath::IsFinite(PerSecond)?(1.f-FMath::Exp(-FMath::Clamp(PerSecond,0.f,60.f)*DeltaTime))/DeltaTime:0.f;};
    DampingHull->AddForce(FVector(0,0,-DampingHull->GetMass()*Velocity.Z*Rate(Profile->HeaveDampingPerSecond)));
    const FQuat Rotation=DampingHull->GetComponentQuat();
    const FVector LocalOmega=Rotation.UnrotateVector(AngularVelocity);
    const FVector AngularDrag(-LocalOmega.X*Rate(Profile->RollDampingPerSecond),-LocalOmega.Y*Rate(Profile->PitchDampingPerSecond),0);
    DampingHull->AddTorqueInRadians(Rotation.RotateVector(AngularDrag),NAME_None,true);
}
FVector UShipMovementComponent::FilterRudderLocation(AActor* Ship,FVector OriginalLocation)
{
    if(const auto* M=IsValid(Ship)?Ship->FindComponentByClass<UShipMovementComponent>():nullptr)
        if(M->Profile)return OriginalLocation*M->Profile->RudderLocationScale;
    return OriginalLocation;
}
FVector UShipMovementComponent::FilterRudder(AActor* Ship,FVector Force)
{
    if(!IsValid(Ship) || Force.ContainsNaN())return FVector::ZeroVector;
    auto* M=Ship->FindComponentByClass<UShipMovementComponent>();
    if(M)M->LastRudderForce=FVector::ZeroVector;
    if(!UNavalBattleSubsystem::CanShipAct(Ship))return FVector::ZeroVector;
    if(const auto* W=Ship->FindComponentByClass<UShipWeaponComponent>();W && W->bIsDestroyed)return FVector::ZeroVector;
    if(M)
    {M->LastRudderForce=Force*M->AppliedRudderScale;return M->LastRudderForce;}
    return Force;
}
float UShipMovementComponent::SignedForwardSpeed() const
{
    auto* Hull=UShipCombatComponent::PhysicalHull(GetOwner());
    return Hull?FVector::DotProduct(Hull->GetPhysicsLinearVelocity(),Forward(GetOwner()))/100:0;
}
FVector UShipMovementComponent::FilterPropulsion(AActor* Ship,FVector Force)
{
    if(!IsValid(Ship) || Force.ContainsNaN())return FVector::ZeroVector;
    auto* M=Ship->FindComponentByClass<UShipMovementComponent>();
    if(M)M->LastPropulsionForce=FVector::ZeroVector;
    if(!UNavalBattleSubsystem::CanShipAct(Ship))return FVector::ZeroVector;
    if(const auto* W=Ship->FindComponentByClass<UShipWeaponComponent>();W && W->bIsDestroyed)return FVector::ZeroVector;
    if(!M)return Force;
    const float Speed=M->SignedForwardSpeed(),Thrust=FVector::DotProduct(Force,Forward(Ship));
    const float Blend=FMath::Max(.1f,M->SpeedLimitBlendMps);
    float Scale=1;
    if(Thrust<0)
    {
        // Opposite thrust brakes forward motion at full strength. Apply the
        // reverse reduction exactly once as the hull crosses through zero.
        Scale=FMath::Lerp(FMath::Clamp(M->ReverseForceRatio,.05f,.95f),1.f,FMath::Clamp(Speed/.5f,0.f,1.f));
        if(Speed<0)Scale*=FMath::Clamp((FMath::Max(1.f,M->ReverseMaxSpeed)+Speed)/Blend,0.f,1.f);
    }
    else if(Thrust>0 && Speed>0)
        Scale=FMath::Clamp((FMath::Max(1.f,M->ForwardMaxSpeed)-Speed)/Blend,0.f,1.f);
    M->LastPropulsionForce=Force*Scale*M->AppliedPropulsionScale;return M->LastPropulsionForce;
}
