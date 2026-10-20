#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShipPhysicsProfile.generated.h"
class USkeletalMesh;
class UStaticMesh;
class UPhysicsAsset;
class UAnimInstance;

UENUM(BlueprintType)
enum class EShipHullClass : uint8 { Small, Large };

/** Replace art/collision here when real hull variants become available. No mesh scaling. */
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
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Buoyancy") float PontoonRadiusMultiplier=1;
    /** Empty preserves the verified existing four-pontoon layout. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Buoyancy") TArray<FVector> PontoonLocations;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Future assets") TObjectPtr<USkeletalMesh> VisualMesh;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Future assets") TSubclassOf<UAnimInstance> AnimationClass;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Future assets") TObjectPtr<UPhysicsAsset> VisualPhysicsAsset;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Future assets") TObjectPtr<UStaticMesh> HullCollisionMesh;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Future assets") FVector CameraOffset=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Future assets") float ProjectileSpeedCmps=20000;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Future assets") float WeaponCooldownSeconds=8;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Future assets") float PlayerShotCooldownSeconds=0;
    void SetDefaults(EShipHullClass Type);
};
