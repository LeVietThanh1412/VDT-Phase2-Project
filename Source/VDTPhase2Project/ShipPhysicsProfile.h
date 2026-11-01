#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShipPhysicsProfile.generated.h"
class USkeletalMesh;
class UStaticMesh;
class UPhysicsAsset;
class UAnimInstance;

UENUM(BlueprintType)
enum class EShipHullClass : uint8 { Small, Large, Medium };

/** Shared balance and independently fitted visual/physical equipment; actor scale stays one. */
UCLASS(BlueprintType)
class VDTPHASE2PROJECT_API UShipPhysicsProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Class") EShipHullClass HullClass=EShipHullClass::Small;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics",meta=(ClampMin="10")) float MassKg=150;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics") FVector InertiaScale=FVector(1);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics") FVector CenterOfMassOffset=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics") float LinearDamping=.5f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics") float AngularDamping=.5f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement") float PropulsionScale=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement") float RudderScale=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement") float ForwardMaxSpeed=26;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement") float ReverseMaxSpeed=7;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement") float CruiseSpeed=20;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Buoyancy") float DisplacementMultiplier=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Buoyancy") float VerticalDampingMultiplier=1;
    /** Passive water resistance in both vertical directions; does not resist forward motion. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Buoyancy|Stability",meta=(ClampMin="0",ClampMax="60")) float HeaveDampingPerSecond=20;
    /** Hull-local angular resistance. Yaw remains governed by the original rudder/body damping. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Buoyancy|Stability",meta=(ClampMin="0",ClampMax="60")) float RollDampingPerSecond=30;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Buoyancy|Stability",meta=(ClampMin="0",ClampMax="60")) float PitchDampingPerSecond=15;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Buoyancy") float PontoonRadiusMultiplier=1;
    /** Empty preserves the verified existing four-pontoon layout. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Buoyancy") TArray<FVector> PontoonLocations;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Future assets") TObjectPtr<USkeletalMesh> VisualMesh;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Future assets") TSubclassOf<UAnimInstance> AnimationClass;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Future assets") TObjectPtr<UPhysicsAsset> VisualPhysicsAsset;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Future assets") TObjectPtr<UStaticMesh> HullCollisionMesh;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Equipment") TObjectPtr<UStaticMesh> StaticVisualMesh;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Equipment") TObjectPtr<UStaticMesh> GunYawMesh;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Equipment") TObjectPtr<UStaticMesh> GunPitchMesh;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Equipment") FTransform StaticVisualTransform;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Equipment") FTransform WeaponDriverTransform;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Equipment") float RudderLocationScale=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Equipment") float CameraDistanceScale=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Equipment") float WreckEffectScale=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Balance") float MaxHealth=100;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Balance") float BaseProjectileDamage=10;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Future assets") FVector CameraOffset=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Future assets") float ProjectileSpeedCmps=20000;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Future assets") float WeaponCooldownSeconds=8;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Future assets") float PlayerShotCooldownSeconds=0;
    void SetDefaults(EShipHullClass Type);
};
