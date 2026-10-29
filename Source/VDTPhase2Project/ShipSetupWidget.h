#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShipSetupWidget.generated.h"

class UComboBoxString;
class UTextBlock;
class UEditableTextBox;

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
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UComboBoxString> BlueCountSelector;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UComboBoxString> RedCountSelector;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UComboBoxString> DifficultySelector;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UComboBoxString> ModeSelector;
    UPROPERTY(BlueprintReadOnly) TArray<TObjectPtr<UComboBoxString>> HullSelectors;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UEditableTextBox> SeedInput;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Error;
    int32 LastBlue=-1,LastRed=-1;
};
