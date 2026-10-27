#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShipDetailsUI.generated.h"

class APawn;
class STextBlock;

/** C++ HUD panel that displays live data for the ship selected by CustomPlayerController. */
UCLASS()
class VDTPHASE2PROJECT_API UShipDetailsUI : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Updates the fallback target; normally the widget follows the controller's active selection. */
    void SetSelectedShip(APawn* InSelectedShip);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    APawn* ResolveSelectedShip() const;
    void UpdateDetails();

    TWeakObjectPtr<APawn> SelectedShip;

    TSharedPtr<STextBlock> ShipNameText;
    TSharedPtr<STextBlock> LongitudeText;
    TSharedPtr<STextBlock> LatitudeText;
    TSharedPtr<STextBlock> SpeedText;
    TSharedPtr<STextBlock> HealthText;
    TSharedPtr<STextBlock> DirectionText;
};
