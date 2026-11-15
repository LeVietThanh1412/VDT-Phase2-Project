#include "Gameplay/CustomPlayerController.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/ShipCombatComponent.h"
#include "Gameplay/FreeCameraPawn.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "UI/ButtonTutorialsUI.h"
#include "Components/Button.h"
#include "Input/Events.h"
#include "Widgets/SWidget.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControllerShipCycleTest, "Naval.Controls.TeammateCycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FControllerShipCycleTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.OwningGameInstance = Instance;
    Context.SetCurrentWorld(World);
    World->SetGameInstance(Instance);
    ON_SCOPE_EXIT {
        World->BeginTearingDown();
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    };
    FURL URL;
    URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
    World->SetGameMode(URL);
    World->InitializeActorsForPlay(URL);
    auto* Controller = World->SpawnActor<ACustomPlayerController>();
    auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    Controller->Player = LocalPlayer;
    LocalPlayer->PlayerController = Controller;
    auto SpawnShip = [World]() {
        auto* Ship = World->SpawnActor<APawn>();
        auto* Root = NewObject<USceneComponent>(Ship);
        Ship->SetRootComponent(Root);
        Root->RegisterComponent();
        auto* Weapon = NewObject<UShipWeaponComponent>(Ship);
        Ship->AddInstanceComponent(Weapon);
        Weapon->RegisterComponent();
        return Ship;
    };
    APawn* First = SpawnShip();
    APawn* Second = SpawnShip();
    APawn* Enemy = SpawnShip();
    Enemy->FindComponentByClass<UShipWeaponComponent>()->Faction = 1;
    Controller->ViewedShip = First;
    Controller->NextTeammate();
    TestEqual(TEXT("Tab advances to the other living teammate"), Controller->GetSelectedShip(), Second);
    Controller->NextTeammate();
    TestEqual(TEXT("Tab wraps to the first teammate"), Controller->GetSelectedShip(), First);
    auto* Guide = NewObject<UButtonTutorialsUI>(World);
    Guide->SetOwningPlayer(Controller);
    Guide->TakeWidget();
    const FKeyEvent Tab(EKeys::Tab, FModifierKeysState(), 0, false, 0, 0);
    const FKeyEvent Q(EKeys::Q, FModifierKeysState(), 0, false, 0, 0);
    TestTrue(TEXT("Focused help button consumes Tab as Slate navigation"),
        Guide->HelpButton->TakeWidget()->OnKeyDown(FGeometry(), Tab).IsEventHandled());
    TestFalse(TEXT("Focused help button leaves Q unhandled"),
        Guide->HelpButton->TakeWidget()->OnKeyDown(FGeometry(), Q).IsEventHandled());
    TestTrue(TEXT("HUD forwards Tab before button navigation consumes it"),
        Guide->NativeOnPreviewKeyDown(FGeometry(), Tab).IsEventHandled());
    TestEqual(TEXT("Tab still changes ship with help-button focus"), Controller->GetSelectedShip(), Second);
    Guide->NativeOnPreviewKeyDown(FGeometry(), Tab);
    const FKeyEvent RepeatedTab(EKeys::Tab, FModifierKeysState(), 0, true, 0, 0);
    Guide->NativeOnPreviewKeyDown(FGeometry(), RepeatedTab);
    TestEqual(TEXT("Held Tab does not repeatedly switch ships"), Controller->GetSelectedShip(), First);
    TestFalse(TEXT("HUD does not intercept Q"), Guide->NativeOnPreviewKeyDown(FGeometry(), Q).IsEventHandled());
    Second->FindComponentByClass<UShipWeaponComponent>()->bIsDestroyed = true;
    Controller->FreeCameraPawn = World->SpawnActor<AFreeCameraPawn>();
    Controller->bFullAISpectating = true;
    Controller->bBattlefieldOverview = true;
    Controller->ActiveShipViewCamera = nullptr;
    Controller->SetViewTarget(Controller->FreeCameraPawn);
    Controller->NextTeammate();
    TestFalse(TEXT("Tab leaves overview even when only the selected ship survives"), Controller->bBattlefieldOverview);
    TestNotNull(TEXT("Tab restores the surviving ship camera"), Controller->ActiveShipViewCamera.Get());
    TestEqual(TEXT("Tab skips the destroyed teammate"), Controller->GetSelectedShip(), First);
    return true;
}
#endif
