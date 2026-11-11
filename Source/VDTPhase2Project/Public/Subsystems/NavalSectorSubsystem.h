#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NavalSectorSubsystem.generated.h"

struct FSeaNavigationMap;

/** Logical spatial data, never a permanent actor per cell. */
USTRUCT(BlueprintType)
struct FNavalSector {
  GENERATED_BODY()
  UPROPERTY(BlueprintReadOnly) FName Id;
  UPROPERTY(BlueprintReadOnly) int32 Row = 0;
  UPROPERTY(BlueprintReadOnly) int32 Column = 0;
  UPROPERTY(BlueprintReadOnly) FVector2D CenterEN = FVector2D::ZeroVector;
  UPROPERTY(BlueprintReadOnly) FVector2D AnchorEN = FVector2D::ZeroVector;
  UPROPERTY(BlueprintReadOnly)
  FVector LongitudeLatitudeHeight = FVector::ZeroVector;
  UPROPERTY(BlueprintReadOnly) FName TeamRegion;
  UPROPERTY(BlueprintReadOnly) float WaterAreaFraction = 0;
  UPROPERTY(BlueprintReadOnly) float ChartSampleFraction = 0;
  UPROPERTY(BlueprintReadOnly) float ShoreClearanceM = 0;
  UPROPERTY(BlueprintReadOnly) int32 ConnectedRegion = 0;
  UPROPERTY(BlueprintReadOnly) bool bNavigable = false;
  UPROPERTY(BlueprintReadOnly) bool bSuitableSmall = false;
  UPROPERTY(BlueprintReadOnly) bool bSuitableMedium = false;
  UPROPERTY(BlueprintReadOnly) bool bSuitableLarge = false;
  UPROPERTY(BlueprintReadOnly) bool bSpawnEligible = false;
  UPROPERTY(BlueprintReadOnly) bool bPatrolEligible = false;
  UPROPERTY(BlueprintReadOnly) TArray<int32> NearbyIslands;
  UPROPERTY(BlueprintReadOnly) TArray<FName> Zones;
  UPROPERTY(BlueprintReadOnly) TArray<FVector2D> SamplesEN;
};

UCLASS()
class VDTPHASE2PROJECT_API UNavalSectorSubsystem : public UWorldSubsystem {
  GENERATED_BODY()
public:
  virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
  virtual void OnWorldBeginPlay(UWorld &World) override;
  UFUNCTION(BlueprintCallable, Category = "Battlefield|Sectors")
  bool LoadSectorGrid();
  UFUNCTION(BlueprintPure, Category = "Battlefield|Sectors")
  bool IsInsideField(FVector2D ChartEN, float InsetM = 0) const;
  UFUNCTION(BlueprintPure, Category = "Battlefield|Sectors")
  int32 SectorAtChartPosition(FVector2D ChartEN) const;
  UFUNCTION(BlueprintPure, Category = "Battlefield|Sectors")
  FVector2D WorldToChart(FVector WorldPosition) const;
  /** Tangent-plane mapping only; spawn authority must query water separately.
   */
  UFUNCTION(BlueprintPure, Category = "Battlefield|Sectors")
  FVector ChartToWorld(FVector2D ChartEN) const;
  UFUNCTION(BlueprintCallable, Category = "Battlefield|Sectors")
  TArray<FVector> FindSectorRoute(int32 StartSector, int32 GoalSector);
  UFUNCTION(BlueprintCallable, Category = "Battlefield|Sectors")
  FString ValidateRequiredConnections();
  UPROPERTY(BlueprintReadOnly, Category = "Battlefield|Sectors")
  TArray<FNavalSector> Sectors;
  UPROPERTY(BlueprintReadOnly, Category = "Battlefield|Sectors")
  FVector2D CenterEN = FVector2D(14500, 3500);
  UPROPERTY(BlueprintReadOnly, Category = "Battlefield|Sectors")
  float WidthM = 2500;
  UPROPERTY(BlueprintReadOnly, Category = "Battlefield|Sectors")
  float HeightM = 2500;
  UPROPERTY(BlueprintReadOnly, Category = "Battlefield|Sectors")
  float SectorSizeM = 100;
  UPROPERTY(BlueprintReadOnly, Category = "Battlefield|Sectors")
  float YawDegrees = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Battlefield|Sectors")
  int32 Columns = 25;
  UPROPERTY(BlueprintReadOnly, Category = "Battlefield|Sectors")
  int32 Rows = 25;
  UPROPERTY(BlueprintReadOnly, Category = "Battlefield|Sectors")
  int32 MainConnectedRegion = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Battlefield|Sectors")
  bool bGridReady = false;
  UPROPERTY(BlueprintReadOnly, Category = "Battlefield|Sectors")
  FString GridError;
  UPROPERTY(BlueprintReadOnly, Category = "Battlefield|Sectors")
  FString LastRouteError;
  const TSharedPtr<const FSeaNavigationMap> &NavigationChart() const {
    return Chart;
  }
  const TMap<FName, int32> &ZoneRepresentatives() const {
    return Representatives;
  }

private:
  TSharedPtr<const FSeaNavigationMap> Chart;
  TMap<FName, int32> Representatives;
};
