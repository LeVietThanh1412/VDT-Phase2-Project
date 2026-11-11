#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ShipWeaponFailureProbe.generated.h"
class USkeletalMeshComponent;
class UShipWeaponComponent;

/** Transient editor contract fixture. Produces no projectile, as a failed spawn
 * would. */
UCLASS(Transient, NotPlaceable)
class AShipWeaponFailureProbe : public APawn {
  GENERATED_BODY()
public:
  AShipWeaponFailureProbe();
  UPROPERTY() TObjectPtr<USkeletalMeshComponent> Ship;
  UPROPERTY() TObjectPtr<UShipWeaponComponent> Weapon;
  UPROPERTY() bool CanShoot = true;
  int32 SpawnAttempts = 0;
  UFUNCTION() void ExecuteWeaponShot(FVector LaunchVelocity, bool bBallistic);
};
