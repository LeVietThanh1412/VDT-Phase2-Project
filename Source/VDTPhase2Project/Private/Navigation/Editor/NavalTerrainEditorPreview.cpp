#include "Navigation/Editor/NavalTerrainEditorPreview.h"

#if WITH_EDITOR
#include "Navigation/NavalLocalTerrainComponent.h"
#include "Cesium3DTileset.h"
#include "CesiumTileMapServiceRasterOverlay.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace NavalTerrainEditorPreview {
namespace {
TWeakObjectPtr<ACesium3DTileset> Preview;
FDelegateHandle MapOpened, BeforePIE, AfterPIE;

void RemovePreview() {
  if (Preview.IsValid()) {
    UPackage* Package = Preview->GetWorld()->GetPackage();
    const bool bWasDirty = Package->IsDirty();
    Preview->Destroy();
    if (!bWasDirty) Package->SetDirtyFlag(false);
  }
  Preview.Reset();
}

void ShowPreview() {
  RemovePreview();
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  if (!World || World->WorldType != EWorldType::Editor) return;
  ACesium3DTileset* Source = nullptr;
  for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    if (It->GetActorLabel() == TEXT("QuangNinh_Terrain")) { Source = *It; break; }
  if (!Source || !Source->GetUrl().IsEmpty()) return;
  const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
  const FString Terrain = Root / TEXT("UnrealTerrainSet/QuangNinh/Terrain/layer.json");
  const FString Imagery = Root / TEXT("UnrealTerrainSet/QuangNinh/Imagery/tilemapresource.xml");
  if (!FPaths::FileExists(Terrain) || !FPaths::FileExists(Imagery)) return;

  // Never saved. Remove before PIE duplicates the world so only the Level
  // Blueprint initializes runtime terrain; restore when returning to editing.
  const bool bWasDirty = World->GetPackage()->IsDirty();
  FActorSpawnParameters Params;
  Params.Template = Source;
  Params.ObjectFlags = RF_Transient | RF_DuplicateTransient;
  Params.bTemporaryEditorActor = true;
  Params.bHideFromSceneOutliner = true;
  auto* Actor = World->SpawnActor<ACesium3DTileset>(
      ACesium3DTileset::StaticClass(), Source->GetActorTransform(), Params);
  if (!Actor) return;
  Preview = Actor;
  Actor->bIsEditorOnlyActor = true;
  Actor->SetActorLabel(TEXT("QuangNinh_Terrain_EditorPreview"), false);
  Actor->SetTilesetSource(ETilesetSource::FromUrl);
  if (auto* Overlay = Actor->FindComponentByClass<UCesiumTileMapServiceRasterOverlay>())
    Overlay->Url = UNavalLocalTerrainComponent::ToFileUrl(Imagery);
  Actor->SetUrl(UNavalLocalTerrainComponent::ToFileUrl(Terrain));
  if (auto* Overlay = Actor->FindComponentByClass<UCesiumTileMapServiceRasterOverlay>())
    Overlay->Refresh();
  if (!bWasDirty) World->GetPackage()->SetDirtyFlag(false);
}
} // namespace

void Startup() {
  if (IsRunningCommandlet()) return;
  MapOpened = FEditorDelegates::OnMapOpened.AddLambda(
      [](const FString&, bool) { ShowPreview(); });
  BeforePIE = FEditorDelegates::PreBeginPIE.AddLambda(
      [](bool) { RemovePreview(); });
  AfterPIE = FEditorDelegates::EndPIE.AddLambda(
      [](bool) { ShowPreview(); });
}

void Shutdown() {
  FEditorDelegates::OnMapOpened.Remove(MapOpened);
  FEditorDelegates::PreBeginPIE.Remove(BeforePIE);
  FEditorDelegates::EndPIE.Remove(AfterPIE);
  RemovePreview();
}
} // namespace NavalTerrainEditorPreview
#endif
