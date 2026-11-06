#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MatchSummaryWidget.generated.h"

/** Full-screen end-of-match scoreboard, kept separate from the live ship HUD. */
UCLASS()
class VDTPHASE2PROJECT_API UMatchSummaryWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    FText GetMatchResultText() const;
    FLinearColor GetOutcomeColor() const;
    FReply ReturnToSetup();
    FReply RestartMatch();
    FReply QuitMatch();
};
