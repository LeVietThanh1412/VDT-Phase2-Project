#include "ShipMovementComponent.h"
#include "ShipCombatComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "BuoyancyComponent.h"
#include "ShipEnemyNavigationComponent.h"
#include "PhysicsEngine/BodyInstance.h"
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
void UShipMovementComponent::BeginPlay()
{
    Super::BeginPlay();if(Profile)ApplyProfile(Profile);else ApplyShipClass(HullClass);
}
bool UShipMovementComponent::ApplyShipClass(EShipHullClass Type)
{
    const TCHAR* Path=Type==EShipHullClass::Large?TEXT("/Game/NavalBattle/Profiles/DA_LargeShip.DA_LargeShip"):TEXT("/Game/NavalBattle/Profiles/DA_SmallShip.DA_SmallShip");
    auto* P=LoadObject<UShipPhysicsProfile>(nullptr,Path);
    if(!P){P=NewObject<UShipPhysicsProfile>(this);P->SetDefaults(Type);}
    return ApplyProfile(P);
}
bool UShipMovementComponent::ApplyProfile(UShipPhysicsProfile* P)
{
    auto* Hull=UShipCombatComponent::PhysicalHull(GetOwner());
    const auto* W=GetOwner()->FindComponentByClass<UShipWeaponComponent>();if(!P || !Hull || (W && W->bIsDestroyed))return false;
    if(P->HullCollisionMesh)if(auto* Static=Cast<UStaticMeshComponent>(Hull))Static->SetStaticMesh(P->HullCollisionMesh);
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
        if(!bCachedBuoyancy){OriginalBuoyancy=B->BuoyancyData;bCachedBuoyancy=true;}
        B->BuoyancyData=OriginalBuoyancy;
        B->BuoyancyData.BuoyancyCoefficient*=FMath::Max(.1f,P->DisplacementMultiplier);
        B->BuoyancyData.MaxBuoyantForce*=FMath::Max(.1f,P->DisplacementMultiplier);
        B->BuoyancyData.BuoyancyDamp*=FMath::Max(.1f,P->VerticalDampingMultiplier);
        B->BuoyancyData.BuoyancyDamp2*=FMath::Max(.1f,P->VerticalDampingMultiplier);
        if(!P->PontoonLocations.IsEmpty())
        {
            B->BuoyancyData.Pontoons.Reset();
            for(const auto& Location:P->PontoonLocations){FSphericalPontoon Pontoon;Pontoon.RelativeLocation=Location;Pontoon.Radius=100;B->BuoyancyData.Pontoons.Add(Pontoon);}
        }
        for(auto& Pontoon:B->BuoyancyData.Pontoons)Pontoon.Radius*=FMath::Clamp(P->PontoonRadiusMultiplier,.2f,5.f);
        B->UpdatePontoonCoefficients();
    }
    Profile=P;HullClass=P->HullClass;AppliedPropulsionScale=FMath::Max(.1f,P->PropulsionScale);AppliedRudderScale=FMath::Max(.1f,P->RudderScale);
    ForwardMaxSpeed=FMath::Max(1.f,P->ForwardMaxSpeed);ReverseMaxSpeed=FMath::Max(1.f,P->ReverseMaxSpeed);
    if(auto* N=GetOwner()->FindComponentByClass<UShipEnemyNavigationComponent>())N->CruiseSpeedMps=FMath::Max(1.f,P->CruiseSpeed);
    if(auto* Weapon=GetOwner()->FindComponentByClass<UShipWeaponComponent>())
    {Weapon->MuzzleSpeedCmps=FMath::Max(1.f,P->ProjectileSpeedCmps);Weapon->ShotCooldownSeconds=FMath::Max(0.f,P->PlayerShotCooldownSeconds);}
    if(auto* Combat=GetOwner()->FindComponentByClass<UShipCombatComponent>())Combat->FireCooldownSeconds=FMath::Max(.1f,P->WeaponCooldownSeconds);
    if(auto* Arm=GetOwner()->FindComponentByClass<USpringArmComponent>())
    {if(!bCachedCamera){OriginalCameraSocketOffset=Arm->SocketOffset;bCachedCamera=true;}Arm->SocketOffset=OriginalCameraSocketOffset+P->CameraOffset;}
    bProfileApplied=true;return true;
}
FVector UShipMovementComponent::FilterRudder(AActor* Ship,FVector Force)
{
    if(!IsValid(Ship) || Force.ContainsNaN())return FVector::ZeroVector;
    if(const auto* W=Ship->FindComponentByClass<UShipWeaponComponent>();W && W->bIsDestroyed)return FVector::ZeroVector;
    if(const auto* M=Ship->FindComponentByClass<UShipMovementComponent>())return Force*M->AppliedRudderScale;
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
    if(const auto* W=Ship->FindComponentByClass<UShipWeaponComponent>();W && W->bIsDestroyed)return FVector::ZeroVector;
    const auto* M=Ship->FindComponentByClass<UShipMovementComponent>();if(!M)return Force;
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
    return Force*Scale*M->AppliedPropulsionScale;
}
