#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShipSetupWidget.generated.h"

class UComboBoxString;
class UTextBlock;
class UEditableTextBox;
class UVerticalBox;
class UButton;

/** Selections prepare a fresh match, reusing the map's ships and patrol actors. */
UCLASS()
class VDTPHASE2PROJECT_API UShipSetupWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) void StartMatch();
    UFUNCTION(BlueprintCallable) void Cancel();
    UFUNCTION(BlueprintCallable) bool ApplySelection();
    UFUNCTION(BlueprintCallable) void RefreshSlots();
    UFUNCTION(BlueprintCallable) void UseMyMap();
    UFUNCTION(BlueprintCallable) void GenerateFromSeed();
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UComboBoxString> BlueCountSelector;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UComboBoxString> RedCountSelector;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UComboBoxString> DifficultySelector;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UComboBoxString> PlayerTeamSelector;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UComboBoxString> ModeSelector;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UComboBoxString> ControlSelector;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UComboBoxString> BalanceSelector;
    /** Internal compatibility selector: 0 = generated, 1 = authored. The UI uses direct buttons. */
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UComboBoxString> NavigationSelector;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UButton> UseMyMapButton;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UButton> GenerateFromSeedButton;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UTextBlock> LayoutDescription;
    UPROPERTY(BlueprintReadOnly) TArray<TObjectPtr<UComboBoxString>> HullSelectors;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UEditableTextBox> SeedInput;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
    void RefreshLayout();
    UFUNCTION() void OnQuitClicked();
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Error;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> QuitLabel;
    /** Second row of Deploy Troops; only visible for generated layouts. */
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> SeedRow;
    /** Player team picker; only visible when Control Mode includes a player. */
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> PlayerTeamColumn;
    UPROPERTY(Transient) TArray<TObjectPtr<UVerticalBox>> SlotRows;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> SlotLabels;
    int32 LastBlue=-1,LastRed=-1,LastPlayerTeam=-1,LastControl=-1,LastLayout=-1;
    bool bInitialSetup=true;
};
