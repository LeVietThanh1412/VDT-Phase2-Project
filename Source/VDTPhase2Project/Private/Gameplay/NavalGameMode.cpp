#include "Gameplay/NavalGameMode.h"
#include "Gameplay/FreeCameraPawn.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/NavalMatchSettings.h"

UClass *ANavalGameMode::GetDefaultPawnClassForController_Implementation(
    AController *InController) {
  const auto *Settings =
      GetGameInstance() ? GetGameInstance()->GetSubsystem<UNavalMatchSettings>()
                        : nullptr;
  if (Settings && Settings->bFullAIDemo)
    return AFreeCameraPawn::StaticClass();
  const int32 PlayerSlot =
      Settings ? FMath::Clamp(Settings->PlayerTeam, 0, 1) * 3 : 0;
  if (Settings && Settings->HullClasses.Num() == 6 &&
      Settings->HullClasses[PlayerSlot] == EShipHullClass::Small)
    if (auto *Small = LoadClass<APawn>(
            nullptr,
            TEXT("/Game/ShipSmall/BluePrint/BP_ShipSmall.BP_ShipSmall_C")))
      return Small;
  return Super::GetDefaultPawnClassForController_Implementation(InController);
}
