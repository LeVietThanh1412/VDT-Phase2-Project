#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "KillFeedWidget.generated.h"

class SVerticalBox;
class UShipWeaponComponent;

/** Top-left kill feed: one white-to-transparent row per eliminated ship. */
UCLASS()
class VDTPHASE2PROJECT_API UKillFeedWidget : public UUserWidget {
  GENERATED_BODY()

protected:
  virtual TSharedRef<SWidget> RebuildWidget() override;
  virtual void NativeConstruct() override;
  virtual void NativeDestruct() override;
  virtual void NativeTick(const FGeometry &MyGeometry,
                          float InDeltaTime) override;

private:
  UFUNCTION()
  void HandleShipDestroyed(UShipWeaponComponent *Ship, AActor *Attacker);

  void BindShips();
  void AddNotification(const FText &AttackerName,
                       const FLinearColor &AttackerColor,
                       const FText &VictimName, const FLinearColor &VictimColor,
                       bool bHasAttacker);

  TSharedPtr<SVerticalBox> FeedBox;
  TArray<TWeakObjectPtr<UShipWeaponComponent>> BoundShips;
  double NextBindTime = 0.0;

  struct FEntry {
    TSharedPtr<SWidget> Row;
    double ExpiresAt = 0.0;
  };
  TArray<FEntry> Entries;
};
