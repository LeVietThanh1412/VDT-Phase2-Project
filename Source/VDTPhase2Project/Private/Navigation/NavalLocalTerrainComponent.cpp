#include "Navigation/NavalLocalTerrainComponent.h"
#include "Utilities/NavalCesiumCompatibility.h"
#include "Cesium3DTileset.h"
#include "CesiumTileMapServiceRasterOverlay.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#if WITH_EDITOR
#include "UObject/ObjectSaveContext.h"
#endif

UNavalLocalTerrainComponent::UNavalLocalTerrainComponent() {
  PrimaryComponentTick.bCanEverTick = false;
}

FString UNavalLocalTerrainComponent::ToFileUrl(const FString& AbsolutePath) {
  FString Path = AbsolutePath;
  FPaths::NormalizeFilename(Path);
  if (FPaths::IsRelative(Path) ||
      (Path.Len() > 1 && Path[1] == ':' && (Path.Len() < 3 || Path[2] != '/')))
    return FString();
  FString Encoded = FGenericPlatformHttp::UrlEncode(Path);
  Encoded.ReplaceInline(TEXT("%2F"), TEXT("/"));
  Encoded.ReplaceInline(TEXT("%3A"), TEXT(":"));
  return Path.StartsWith(TEXT("//")) ? TEXT("file:") + Encoded
       : Path.StartsWith(TEXT("/")) ? TEXT("file://") + Encoded
                                    : TEXT("file:///") + Encoded;
}

void UNavalLocalTerrainComponent::OnRegister() {
  Super::OnRegister();
  if (!IsTemplate() && GetOwner() && !IsRunningCommandlet())
    ConfigureLocalPaths();
}

bool UNavalLocalTerrainComponent::ConfigureLocalPaths() {
  bPathsReady = false;
  auto* Tileset = Cast<ACesium3DTileset>(GetOwner());
  auto Fail = [&](const FString& Message) {
    PathError = Message;
    if (Tileset) {
      Tileset->SetTilesetSource(ETilesetSource::FromUrl);
      Tileset->SetUrl(FString());
    }
    if (Imagery) { Imagery->Deactivate(); Imagery->Url.Empty(); }
    UE_LOG(LogTemp, Error, TEXT("Naval local terrain: %s"), *Message);
    if (!bErrorReported && !IsRunningCommandlet() && !IsRunningDedicatedServer() &&
        !FApp::IsUnattended() && GetWorld() && GetWorld()->IsGameWorld()) {
      bErrorReported = true;
      FMessageDialog::Open(EAppMsgType::Ok,
          FText::FromString(Message + TEXT("\nRestore the complete game package, including Data/UnrealTerrainSet/QuangNinh.")),
          FText::FromString(TEXT("Local terrain data unavailable")));
    }
    return false;
  };
  if (!Tileset || !IsValid(Imagery) || Imagery->GetOwner() != Tileset)
    return Fail(TEXT("Component requires its owning Cesium tileset and its TMS imagery overlay."));
  const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
  auto Resolve = [&](const FString& Relative, FString& Out) {
    if (!FPaths::IsRelative(Relative)) return false;
    Out = FPaths::ConvertRelativePathToFull(ProjectRoot, Relative);
    FPaths::NormalizeFilename(Out);
    return FPaths::IsUnderDirectory(Out, ProjectRoot) && FPaths::FileExists(Out);
  };
  FString TerrainPath, ImageryPath;
  if (!Resolve(TerrainFile, TerrainPath) || !Resolve(ImageryFile, ImageryPath))
    return Fail(FString::Printf(TEXT("Required local files missing under %s. Stage the complete Data/UnrealTerrainSet/QuangNinh dataset."), *ProjectRoot));
  const FString TerrainUrl = ToFileUrl(TerrainPath);
  const FString ImageryUrl = ToFileUrl(ImageryPath);
  if (TerrainUrl.IsEmpty() || ImageryUrl.IsEmpty())
    return Fail(TEXT("Unable to construct absolute local file URIs."));
  Tileset->SetTilesetSource(ETilesetSource::FromUrl);
  Tileset->SetUrl(TerrainUrl);
  if (Imagery->Url != ImageryUrl) {
    Imagery->Deactivate();
    Imagery->Url = ImageryUrl;
  }
  if (!Imagery->IsActive()) Imagery->Activate(true);
  bPathsReady = true;
  PathError.Empty();
  UE_LOG(LogTemp, Display, TEXT("Naval local terrain resolved: %s | imagery: %s"), *TerrainUrl, *ImageryUrl);
  return true;
}

#if WITH_EDITOR
void UNavalLocalTerrainComponent::PreSave(FObjectPreSaveContext SaveContext) {
  Super::PreSave(SaveContext);
  // Cooked actors carry no editor-preview drive paths. OnRegister resolves them
  // from the installed project directory before Cesium's BeginPlay loads tiles.
  if (SaveContext.IsCooking() && !IsTemplate()) {
    if (auto* Tileset = Cast<ACesium3DTileset>(GetOwner()))
      Tileset->SetUrl(FString());
    if (Imagery) Imagery->Url.Empty();
  }
}
#endif

UNavalLocalTerrainComponent* UNavalLocalTerrainComponent::InstallOnTileset(
    ACesium3DTileset* Tileset, UCesiumTileMapServiceRasterOverlay* Overlay) {
#if WITH_EDITOR
  if (!IsValid(Tileset) || !IsValid(Overlay) || Overlay->GetOwner() != Tileset)
    return nullptr;
  Tileset->Modify();
  auto* Component = Tileset->FindComponentByClass<UNavalLocalTerrainComponent>();
  if (!Component) {
    Component = NewObject<UNavalLocalTerrainComponent>(Tileset,
        TEXT("PortableLocalTerrain"), RF_Transactional);
    Component->Imagery = Overlay;
    Tileset->AddInstanceComponent(Component);
    Component->OnComponentCreated();
    Component->RegisterComponent();
  } else {
    Component->Modify();
    Component->Imagery = Overlay;
    Component->ConfigureLocalPaths();
  }
  Tileset->MarkPackageDirty();
  return Component;
#else
  return nullptr;
#endif
}
