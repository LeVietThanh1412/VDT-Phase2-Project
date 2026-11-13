#pragma once

#include "Components/MaterialBillboardComponent.h"
#include "CoreMinimal.h"
#include "ShipWreckBillboardComponent.generated.h"

class UShipWeaponComponent;

/** A small camera-facing fire/smoke fallback driven by the existing death
 * event. */
UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class VDTPHASE2PROJECT_API UShipWreckBillboardComponent
    : public UMaterialBillboardComponent {
  GENERATED_BODY()
public:
  UShipWreckBillboardComponent();
  /** Centimeters along hull heading/right/geographic up, relative to hull root.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wreck effect")
  FVector DeckOffsetCm = FVector(0, 0, 2500);
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wreck effect")
  float Phase = 0;
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wreck effect")
  bool bWreckVisualActive = false;
  UFUNCTION(BlueprintCallable, Category = "Wreck effect")
  void ActivateForWreck();

protected:
  virtual void BeginPlay() override;
  virtual void EndPlay(const EEndPlayReason::Type Reason) override;
  virtual void
  TickComponent(float DeltaTime, ELevelTick TickType,
                FActorComponentTickFunction *TickFunction) override;

private:
  TWeakObjectPtr<UShipWeaponComponent> Weapon;
  UFUNCTION()
  void HandleDestroyed(UShipWeaponComponent *Ship, AActor *Attacker);
  void UpdatePlacement();
  void HideEffect();
};
