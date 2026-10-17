#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShipEnemyProjectile.generated.h"
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/** Enemy-only equivalent of the inspected cannon projectile, with owner-safe damage. */
UCLASS()
class VDTPHASE2PROJECT_API AShipEnemyProjectile : public AActor
{
    GENERATED_BODY()
public:
    AShipEnemyProjectile();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UProjectileMovementComponent> Movement;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ExposeOnSpawn=true)) FVector LaunchVelocity=FVector::ZeroVector;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float Damage=10000;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float DamageRadiusCm=1000;
protected:
    virtual void BeginPlay() override;
private:
    UFUNCTION() void OnStopped(const FHitResult& Hit);
};
