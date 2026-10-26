#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NavalBattleUI.generated.h"

class UShipWeaponComponent;
class UImage;
class UProgressBar;
class UTextBlock;

/** Adds naval team/death presentation to the map team's existing marker widgets. */
UCLASS()
class VDTPHASE2PROJECT_API UShipMinimapWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
private:
    void RefreshNavalMarkers();
    FTimerHandle MarkerTimer;
};

/** Native marker base: keeps WBP_ShipMarker's icon in sync with its ship's TeamName. */
UCLASS(Blueprintable)
class VDTPHASE2PROJECT_API UShipMarkerWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
private:
    UPROPERTY(Transient) TObjectPtr<UImage> MarkerImage;
};

/** Presentation is inherited by the saved WBP assets; all values are event driven. */
UCLASS()
class VDTPHASE2PROJECT_API UShipHealthBarWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Refresh(UShipWeaponComponent* Ship);
    UPROPERTY(BlueprintReadOnly) float DisplayedFraction=1;
    UPROPERTY(BlueprintReadOnly) FLinearColor HealthColor=FLinearColor::Green;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
private:
    UPROPERTY(Transient) TObjectPtr<UShipWeaponComponent> BoundShip;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> Bar;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Label;
};
