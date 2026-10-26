#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ShipPhysicsProfile.h"
#include "ShipCombatComponent.h"
#include "NavalMatchSettings.generated.h"

UENUM(BlueprintType)
enum class ENavalBattleMode : uint8 { RealTime, TurnBased };

/** Selections survive level travel; an untouched direct PIE uses saved defaults. */
UCLASS()
class VDTPHASE2PROJECT_API UNavalMatchSettings : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly) int32 BlueCount=3;
    UPROPERTY(BlueprintReadOnly) int32 RedCount=3;
    UPROPERTY(BlueprintReadOnly) TArray<EShipHullClass> HullClasses={EShipHullClass::Small,EShipHullClass::Small,EShipHullClass::Small,EShipHullClass::Small,EShipHullClass::Small,EShipHullClass::Small};
    UPROPERTY(BlueprintReadOnly) EShipAISkill Difficulty=EShipAISkill::Normal;
    UPROPERTY(BlueprintReadOnly) ENavalBattleMode Mode=ENavalBattleMode::RealTime;
    UPROPERTY(BlueprintReadOnly) bool bExplicitSelection=false;
    UFUNCTION(BlueprintCallable) bool Configure(int32 Blue,int32 Red,const TArray<EShipHullClass>& Classes,EShipAISkill Skill,ENavalBattleMode BattleMode);
};
