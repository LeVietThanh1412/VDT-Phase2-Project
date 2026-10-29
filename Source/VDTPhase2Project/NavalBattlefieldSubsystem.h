#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ShipPhysicsProfile.h"
#include "NavalBattlefieldSubsystem.generated.h"
class APawn;
class ACameraActor;
class ACesiumCameraManager;
struct FSeaRouteResult;
struct FSeaNavigationMap;

USTRUCT(BlueprintType)
struct FNavalSpawnAssignment
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TObjectPtr<APawn> Ship;
    UPROPERTY(BlueprintReadOnly) int32 Team=0;
    UPROPERTY(BlueprintReadOnly) int32 Slot=0;
    UPROPERTY(BlueprintReadOnly) FName SpawnSector;
    UPROPERTY(BlueprintReadOnly) FVector2D SpawnEN=FVector2D::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector2D TargetEN=FVector2D::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float HeadingDegrees=0;
    UPROPERTY(BlueprintReadOnly) float HullRadiusM=0;
    UPROPERTY(BlueprintReadOnly) bool bPhysicalClearanceVerified=false;
    UPROPERTY(BlueprintReadOnly) FString DepthStatus;
};

/** One prematch placement authority. Never applies a gameplay movement force. */
UCLASS()
class VDTPHASE2PROJECT_API UNavalBattlefieldSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual void Deinitialize() override;
    void InitializeFleet(const TArray<APawn*>& Ships);
    UFUNCTION(BlueprintPure,Category="Battlefield") bool IsEnabled() const;
    UFUNCTION(BlueprintPure,Category="Battlefield") bool CanBeginCombat() const {return !IsEnabled() || bReady;}
    UFUNCTION(BlueprintPure,Category="Battlefield") FString LayoutReport() const;
    UPROPERTY(BlueprintReadOnly,Category="Battlefield") int32 ActualSeed=0;
    UPROPERTY(BlueprintReadOnly,Category="Battlefield") bool bReady=false;
    UPROPERTY(BlueprintReadOnly,Category="Battlefield") FString Status=TEXT("Not initialized");
    UPROPERTY(BlueprintReadOnly,Category="Battlefield") FString InitializationError;
    UPROPERTY(BlueprintReadOnly,Category="Battlefield") TArray<FNavalSpawnAssignment> Assignments;
    UPROPERTY(BlueprintReadOnly,Category="Battlefield") int32 LayoutAttempts=0;
    UPROPERTY(BlueprintReadOnly,Category="Battlefield") int32 PhysicalLayoutRetries=0;
    UPROPERTY(BlueprintReadOnly,Category="Battlefield") bool bUsedSafeFallback=false;
    TSharedPtr<const FSeaNavigationMap> NavigationChartForShip(APawn* Ship) const;
private:
    bool GenerateLayout(const TArray<APawn*>& Ships);
    bool ValidatePhysicalLayout(bool& bWaitingForTerrain);
    bool CheckPhysicalAssignment(FNavalSpawnAssignment& Assignment,const FSeaRouteResult& Route,bool& bWaitingForTerrain);
    bool BuildOperationalCharts();
    bool PlaceFleet();
    void Fail(const FString& Reason);
    FRandomStream LayoutRandom;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> LoadingCamera;
    TWeakObjectPtr<ACesiumCameraManager> StreamingCameraManager;
    UPROPERTY(Transient) TArray<TObjectPtr<APawn>> RetainedFleet;
    double InitializedAt=0,PlacedAt=-1;
    bool bInitialized=false;
    bool bPhysicalSearch=false;
    FString LastPhysicalRejection;
    FString LastPhysicalCollider;
    double TerrainStableSince=-1;
    TMap<EShipHullClass,TSharedPtr<const FSeaNavigationMap>> OperationalCharts;
};
