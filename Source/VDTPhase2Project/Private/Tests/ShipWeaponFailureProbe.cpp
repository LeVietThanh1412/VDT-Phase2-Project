#include "Tests/ShipWeaponFailureProbe.h"
#include "Combat/ShipCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AShipWeaponFailureProbe::AShipWeaponFailureProbe() {
  PrimaryActorTick.bCanEverTick = false;
  Ship = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Ship"));
  SetRootComponent(Ship);
  Ship->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Ship->SetVisibility(false);
  Weapon = CreateDefaultSubobject<UShipWeaponComponent>(TEXT("Weapon"));
  Weapon->Faction = 2;
}
void AShipWeaponFailureProbe::ExecuteWeaponShot(FVector LaunchVelocity,
                                                bool bBallistic) {
  ++SpawnAttempts;
  // Intentionally no actor allocation: the dispatch receives the same null
  // completion as a failed original BP spawn. It must restore the input gate.
  Weapon->FinishShot(nullptr);
}
FString UShipCombatLibrary::VerifyMagazineFailurePath(APawn *TemplateShip) {
#if WITH_EDITOR
  auto *Source =
      IsValid(TemplateShip)
          ? TemplateShip->FindComponentByClass<UShipWeaponComponent>()
          : nullptr;
  auto *Mesh = Source ? Source->Mesh() : nullptr;
  if (!Mesh || !Mesh->GetSkeletalMeshAsset())
    return TEXT("ERROR: fixture requires a real ship muzzle");
  UWorld *World = TemplateShip->GetWorld();
  const FTransform Transform(FVector(0, 0, 100000));
  auto *Probe = World->SpawnActorDeferred<AShipWeaponFailureProbe>(
      AShipWeaponFailureProbe::StaticClass(), Transform, nullptr, nullptr,
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if (!Probe)
    return TEXT("ERROR: could not create transient fixture");
  Probe->Ship->SetSkeletalMeshAsset(Mesh->GetSkeletalMeshAsset());
  UGameplayStatics::FinishSpawningActor(Probe, Transform);
  auto *W = Probe->Weapon.Get();
  const int32 Before = W->RoundsRemaining;
  const bool First = W->TryFireWeapon(FVector::ZeroVector, nullptr,
                                      FVector::ZeroVector, false);
  const FString Rejection = W->RejectionReason;
  const bool Second = W->TryFireWeapon(FVector::ZeroVector, nullptr,
                                       FVector::ZeroVector, false);
  const bool OK =
      !First && !Second && Probe->SpawnAttempts == 2 && Probe->CanShoot &&
      W->RoundsRemaining == Before && Before == 10 && W->ShotsFired == 0 &&
      W->ReloadsStarted == 0 && !W->bReloading &&
      Rejection == TEXT("Original Blueprint projectile spawn failed");
  const FString Result = FString::Printf(
      TEXT("{\"status\":\"%s\",\"scope\":\"Transient null-projectile producer "
           "through actual shared dispatch; not allocator-failure "
           "injection\",\"attempts\":%d,\"rounds_before\":%d,\"rounds_after\":%"
           "d,\"shots\":%d,\"input_gate_restored\":%s}"),
      OK ? TEXT("PASSED") : TEXT("FAILED"), Probe->SpawnAttempts, Before,
      W->RoundsRemaining, W->ShotsFired,
      Probe->CanShoot ? TEXT("true") : TEXT("false"));
  Probe->Destroy();
  return Result;
#else
  return TEXT("Editor-only contract fixture");
#endif
}
