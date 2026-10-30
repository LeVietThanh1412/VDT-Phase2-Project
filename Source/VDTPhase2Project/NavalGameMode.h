#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NavalGameMode.generated.h"

/** Keeps the existing GameMode Blueprint and selects its Small child at spawn. */
UCLASS()
class VDTPHASE2PROJECT_API ANavalGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
};
