#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "LoadingScreenWidget.generated.h"

/** Full-screen "preparing battlefield" overlay shown between Start Match and
 * the ready demo. */
UCLASS()
class VDTPHASE2PROJECT_API ULoadingScreenWidget : public UUserWidget {
  GENERATED_BODY()

protected:
  virtual TSharedRef<SWidget> RebuildWidget() override;

private:
  FText GetLoadingText() const;
};
