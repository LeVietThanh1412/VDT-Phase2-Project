#pragma once
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "SeaTerrainCollisionComponent.generated.h"

class UStaticMeshComponent;
class UProceduralMeshComponent;
class ACesiumGeoreference;
struct FSeaNavigationMap;

/** QuangNinh land DEM encodes open water as a solid zero-height surface.
 * Replace ONLY those water triangles in runtime collision, retaining the
 * original streamed land triangles, transforms, rendering and source data.
 */
UCLASS(ClassGroup = (Navigation), meta = (BlueprintSpawnableComponent))
class VDTPHASE2PROJECT_API USeaTerrainCollisionComponent
    : public UActorComponent {
  GENERATED_BODY()
public:
  USeaTerrainCollisionComponent();
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain collision")
  bool bFilterSeaLevelDEM = true;
  /** Dataset-specific ellipsoid height cutoff. Coast/chart exclusions remain
   * solid. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain collision",
            meta = (ClampMin = "0", ClampMax = "1"))
  double SeaLevelToleranceM = .05;
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain collision")
  int32 FilteredTiles = 0;
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain collision")
  int32 RetainedLandTriangles = 0;
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain collision")
  int32 RemovedWaterTriangles = 0;
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain collision")
  int32 RetainedSubmergedTriangles = 0;
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain collision")
  int32 RemovedSeaSkirtTriangles = 0;
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain collision")
  int32 SkirtMetadataTiles = 0;
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain collision")
  FString LastError;
  bool HasCollisionCoverageAt(const FVector &World) const;

protected:
  virtual void BeginPlay() override;
  virtual void EndPlay(const EEndPlayReason::Type Reason) override;
  virtual void
  TickComponent(float DeltaTime, ELevelTick TickType,
                FActorComponentTickFunction *ThisTickFunction) override;

private:
  struct FTile {
    TWeakObjectPtr<UStaticMeshComponent> Source;
    TWeakObjectPtr<UProceduralMeshComponent> Collision;
    TWeakObjectPtr<UProceduralMeshComponent> WaterDetailCollision;
    ECollisionEnabled::Type Original = ECollisionEnabled::NoCollision;
    int32 Land = 0, Water = 0, Bathymetry = 0, SeaSkirts = 0;
    bool bSkirtMetadata = false;
    uint32 GeometryHash = 0;
    FTransform RootRelative = FTransform::Identity;
  };
  TArray<FTile> Tiles;
  TSharedPtr<const FSeaNavigationMap> Chart;
  TWeakObjectPtr<ACesiumGeoreference> Georeference;
  bool BuildTile(UStaticMeshComponent *Source, FTile &Out);
  bool IsChartWater(const FVector2D &A, const FVector2D &B,
                    const FVector2D &C) const;
};
