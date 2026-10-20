#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShipPhysicsProfile.h"
#include "BuoyancyTypes.h"
#include "ShipMovementComponent.generated.h"

/** Filters the force supplied to the original Blueprint AddForce. Never applies force itself. */
UCLASS(ClassGroup=(Movement),meta=(BlueprintSpawnableComponent))
class VDTPHASE2PROJECT_API UShipMovementComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ship class") EShipHullClass HullClass=EShipHullClass::Small;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ship class") TObjectPtr<UShipPhysicsProfile> Profile;
    UPROPERTY(BlueprintReadOnly,Category="Ship class") bool bProfileApplied=false;
    UPROPERTY(BlueprintReadOnly,Category="Ship class") float AppliedPropulsionScale=1;
    UPROPERTY(BlueprintReadOnly,Category="Ship class") float AppliedRudderScale=1;
    UFUNCTION(BlueprintCallable,Category="Ship class") bool ApplyShipClass(EShipHullClass Type);
    UFUNCTION(BlueprintCallable,Category="Ship class") bool ApplyProfile(UShipPhysicsProfile* NewProfile);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Propulsion",meta=(ClampMin="1",Units="m/s")) float ForwardMaxSpeed=26;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Propulsion",meta=(ClampMin="1",Units="m/s")) float ReverseMaxSpeed=7;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Propulsion",meta=(ClampMin="0.05",ClampMax="0.95")) float ReverseForceRatio=.45f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Propulsion",meta=(ClampMin="0.1",Units="m/s")) float SpeedLimitBlendMps=1;
    UFUNCTION(BlueprintPure,Category="Propulsion") float SignedForwardSpeed() const;
    UFUNCTION(BlueprintPure,Category="Propulsion") static FVector FilterPropulsion(AActor* Ship,FVector OriginalForce);
    UFUNCTION(BlueprintPure,Category="Propulsion") static FVector FilterRudder(AActor* Ship,FVector OriginalForce);
    UFUNCTION(BlueprintCallable,Category="Propulsion|Editor",meta=(DevelopmentOnly)) static FString InstallPropulsionFilter();
    UFUNCTION(BlueprintCallable,Category="Ship class|Editor",meta=(DevelopmentOnly)) static FString CreateDefaultProfiles();
protected:
    virtual void BeginPlay() override;
private:
    bool bCachedBuoyancy=false;
    FBuoyancyData OriginalBuoyancy;
    bool bCachedCamera=false;
    FVector OriginalCameraSocketOffset=FVector::ZeroVector;
    bool bCachedBody=false;
    FVector OriginalCOMOffset=FVector::ZeroVector,OriginalInertiaScale=FVector(1);
};
