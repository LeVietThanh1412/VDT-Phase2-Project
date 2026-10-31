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
    UPROPERTY(BlueprintReadOnly) FVector2D DefensiveAnchorEN=FVector2D::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FString MissionState=TEXT("Initial patrol");
    UPROPERTY(BlueprintReadOnly) int32 InterceptionsStarted=0;
    UPROPERTY(BlueprintReadOnly) int32 DefensiveReturns=0;
    UPROPERTY(BlueprintReadOnly) int32 AssaultPhase=0;
    UPROPERTY(BlueprintReadOnly) int32 AssaultGoalsReached=0;
    UPROPERTY(BlueprintReadOnly) FVector2D MissionGoalEN=FVector2D::ZeroVector;
    UPROPERTY(BlueprintReadOnly) bool bAssaultReleased=false;
    UPROPERTY(BlueprintReadOnly) double AssaultReleasedAt=-1;
    UPROPERTY(BlueprintReadOnly) float FormationDistanceM=0;
    UPROPERTY(BlueprintReadOnly) int32 MissionRouteRetries=0;
    bool bDefensiveIntercept=false;
    double LastMissionPlanAt=-1000;
    double MissionRetryAt=0;
    float BaseCruiseSpeedMps=0,FormationWaitSeconds=0;
    bool bHoldingForLeader=false;
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
    /** Read-only QA access to the exact surveyed chart and native planner. */
    UFUNCTION(BlueprintCallable,Category="Battlefield|Debug",meta=(DevelopmentOnly)) FString InspectNavigationChart(APawn* Ship) const;
    UFUNCTION(BlueprintCallable,Category="Battlefield|Debug",meta=(DevelopmentOnly)) FString InspectNavigationRoute(APawn* Ship,FVector2D Start,FVector2D Goal) const;
    UFUNCTION(BlueprintCallable,Category="Battlefield|Debug",meta=(DevelopmentOnly)) bool ValidateDevelopmentPlacement(APawn* Ship,FVector2D Position,float HeadingDegrees);
    UFUNCTION(BlueprintPure,Category="Battlefield") bool AreTacticsEnabled() const;
    bool IsDefensiveTargetAllowed(AActor* Ship,AActor* Target) const;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Battlefield|Defense",meta=(ClampMin="500",ClampMax="1000",Units="m")) float DefensiveLeashM=800;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Battlefield|Assault",meta=(ClampMin="180",ClampMax="350",Units="m")) float FormationSpacingM=240;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Battlefield|Assault",meta=(ClampMin="1",ClampMax="15",Units="s")) float FormationDepartureIntervalSeconds=3;
    UPROPERTY(BlueprintReadOnly,Category="Battlefield|Assault") TWeakObjectPtr<APawn> AssaultLeader;
    UPROPERTY(BlueprintReadOnly,Category="Battlefield|Assault") int32 AssaultLeaderChanges=0;
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
    void UpdateDefenders();
    void UpdateAssault();
    bool SetAssaultGoal(FNavalSpawnAssignment& Assignment,FVector2D Goal);
    bool AdvanceAssault(FNavalSpawnAssignment& Assignment);
    bool FindMissionGoal(const FNavalSpawnAssignment& Assignment,FVector2D Desired,bool bDefender,FVector2D& OutGoal,FVector2D Threat=FVector2D::ZeroVector,float MinimumThreatDistanceM=0) const;
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
    double LastMissionUpdateAt=-1000;
    double LastAssaultUpdateAt=-1000;
    bool bAssaultInitialized=false;
    FVector2D ScoutContactEN=FVector2D::ZeroVector;
    double ScoutContactAt=-1000;
    TMap<EShipHullClass,TSharedPtr<const FSeaNavigationMap>> OperationalCharts;
};
