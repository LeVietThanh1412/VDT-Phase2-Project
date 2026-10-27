#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SeaNavigationMap.h"
#include "ShipEnemyNavigationComponent.generated.h"

class ACesiumGeoreference;
class UCesiumGlobeAnchorComponent;
class UPrimitiveComponent;
class USceneComponent;
class FNumericProperty;
class AShipEnemyProjectile;
class APawn;

UENUM(BlueprintType)
enum class EShipEnemyRole : uint8 { Passive, Coward, Fighter, Hunter };
UENUM(BlueprintType)
enum class EShipNavigationMode : uint8 { Patrol, Destination };
UENUM(BlueprintType)
enum class EShipNavigationState : uint8
{ Idle, Patrol, FollowRoute, Avoid, Replan, Flee, Chase, Attack, Recovery, Arrived, Failed, Dead };

/** Calculates commands only. Existing BP force functions remain the sole physics actuators. */
UCLASS(ClassGroup=(Navigation), meta=(BlueprintSpawnableComponent))
class VDTPHASE2PROJECT_API UShipEnemyNavigationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UShipEnemyNavigationComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation") bool bNewNavigationEnabled = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation") EShipNavigationMode NavigationMode = EShipNavigationMode::Patrol;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation") EShipEnemyRole Role = EShipEnemyRole::Passive;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation", meta=(ToolTip="Content-relative packaged local navigation JSON")) FString NavigationMapFile = TEXT("SeaNavigation/halong_navigation.json");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Patrol", meta=(ToolTip="X longitude degrees, Y latitude degrees, Z WGS84 ellipsoid height meters. Routing is horizontal.")) FVector PatrolPointA = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Patrol") FVector PatrolPointB = FVector::ZeroVector;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Patrol") TObjectPtr<AActor> PatrolActorA;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Patrol") TObjectPtr<AActor> PatrolActorB;
    /** Formation offsets in fixed-chart East/North meters, validated against water. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Patrol") FVector PatrolOffsetA=FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Patrol") FVector PatrolOffsetB=FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Patrol") bool bStartAtPatrolB=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Patrol", meta=(ClampMin="0")) float PatrolWaitSeconds = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Patrol", meta=(ClampMin="0")) float PatrolStartupDelaySeconds = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control", meta=(Units="m",ClampMin="0")) float SafeShipSeparationM = 300;
    UPROPERTY(BlueprintReadOnly, Category="Status") bool bYieldingToShip=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control", meta=(ClampMin="5")) float TrafficPriorityWaitSeconds=15;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control", meta=(ClampMin="10")) float TrafficPriorityHoldSeconds=40;
    UPROPERTY(BlueprintReadOnly, Category="Status") float TrafficWaitSeconds=0;
    UPROPERTY(BlueprintReadOnly, Category="Status") bool bTemporaryTrafficPriority=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control", meta=(Units="m/s",ClampMin="0.1")) float CruiseSpeedMps = 12;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control", meta=(Units="m",ClampMin="10")) float ArrivalRadiusM = 60;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control", meta=(Units="m",ClampMin="20")) float MinimumTurnRadiusM = 250;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control", meta=(ClampMin="0.01")) float BrakingDecelerationMps2 = 0.3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control", meta=(ClampMin="0.01")) float MaxLateralAccelerationMps2 = 0.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control", meta=(ClampMin="-1",ClampMax="1",ToolTip="Verified live BP uses positive yaw/45. Tune only after physical testing.")) float RudderSign = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control") float YawRateDamping = 0.04f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control", meta=(ClampMin="0.01")) float RudderRatePerSecond = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control", meta=(ClampMin="0.01")) float ThrottleRatePerSecond = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control", meta=(ClampMin="0",ClampMax="1")) float MaxForwardThrottle = 0.4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control", meta=(ToolTip="Supplement existing 12 rays with ONE hull-width predictive sweep.")) bool bUseHullSweep = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Control", meta=(ClampMin="0")) float LocalClearanceMarginM = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Planning", meta=(ClampMin="0.5")) float ReplanIntervalSeconds = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Planning", meta=(ClampMin="100",ClampMax="200000")) int32 MaxSearchExpansions = 75000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Planning", meta=(ClampMin="1",ClampMax="100")) float MaxSearchMilliseconds = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recovery", meta=(ClampMin="3")) float StuckWindowSeconds = 15;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recovery", meta=(ClampMin="1",ClampMax="10")) int32 MaxRecoveryAttempts = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recovery", meta=(ClampMin="1")) float MaxReverseDistanceM = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recovery", meta=(ClampMin="0.5")) float MaxReverseSeconds = 6;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Perception", meta=(Units="m",ClampMin="1")) float DetectionRadiusM = 3000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Perception", meta=(Units="m",ClampMin="1")) float LoseTargetRadiusM = 4000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Perception") TSubclassOf<APawn> PlayerShipClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(Units="m",ClampMin="20")) float EngagementDistanceM = 600;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.1")) float FireCooldownSeconds = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0")) float MuzzleSpeedCmps = 20000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") FName MuzzleSocket = TEXT("front_gun_v_socket");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") TSubclassOf<AShipEnemyProjectile> ProjectileClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health", meta=(ClampMin="1")) float MaxHealth = 20000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") bool bDrawDebug = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") bool bDrawLandGrid = false;
    UPROPERTY(BlueprintReadOnly, Category="Status") EShipNavigationState NavigationState = EShipNavigationState::Idle;
    UPROPERTY(BlueprintReadOnly, Category="Status") FVector CurrentFinalDestination = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="Status", meta=(ToolTip="Persistent fixed-chart East/North meters; NOT Unreal positions.")) TArray<FVector> CurrentRoute;
    UPROPERTY(BlueprintReadOnly, Category="Status") int32 CurrentWaypointIndex = 0;
    UPROPERTY(BlueprintReadOnly, Category="Status") float DesiredHeading = 0;
    UPROPERTY(BlueprintReadOnly, Category="Status") float DesiredSpeed = 0;
    UPROPERTY(BlueprintReadOnly, Category="Status") float DesiredThrottle = 0;
    UPROPERTY(BlueprintReadOnly, Category="Status") float DesiredRudder = 0;
    UPROPERTY(BlueprintReadOnly, Category="Status") bool bPathfindingSucceeded = false;
    UPROPERTY(BlueprintReadOnly, Category="Status") bool bAvoidingObstacle = false;
    UPROPERTY(BlueprintReadOnly, Category="Status") bool bStuck = false;
    UPROPERTY(BlueprintReadOnly, Category="Status") bool bCollisionDataUncertain = true;
    UPROPERTY(BlueprintReadOnly, Category="Status") int32 RecoveryAttemptCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="Status") FString LastNavigationFailureReason;
    UPROPERTY(BlueprintReadOnly, Category="Status") float Health = 20000;
    UPROPERTY(BlueprintReadOnly, Category="Status") int32 PatrolLegsCompleted = 0;
    UPROPERTY(BlueprintReadOnly, Category="Status") int32 LastSearchExpansions = 0;
    UPROPERTY(BlueprintReadOnly, Category="Status") float LastSearchMilliseconds = 0;
    UPROPERTY(BlueprintReadOnly, Category="Status") TObjectPtr<AActor> DetectedPlayer;
    UPROPERTY(BlueprintReadOnly, Category="Status") FString WeaponStatus = TEXT("Idle");

    /** Called synchronously from BP Tick before force calls. False selects untouched legacy branch. */
    UFUNCTION(BlueprintCallable, Category="Navigation") bool UpdateNavigation(float DeltaSeconds);
    UFUNCTION(BlueprintCallable, Category="Navigation") void SetDriveCommand(float Throttle, float Rudder);
    UFUNCTION(BlueprintCallable, Category="Navigation") void SetGeographicDestination(FVector LongitudeLatitudeHeight);
    UFUNCTION(BlueprintCallable, Category="Navigation") void StartPatrol();
    UFUNCTION(BlueprintCallable, Category="Navigation") void RequestReplan();
    /** Refresh cached footprint after a prematch archetype replaces the physical hull. */
    void RefreshHullGeometry();
    UFUNCTION(BlueprintCallable, Category="Navigation") void StopNavigation();
    UFUNCTION(BlueprintCallable, Category="Navigation|Tactics") bool BeginRetreatFrom(AActor* Threat,float RetreatDistanceM);
    UFUNCTION(BlueprintCallable, Category="Navigation|Tactics") bool BeginBackwardRetreat(AActor* Threat,float DistanceM);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation|Tactics",meta=(Units="m/s",ClampMin="1",ClampMax="7")) float BackwardRetreatSpeedMps=6;
    UPROPERTY(BlueprintReadOnly, Category="Navigation|Tactics") bool bBackwardRetreatActive=false;
    UPROPERTY(BlueprintReadOnly, Category="Navigation|Tactics") bool bBackwardRetreatBlocked=false;
    UPROPERTY(BlueprintReadOnly, Category="Navigation|Tactics") bool bBackwardRetreatCompleted=false;
    UFUNCTION(BlueprintCallable, Category="Navigation|Tactics") bool BeginPursuitOf(AActor* Threat,float StandOffDistanceM);
    UFUNCTION(BlueprintCallable, Category="Navigation|Tactics") void ReturnToPatrol();
    UFUNCTION(BlueprintPure, Category="Navigation|Tactics") bool HasTacticalDestination() const { return bTacticalOverride; }
    UFUNCTION(BlueprintCallable, Category="Navigation") FVector GeographicToWorld(FVector LLH) const;
    UFUNCTION(BlueprintCallable, Category="Navigation") FVector WorldToGeographic(FVector WorldPosition) const;
    /** Conservative chart fallback for unloaded terrain collision. */
    bool WaterLineClear(const FVector& Start,const FVector& End) const;
    UFUNCTION(BlueprintCallable, Category="Navigation|Editor", meta=(DevelopmentOnly)) static FString InstallInEnemyBlueprint(bool bSave);
    UFUNCTION(BlueprintCallable, Category="Navigation|Editor", meta=(DevelopmentOnly)) static FString InstallNavigationBlueprint(const FString& BlueprintPath, bool bSave);
    UFUNCTION(BlueprintCallable, Category="Navigation|Editor", meta=(DevelopmentOnly)) static FString ValidateInstalledBlueprint();
    UFUNCTION(BlueprintCallable, Category="Navigation|Editor", meta=(DevelopmentOnly)) static FString InspectMovementBlueprints();
    UFUNCTION(BlueprintCallable, Category="Navigation|Editor", meta=(DevelopmentOnly)) static FString CompileBattleBlueprints();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend class FSeaLiveComponentCases;
    bool InitializeNavigation();
    bool PlanRoute(const FVector2D& Position, const FVector2D& Goal);
    void SelectObjective(const FVector2D& Position);
    void UpdatePerception();
    void UpdateWeapon(float Dt);
    bool TryRecovery(const FVector2D& Position,float Speed,float Dt,float& Throttle,float& Rudder);
    float SensorDistance(float RelativeAngleDegrees) const;
    bool ReadSensors();
    bool SweepClear(const FVector& Start,const FVector& End,float RadiusM) const;
    bool RearPathClear(float DistanceM) const;
    bool UpdateBackwardRetreat(float Dt);
    bool CollisionCoverageAvailable() const;
    FVector2D PositionMetric() const;
    FVector2D LLHToMetric(const FVector& LLH) const;
    FVector MetricToWorld(const FVector2D& P) const;
    FVector WorldToECEF(const FVector& P) const;
    void Fail(const FString& Reason);
    UFUNCTION() void OnGeoreferenceChanged();
    UFUNCTION() void OnOwnerDamaged(AActor* Actor,float Damage,const class UDamageType* Type,class AController* Instigator,AActor* Causer);
    UPROPERTY(Transient) TObjectPtr<UPrimitiveComponent> Hull;
    UPROPERTY(Transient) TObjectPtr<USceneComponent> Heading;
    UPROPERTY(Transient) TObjectPtr<class USkeletalMeshComponent> VisualMesh;
    UPROPERTY(Transient) TObjectPtr<ACesiumGeoreference> Georeference;
    UPROPERTY(Transient) TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;
    UPROPERTY(Transient) TObjectPtr<class AAIController> OwnedAimController;
    TSharedPtr<const FSeaNavigationMap> Map;
    FNumericProperty* ThrottleProperty=nullptr;
    FNumericProperty* RudderProperty=nullptr;
    TArray<float> SensorAngles, SensorDistances;
    TArray<FVector2D> FailedCorridors;
    FVector2D GoalMetric=FVector2D::ZeroVector, LastGoal=FVector2D(DBL_MAX,DBL_MAX), ProgressPosition=FVector2D::ZeroVector;
    FVector2D RecoveryStart=FVector2D::ZeroVector;
    float HullRadiusM=0,HullHalfWidthM=0, TimeSinceProgress=0, RecoveryElapsed=0, AvoidElapsed=0, PerceptionClock=0;
    double BestRemainingDistance=DBL_MAX;
    double SuccessfulProgressSinceRecovery=0;
    double LastPlanTime=-1000, LastSeenTime=-1000, LastShotTime=-1000, ArrivalTime=-1;
    int32 AvoidSide=0,RecoveryPhase=0,LastAvoidSide=0,AvoidSideChanges=0;
    bool bInitialized=false,bTargetB=true,bNeedsPlan=true,bHasGoal=false,bDead=false,bOriginChanged=false,bOwnsCommands=false,bExplicitStop=false;
    bool bTacticalOverride=false;
    FVector TacticalDestination=FVector::ZeroVector;
    TWeakObjectPtr<AActor> ReverseThreat;
    FVector2D ReverseStart=FVector2D::ZeroVector;
    float ReverseDistanceM=250;
    double PatrolReadyAt=0;
    FIntVector LastWorldOrigin=FIntVector::ZeroValue;
};
