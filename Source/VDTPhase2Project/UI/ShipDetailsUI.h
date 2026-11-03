#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShipDetailsUI.generated.h"

class APawn;
class STextBlock;
class FReply;
struct EVisibility;

/** C++ HUD panel that displays live data for the ship selected by CustomPlayerController. */
UCLASS()
class VDTPHASE2PROJECT_API UShipDetailsUI : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Updates the fallback target; normally the widget follows the controller's active selection. */
    void SetSelectedShip(APawn* InSelectedShip);
    UPROPERTY(BlueprintReadOnly) FString AmmoText;
    UPROPERTY(BlueprintReadOnly) FString IdentityText;
    UPROPERTY(BlueprintReadOnly) FString BattleText;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<APawn> DisplayedShip;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    APawn* ResolveSelectedShip() const;
    void UpdateDetails();
    EVisibility GetMatchResultVisibility() const;
    FText GetMatchResultText() const;
    FReply ReturnToSetup();

    TWeakObjectPtr<APawn> SelectedShip;

    TSharedPtr<STextBlock> ShipNameText;
    TSharedPtr<STextBlock> LongitudeText;
    TSharedPtr<STextBlock> LatitudeText;
    TSharedPtr<STextBlock> SpeedText;
    TSharedPtr<STextBlock> HealthText;
    TSharedPtr<STextBlock> DirectionText;
    TSharedPtr<STextBlock> MagazineText;
};
