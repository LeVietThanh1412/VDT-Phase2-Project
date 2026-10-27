#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ButtonTutorialsUI.generated.h"

class UBorder;
class UButton;

/** Bottom-right controls guide toggled by the "?" button or the ? key. */
UCLASS()
class VDTPHASE2PROJECT_API UButtonTutorialsUI : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Tutorial")
    void ToggleTutorial();

    UFUNCTION(BlueprintPure, Category="Tutorial")
    bool IsTutorialVisible() const { return bTutorialVisible; }

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    UFUNCTION()
    void HandleHelpClicked();

    UPROPERTY(Transient)
    TObjectPtr<UBorder> TutorialPanel;

    UPROPERTY(Transient)
    TObjectPtr<UButton> HelpButton;

    bool bTutorialVisible = false;
};
