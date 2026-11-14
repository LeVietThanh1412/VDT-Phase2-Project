#include "Navigation/NavalLocalTerrainComponent.h"

#if WITH_EDITOR
#include "Cesium3DTileset.h"
#include "CesiumTileMapServiceRasterOverlay.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_Literal.h"
#include "K2Node_VariableSet.h"
#include "Kismet/BlueprintPathsLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

FString UNavalLocalTerrainComponent::InstallLevelBlueprintPaths(
    UObject* WorldContextObject, bool bApply) {
#if WITH_EDITOR
  UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
  if (!World || World->IsGameWorld()) return TEXT("ERROR: editor world required");
  auto* BP = World->PersistentLevel->GetLevelScriptBlueprint();
  auto* Graph = BP ? FBlueprintEditorUtils::FindEventGraph(BP) : nullptr;
  if (!Graph) return TEXT("ERROR: existing Level Blueprint EventGraph required");
  FString Report;
  UK2Node_Event* Begin = nullptr;
  bool bInstalled = false;
  for (UEdGraphNode* Node : Graph->Nodes) {
    Report += Node->GetNodeTitle(ENodeTitleType::ListView).ToString() + TEXT("\n");
    bInstalled |= Node->NodeComment == TEXT("Content terrain URL initialization");
    if (auto* Event = Cast<UK2Node_Event>(Node))
      if (Event->EventReference.GetMemberName() == TEXT("ReceiveBeginPlay")) Begin = Event;
  }
  if (!bApply) return Report;
  if (bInstalled) return TEXT("ERROR: Content terrain nodes already installed");
  if (!Begin || !Begin->FindPin(UEdGraphSchema_K2::PN_Then) ||
      Begin->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Num() > 1)
    return TEXT("ERROR: expected existing BeginPlay with at most one continuation");
  ACesium3DTileset* Terrain = nullptr;
  UCesiumTileMapServiceRasterOverlay* Overlay = nullptr;
  for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    if (It->GetActorLabel() == TEXT("QuangNinh_Terrain")) {
      if (Terrain) return TEXT("ERROR: ambiguous QuangNinh terrain actor");
      Terrain = *It;
      Overlay = Terrain->FindComponentByClass<UCesiumTileMapServiceRasterOverlay>();
    }
  if (!Terrain || !Overlay) return TEXT("ERROR: terrain actor or imagery missing");
  BP->Modify(); Graph->Modify(); Begin->Modify();
  const auto Before = Graph->Nodes;
  auto* Then = Begin->FindPin(UEdGraphSchema_K2::PN_Then);
  const auto Continuation = Then->LinkedTo;
  const auto* Schema = GetDefault<UEdGraphSchema_K2>();
  const int32 X = Begin->NodePosX + 350, Y = Begin->NodePosY - 800;
  auto Init = [&](UEdGraphNode* Node, int32 DX, int32 DY) {
    Graph->AddNode(Node, false, false); Node->CreateNewGuid();
    Node->AllocateDefaultPins(); Node->NodePosX = X + DX; Node->NodePosY = Y + DY;
  };
  auto Call = [&](UClass* Class, FName Name, int32 DX, int32 DY) {
    auto* Node = NewObject<UK2Node_CallFunction>(Graph);
    Node->SetFromFunction(Class->FindFunctionByName(Name)); Init(Node, DX, DY);
    return Node;
  };
  bool bWired = true;
  auto Link = [&](UEdGraphPin* A, UEdGraphPin* B) {
    bWired &= A && B && Schema->TryCreateConnection(A, B);
  };
  auto* Content = Call(UKismetSystemLibrary::StaticClass(),
                      TEXT("GetProjectContentDirectory"), 0, 0);
  Content->NodeComment = TEXT("Content terrain URL initialization");
  Content->bCommentBubbleVisible = true;
  auto* Ref = NewObject<UK2Node_Literal>(Graph);
  Ref->SetObjectRef(Terrain); Init(Ref, 800, 650);
  auto* GetOverlay = Call(AActor::StaticClass(), TEXT("GetComponentByClass"), 1100, 650);
  GetOverlay->FindPin(TEXT("ComponentClass"))->DefaultObject =
      UCesiumTileMapServiceRasterOverlay::StaticClass();
  GetOverlay->PinDefaultValueChanged(GetOverlay->FindPin(TEXT("ComponentClass")));
  Link(Ref->GetValuePin(), GetOverlay->FindPin(UEdGraphSchema_K2::PN_Self));
  UK2Node_VariableSet* Sets[2];
  for (int32 Index = 0; Index < 2; ++Index) {
    const int32 Row = Index * 300;
    auto* Append = Call(UKismetStringLibrary::StaticClass(), TEXT("Concat_StrStr"), 300, Row);
    Append->FindPin(TEXT("B"))->DefaultValue = Index == 0
        ? TEXT("UnrealTerrainSet/QuangNinh/Terrain/layer.json")
        : TEXT("UnrealTerrainSet/QuangNinh/Imagery/tilemapresource.xml");
    Link(Content->GetReturnValuePin(), Append->FindPin(TEXT("A")));
    auto* Full = Call(UBlueprintPathsLibrary::StaticClass(), TEXT("ConvertRelativePathToFull"), 650, Row);
    Link(Append->GetReturnValuePin(), Full->FindPin(TEXT("InPath")));
    auto* Uri = Call(StaticClass(), TEXT("ToFileUrl"), 1000, Row);
    Link(Full->GetReturnValuePin(), Uri->FindPin(TEXT("AbsolutePath")));
    Sets[Index] = NewObject<UK2Node_VariableSet>(Graph);
    Sets[Index]->VariableReference.SetExternalMember(TEXT("Url"), Index == 0
        ? ACesium3DTileset::StaticClass() : UCesiumTileMapServiceRasterOverlay::StaticClass());
    Init(Sets[Index], 1400 + Index * 300, 900);
    Link(Uri->GetReturnValuePin(), Sets[Index]->FindPin(TEXT("Url")));
    Link(Index == 0 ? Ref->GetValuePin() : GetOverlay->GetReturnValuePin(),
         Sets[Index]->FindPin(UEdGraphSchema_K2::PN_Self));
  }
  auto* Refresh = Call(UCesiumRasterOverlay::StaticClass(), TEXT("Refresh"), 2000, 900);
  Link(GetOverlay->GetReturnValuePin(), Refresh->FindPin(UEdGraphSchema_K2::PN_Self));
  Then->BreakAllPinLinks();
  Link(Then, Sets[0]->FindPin(UEdGraphSchema_K2::PN_Execute));
  Link(Sets[0]->FindPin(UEdGraphSchema_K2::PN_Then), Sets[1]->FindPin(UEdGraphSchema_K2::PN_Execute));
  Link(Sets[1]->FindPin(UEdGraphSchema_K2::PN_Then), Refresh->GetExecPin());
  for (auto* Pin : Continuation) Link(Refresh->GetThenPin(), Pin);
  if (!bWired) {
    const auto Nodes = Graph->Nodes;
    for (UEdGraphNode* Node : Nodes) if (!Before.Contains(Node)) Graph->RemoveNode(Node);
    for (auto* Pin : Continuation) Schema->TryCreateConnection(Then, Pin);
    return TEXT("ERROR: graph connection rejected; original continuation restored");
  }
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
  FKismetEditorUtilities::CompileBlueprint(BP);
  if (BP->Status == BS_Error) return TEXT("ERROR: Level Blueprint compile failed; not saved");
  Terrain->Modify(); Overlay->Modify();
  if (auto* Component = Terrain->FindComponentByClass<UNavalLocalTerrainComponent>()) {
    Terrain->RemoveInstanceComponent(Component); Component->DestroyComponent();
  }
  Terrain->SetTilesetSource(ETilesetSource::FromUrl);
  Terrain->SetUrl(FString()); Overlay->Url.Empty();
  Terrain->MarkPackageDirty(); BP->MarkPackageDirty();
  return FString::Printf(TEXT("OK: Content-relative terrain/imagery nodes compiled; %d original continuation preserved; component removed; actor URLs cleared. Save level and actor."), Continuation.Num());
#else
  return TEXT("ERROR: editor-only operation");
#endif
}
