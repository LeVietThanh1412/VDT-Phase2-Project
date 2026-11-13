#include "UI/NavalBattleUI.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMinimapZoomModeTest, "Naval.Minimap.InteractiveZoom",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMinimapZoomModeTest::RunTest(const FString&)
{
    auto* Map = NewObject<UShipMinimapWidget>();
    TestEqual(TEXT("Overview starts at 1x"), Map->GetCurrentZoom(), 1.f);
    Map->SetInteractiveMode(true);
    TestTrue(TEXT("Interaction is enabled"), Map->IsInteractiveActive());
    TestEqual(TEXT("Entering interaction immediately zooms to 9x"), Map->GetCurrentZoom(), 9.f);
    Map->SetMinimapZoom(3.f);
    TestEqual(TEXT("Intermediate zoom is preserved"), Map->GetCurrentZoom(), 3.f);
    Map->SetInteractiveMode(false);
    TestFalse(TEXT("Interaction is disabled"), Map->IsInteractiveActive());
    TestEqual(TEXT("Exiting interaction restores 1x"), Map->GetCurrentZoom(), 1.f);
    Map->SetMinimapZoom(99.f);
    TestEqual(TEXT("Maximum is 9x"), Map->GetCurrentZoom(), 9.f);
    Map->SetMinimapZoom(-1.f);
    TestEqual(TEXT("Minimum is 1x"), Map->GetCurrentZoom(), 1.f);
    Map->ToggleZoom();
    TestEqual(TEXT("Double-click zoom action reaches 9x"), Map->GetCurrentZoom(), 9.f);
    Map->ToggleZoom();
    TestEqual(TEXT("Double-click zoom action returns to 1x"), Map->GetCurrentZoom(), 1.f);
    return true;
}
#endif
