#include "UI/NavalBattleUI.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Input/Events.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMinimapZoomModeTest, "Naval.Minimap.InteractiveZoom",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMinimapZoomModeTest::RunTest(const FString&)
{
    auto* Map = NewObject<UShipMinimapWidget>();
    TestEqual(TEXT("Overview starts at 1x"), Map->GetCurrentZoom(), 1.f);
    Map->SetInteractiveMode(true);
    TestTrue(TEXT("Interaction is enabled"), Map->IsInteractiveActive());
    TestTrue(TEXT("Entering interaction enables dynamic ship focus"), Map->IsFollowingShip());
    TestEqual(TEXT("Entering interaction immediately zooms to 9x"), Map->GetCurrentZoom(), 9.f);
    Map->SetMinimapZoom(3.f);
    TestEqual(TEXT("Intermediate zoom is preserved"), Map->GetCurrentZoom(), 3.f);
    Map->SetInteractiveMode(false);
    TestFalse(TEXT("Interaction is disabled"), Map->IsInteractiveActive());
    TestFalse(TEXT("Overview disables dynamic ship focus"), Map->IsFollowingShip());
    TestEqual(TEXT("Exiting interaction restores 1x"), Map->GetCurrentZoom(), 1.f);
    Map->SetMinimapZoom(99.f);
    TestEqual(TEXT("Maximum is 9x"), Map->GetCurrentZoom(), 9.f);
    Map->SetMinimapZoom(-1.f);
    TestEqual(TEXT("Minimum is 1x"), Map->GetCurrentZoom(), 1.f);
    Map->ToggleZoom();
    TestFalse(TEXT("Manual zoom does not restart tracking"), Map->IsFollowingShip());
    TestEqual(TEXT("Double-click zoom action reaches 9x"), Map->GetCurrentZoom(), 9.f);
    Map->ToggleZoom();
    TestEqual(TEXT("Double-click zoom action returns to 1x"), Map->GetCurrentZoom(), 1.f);
    Map->SetInteractiveMode(true);
    const FPointerEvent WheelUp(0, FVector2D(120,100), FVector2D(120,100),
        TSet<FKey>(), EKeys::Invalid, 1.f, FModifierKeysState());
    TestFalse(TEXT("Zoom without explicit map control passes mouse input through"),
        Map->NativeOnMouseWheel(FGeometry(), WheelUp).IsEventHandled());
    TestFalse(TEXT("Zoom does not enable map mouse control"), Map->IsMouseInteractionEnabled());
    TestEqual(TEXT("Map and its children are click-through during gameplay"),
        Map->GetVisibility(), ESlateVisibility::HitTestInvisible);
    const FPointerEvent FireClick(0, FVector2D(120,100), FVector2D(120,100),
        TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
    TestFalse(TEXT("Zoom cannot capture the firing mouse button"),
        Map->NativeOnMouseButtonDown(FGeometry(), FireClick).IsEventHandled());
    Map->SetMouseInteractionEnabled(true);
    TestTrue(TEXT("Explicit map control enables mouse interaction"), Map->IsMouseInteractionEnabled());
    Map->NativeOnMouseWheel(FGeometry(), WheelUp);
    TestTrue(TEXT("Wheel at maximum leaves following enabled"), Map->IsFollowingShip());
    const FPointerEvent WheelDown(0, FVector2D(120,100), FVector2D(120,100),
        TSet<FKey>(), EKeys::Invalid, -1.f, FModifierKeysState());
    Map->NativeOnMouseWheel(FGeometry(), WheelDown);
    TestFalse(TEXT("Effective manual wheel zoom suspends following"), Map->IsFollowingShip());
    Map->SetInteractiveMode(true);
    Map->SetMouseInteractionEnabled(true);
    Map->bIsDragging = true;
    Map->DragStartMouse = FVector2D(100,100);
    Map->DragStartPan = Map->PanOffset;
    Map->NativeOnMouseMove(FGeometry(), WheelDown);
    TestFalse(TEXT("Effective drag suspends following"), Map->IsFollowingShip());
    Map->SetMouseInteractionEnabled(false);
    Map->SetInteractiveMode(false);
    Map->SetInteractiveMode(true);
    TestTrue(TEXT("Reopening interaction resumes following"), Map->IsFollowingShip());
    TestFalse(TEXT("Reopening does not steal the mouse"), Map->IsMouseInteractionEnabled());
    Map->SetInteractiveMode(false);
    Map->SetMouseInteractionEnabled(true);
    TestTrue(TEXT("Alt enables map mouse control without M"), Map->IsMouseInteractionEnabled());
    TestEqual(TEXT("Alt alone keeps the overview at 1x"), Map->GetCurrentZoom(), 1.f);
    Map->SetInteractiveMode(true);
    TestTrue(TEXT("M preserves held Alt mouse control"), Map->IsMouseInteractionEnabled());
    Map->bIsDragging = true;
    Map->SetInteractiveMode(false);
    TestFalse(TEXT("Changing M ends the old drag gesture"), Map->bIsDragging);
    TestTrue(TEXT("Turning M off preserves held Alt mouse control"), Map->IsMouseInteractionEnabled());
    Map->SetMouseInteractionEnabled(false);
    TestEqual(TEXT("Releasing Alt restores click-through without changing zoom"),
        Map->GetVisibility(), ESlateVisibility::HitTestInvisible);
    return true;
}
#endif
