#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TeamCountBarWidget.generated.h"

/** Top-center HUD bar that shows how many ships each team has left. */
UCLASS()
class VDTPHASE2PROJECT_API UTeamCountBarWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    FText GetBlueCountText() const;
    FText GetRedCountText() const;
};
