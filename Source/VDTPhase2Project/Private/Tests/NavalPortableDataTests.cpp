#include "Navigation/NavalLocalTerrainComponent.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavalFileUriTest, "Naval.PortableData.FileUri",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNavalFileUriTest::RunTest(const FString& Parameters) {
  TestEqual(TEXT("Different drive and spaces"),
      UNavalLocalTerrainComponent::ToFileUrl(TEXT("D:\\Games\\Ship War\\Content\\layer.json")),
      FString(TEXT("file:///D:/Games/Ship%20War/Content/layer.json")));
  TestEqual(TEXT("URI fragment and percent are literal filename characters"),
      UNavalLocalTerrainComponent::ToFileUrl(TEXT("F:/Ship #1/100%/layer.json")),
      FString(TEXT("file:///F:/Ship%20%231/100%25/layer.json")));
  TestEqual(TEXT("UTF8 path encoding"),
      UNavalLocalTerrainComponent::ToFileUrl(TEXT("E:/Đảo/layer.json")),
      FString(TEXT("file:///E:/%C4%90%E1%BA%A3o/layer.json")));
  TestEqual(TEXT("UNC share"),
      UNavalLocalTerrainComponent::ToFileUrl(TEXT("\\\\server\\share\\Ship War\\layer.json")),
      FString(TEXT("file://server/share/Ship%20War/layer.json")));
  TestTrue(TEXT("Relative process-directory paths rejected"),
      UNavalLocalTerrainComponent::ToFileUrl(TEXT("Content/layer.json")).IsEmpty());
  TestTrue(TEXT("Drive-relative paths rejected"),
      UNavalLocalTerrainComponent::ToFileUrl(TEXT("E:Content/layer.json")).IsEmpty());
  return true;
}
#endif
