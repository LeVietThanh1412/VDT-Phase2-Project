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
    UPROPERTY(BlueprintReadOnly) TArray<EShipHullClass> HullClasses={EShipHullClass::Small,EShipHullClass::Small,EShipHullClass::Small,EShipHullClass::Small,EShipHullClass::Large,EShipHullClass::Medium};
    UPROPERTY(BlueprintReadOnly) EShipAISkill Difficulty=EShipAISkill::Normal;
    UPROPERTY(BlueprintReadOnly) ENavalBattleMode Mode=ENavalBattleMode::RealTime;
    /** 0 = Blue, 1 = Red. The player always occupies the first slot of this team. */
    UPROPERTY(BlueprintReadOnly) int32 PlayerTeam=0;
    UPROPERTY(BlueprintReadOnly) bool bExplicitSelection=false;
    UPROPERTY(BlueprintReadWrite,Category="Match") bool bFullAIDemo=false;
    /** Standard presentation balance; class physics/meshes remain unchanged. */
    UPROPERTY(BlueprintReadWrite,Category="Match") bool bStandardDemoBalance=true;
    UPROPERTY(BlueprintReadWrite,Category="Battlefield",meta=(ClampMin="0")) int32 BattlefieldSeed=20261028;
    UPROPERTY(BlueprintReadWrite,Category="Battlefield") bool bUseSectorBattlefield=true;
    UPROPERTY(BlueprintReadWrite,Category="Battlefield") bool bEnableSectorTactics=true;
    UPROPERTY(BlueprintReadWrite,Category="Battlefield") bool bEnableRedAssault=true;
    UPROPERTY(BlueprintReadWrite,Category="Battlefield") bool bUseIslandPatrolCircuit=true;
    UFUNCTION(BlueprintCallable,Category="Battlefield") bool SetBattlefieldSeed(int32 Seed);
    UFUNCTION(BlueprintCallable) bool Configure(int32 Blue,int32 Red,const TArray<EShipHullClass>& Classes,EShipAISkill Skill,ENavalBattleMode BattleMode,int32 InPlayerTeam=0);
};
