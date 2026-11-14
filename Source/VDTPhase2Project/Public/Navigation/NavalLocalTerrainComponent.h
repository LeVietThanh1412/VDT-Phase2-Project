#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NavalLocalTerrainComponent.generated.h"

class ACesium3DTileset;
class UCesiumTileMapServiceRasterOverlay;

/** Resolves the existing local Cesium actor before its BeginPlay loads tiles. */
UCLASS(ClassGroup = (Naval), meta = (BlueprintSpawnableComponent))
class VDTPHASE2PROJECT_API UNavalLocalTerrainComponent : public UActorComponent {
  GENERATED_BODY()
public:
  UNavalLocalTerrainComponent();
  UPROPERTY(EditAnywhere, Category = "Local data")
  FString TerrainFile = TEXT("UnrealTerrainSet/QuangNinh/Terrain/layer.json");
  UPROPERTY(EditAnywhere, Category = "Local data")
  FString ImageryFile = TEXT("UnrealTerrainSet/QuangNinh/Imagery/tilemapresource.xml");
  UPROPERTY(EditInstanceOnly, Category = "Local data")
  TObjectPtr<UCesiumTileMapServiceRasterOverlay> Imagery;
  UPROPERTY(BlueprintReadOnly, Transient, Category = "Local data")
  bool bPathsReady = false;
  UPROPERTY(BlueprintReadOnly, Transient, Category = "Local data")
  FString PathError;

  UFUNCTION(BlueprintCallable, Category = "Local data")
  bool ConfigureLocalPaths();
  UFUNCTION(BlueprintPure, Category = "Local data")
  static FString ToFileUrl(const FString& AbsolutePath);
  UFUNCTION(BlueprintCallable, Category = "Local data|Editor", meta = (DevelopmentOnly))
  static UNavalLocalTerrainComponent* InstallOnTileset(
      ACesium3DTileset* Tileset, UCesiumTileMapServiceRasterOverlay* Overlay);

  /** Editor migration: inspect first, then insert Content-relative BeginPlay nodes. */
  UFUNCTION(BlueprintCallable, Category = "Local data|Editor", meta = (DevelopmentOnly))
  static FString InstallLevelBlueprintPaths(UObject* WorldContextObject, bool bApply = false);

protected:
  virtual void OnRegister() override;
#if WITH_EDITOR
  virtual void PreSave(FObjectPreSaveContext SaveContext) override;
#endif
private:
  bool bErrorReported = false;
};
