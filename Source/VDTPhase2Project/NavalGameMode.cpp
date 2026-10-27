#include "NavalGameMode.h"
#include "NavalMatchSettings.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"

UClass* ANavalGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    const auto* Settings=GetGameInstance()?GetGameInstance()->GetSubsystem<UNavalMatchSettings>():nullptr;
    if(Settings && Settings->HullClasses.Num()==6 && Settings->HullClasses[0]==EShipHullClass::Small)
        if(auto* Small=LoadClass<APawn>(nullptr,TEXT("/Game/ShipSmall/BluePrint/BP_ShipSmall.BP_ShipSmall_C")))return Small;
    return Super::GetDefaultPawnClassForController_Implementation(InController);
}
