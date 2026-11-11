#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Navigation/ShipEnemyNavigationComponent.h"
#include "ShipCombatComponent.generated.h"

class USkeletalMeshComponent;
class UPrimitiveComponent;
class UWidgetComponent;
class UShipWeaponComponent;
class UNiagaraComponent;
class ACameraActor;
class UCesiumGlobeAnchorComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FShipHealthChanged,
                                               UShipWeaponComponent *, Ship,
                                               float, Current, float, Maximum);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShipDestroyed,
                                             UShipWeaponComponent *, Ship,
                                             AActor *, Attacker);

UENUM(BlueprintType)
enum class EShipTacticalState : uint8 {
  Patrol,
  Engage,
  Retreat,
  ReturnToPatrol,
  Passive,
  Pursuit,
  Dead,
  BackwardRetreat,
  Escape
};
UENUM(BlueprintType)
enum class EShipAISkill : uint8 { Easy, Normal, Hard, Custom };
UENUM(BlueprintType)
enum class EShipHitRegion : uint8 { Bow, Midship, Stern };
UENUM(BlueprintType)
enum class EShipDeathReason : uint8 { HealthDepleted, Capsized, Other };
UENUM(BlueprintType)
enum class EShipDestructionStage : uint8 {
  Alive,
  Dying,
  Burning,
  Sinking,
  Submerged,
  Cleanup
};

/** Designer-authored ship identity. Add to any ship Blueprint to expose a name
 * on the minimap. */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VDTPHASE2PROJECT_API UShipIdentityComponent : public UActorComponent {
  GENERATED_BODY()
public:
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
  FString ShipName;
  /** Designer-authored team name. Empty falls back to Blue/Red from the ship's
   * faction. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
  FString TeamName;
  /** Returns ShipName, or a readable fallback (player ship / actor label). */
  UFUNCTION(BlueprintPure, Category = "Identity")
  FString GetDisplayName() const;
  /** Returns TeamName, or Blue/Red based on the ship's faction. */
  UFUNCTION(BlueprintPure, Category = "Identity") FString GetTeamName() const;

protected:
  virtual void BeginPlay() override;
};

/** Shared by player input and AI. Executes the ORIGINAL BP projectile/FX/recoil
 * graph. */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VDTPHASE2PROJECT_API UShipWeaponComponent : public UActorComponent {
  GENERATED_BODY()
  friend class UShipCombatLibrary;
  FRotator LastPermittedManualAim = FRotator::ZeroRotator;

public:
  UShipWeaponComponent();
  /** Reads the ship's UShipIdentityComponent, falling back to the actor label.
   */
  UFUNCTION(BlueprintPure, Category = "Identity") FString GetShipName() const;
  /** Reads TeamName from the ship identity, falling back to Blue/Red by team.
   */
  UFUNCTION(BlueprintPure, Category = "Identity") FString GetTeamName() const;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
  float MuzzleSpeedCmps = 20000;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
  float ShotCooldownSeconds = 0;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
  int32 Faction = 0;
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Magazine",
            meta = (ClampMin = "1"))
  int32 MagazineCapacity = 10;
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Magazine",
            meta = (ClampMin = "0.1", Units = "s"))
  float ReloadDurationSeconds = 30;
  UPROPERTY(BlueprintReadOnly, Category = "Weapon|Magazine")
  int32 RoundsRemaining = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Weapon|Magazine")
  bool bReloading = false;
  UPROPERTY(BlueprintReadOnly, Category = "Weapon|Magazine")
  int32 ReloadsStarted = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Weapon|Magazine")
  int32 ReloadsCompleted = 0;
  UFUNCTION(BlueprintPure, Category = "Weapon|Magazine")
  float ReloadRemaining() const;
  /** Standard Actor.Tags mirror Team(), the applied hull and live mission;
   * never independent team authority. */
  UFUNCTION(BlueprintCallable, Category = "Ship|Identity")
  void RefreshShipTags();
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
  FName MuzzleSocket = TEXT("front_gun_v_socket");
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health",
            meta = (ClampMin = "1"))
  float MaxHealth = 100;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon",
            meta = (ClampMin = "0"))
  float BaseProjectileDamage = 10;
  UPROPERTY(BlueprintReadOnly, Category = "Health") float CurrentHealth = 100;
  UPROPERTY(BlueprintReadOnly, Category = "Health") bool bIsDestroyed = false;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
  float DestructionDelaySeconds = 7;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction")
  bool bEnableCapsizeDeath = true;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction",
            meta = (ClampMin = "90", ClampMax = "175", Units = "deg"))
  float CapsizeAngleDegrees = 105;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction",
            meta = (ClampMin = "0.1"))
  float CapsizeConfirmSeconds = .65f;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction",
            meta = (ClampMin = "1"))
  float BurningSeconds = 4;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction",
            meta = (ClampMin = "3"))
  float SinkingSeconds = 12;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction",
            meta = (ClampMin = "20", Units = "m"))
  float SinkDepthM = 65;
  UPROPERTY(BlueprintReadOnly, Category = "Destruction")
  EShipDeathReason DeathReason = EShipDeathReason::Other;
  UPROPERTY(BlueprintReadOnly, Category = "Destruction")
  EShipDestructionStage DestructionStage = EShipDestructionStage::Alive;
  UPROPERTY(BlueprintReadOnly, Category = "Destruction")
  float CurrentCapsizeAngle = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Destruction")
  float SunkDistanceM = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Destruction")
  TObjectPtr<UNiagaraComponent> DestructionFX;
  UFUNCTION(BlueprintCallable, Category = "Destruction")
  bool Die(EShipDeathReason Reason, AActor *Attacker);
  UFUNCTION(BlueprintCallable, Category = "Destruction|Debug")
  FString DescribeDestructionFX() const;
  UFUNCTION(BlueprintCallable, Category = "Destruction|Editor",
            meta = (DevelopmentOnly))
  static FString InspectWreckEffectParameters();
  UFUNCTION(BlueprintCallable, Category = "Destruction|Editor",
            meta = (DevelopmentOnly))
  static FString ConfigureWreckEffect(const FString &OverridesJson);
  UPROPERTY(BlueprintAssignable, Category = "Health")
  FShipHealthChanged OnHealthChanged;
  UPROPERTY(BlueprintAssignable, Category = "Health")
  FShipDestroyed OnShipDestroyed;
  UPROPERTY(BlueprintReadOnly, Category = "Health") int32 DestructionEvents = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Health")
  int32 AcceptedDamageEvents = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Health") int32 RejectedTeamHits = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Health")
  TObjectPtr<UWidgetComponent> HealthWidget;
  /** Static camera left at the wreck while the player's ship is cleaned up. */
  UPROPERTY(Transient) TObjectPtr<ACameraActor> WreckCamera;
  /** Single final damage authority, shared by manual player fire, AI and
   * splash. */
  UFUNCTION(BlueprintCallable, Category = "Health")
  bool ReceiveShipDamage(float Damage, AActor *Causer);
  UFUNCTION(BlueprintCallable, Category = "Health")
  bool ReceiveHullImpact(AActor *Projectile, FVector ImpactWorld);
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Regions",
            meta = (ClampMin = "0", ClampMax = "0.45"))
  float BowLengthFraction = .25f;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Regions",
            meta = (ClampMin = "0", ClampMax = "0.45"))
  float SternLengthFraction = .25f;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Regions",
            meta = (ClampMin = "0", ClampMax = "3"))
  float BowDamageMultiplier = .8f;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Regions",
            meta = (ClampMin = "0", ClampMax = "3"))
  float MidshipDamageMultiplier = 1;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Regions",
            meta = (ClampMin = "0", ClampMax = "3"))
  float SternDamageMultiplier = .8f;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Regions")
  bool bDebugDamage = false;
  UPROPERTY(BlueprintReadOnly, Category = "Health|Regions")
  EShipHitRegion LastImpactRegion = EShipHitRegion::Midship;
  UPROPERTY(BlueprintReadOnly, Category = "Health|Regions")
  FVector LastImpactWorld = FVector::ZeroVector;
  UPROPERTY(BlueprintReadOnly, Category = "Health|Regions")
  FVector LastImpactLocal = FVector::ZeroVector;
  UPROPERTY(BlueprintReadOnly, Category = "Health|Regions")
  float LastNormalizedImpactX = .5f;
  UPROPERTY(BlueprintReadOnly, Category = "Health|Regions")
  float LastAppliedImpactDamage = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Weapon|Status") int32 ShotsFired = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Weapon|Status") int32 Hits = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Weapon|Status")
  TObjectPtr<AActor> LastProjectile;
  UPROPERTY(BlueprintReadOnly, Category = "Weapon|Status")
  TObjectPtr<AActor> LastHitActor;
  UPROPERTY(BlueprintReadOnly, Category = "Weapon|Status")
  FVector LastMuzzle = FVector::ZeroVector;
  UPROPERTY(BlueprintReadOnly, Category = "Weapon|Status")
  FVector LastLaunchVelocity = FVector::ZeroVector;
  UPROPERTY(BlueprintReadOnly, Category = "Weapon|Status")
  FString RejectionReason;
  UFUNCTION(BlueprintCallable, Category = "Weapon") bool FirePlayer();
  UFUNCTION(BlueprintCallable, Category = "Weapon")
  bool TryFireWeapon(FVector TargetLocation, AActor *TargetActor,
                     FVector OptionalLaunchVelocity, bool bHasLaunchVelocity);
  /** Called by the preserved BP spawn nodes, before their original
   * FX/sound/recoil chain. */
  UFUNCTION(BlueprintCallable, Category = "Weapon")
  void FinishShot(AActor *Projectile);
  UFUNCTION(BlueprintPure, Category = "Weapon") float CooldownRemaining() const;
  UFUNCTION(BlueprintPure, Category = "Weapon") int32 Team() const;
  USkeletalMeshComponent *Mesh() const;

protected:
  virtual void BeginPlay() override;
  virtual void EndPlay(const EEndPlayReason::Type Reason) override;
  UFUNCTION() void OnProjectileStop(const FHitResult &Hit);
  UFUNCTION()
  void OnDamaged(AActor *Actor, float Damage, const class UDamageType *Type,
                 class AController *Instigator, AActor *Causer);

private:
  double LastShotTime = -1000;
  bool bDispatching = false;
  bool bAcceptedDispatch = false;
  double ReloadEndsAt = 0;
  FTimerHandle ReloadTimer;
  void BeginReload();
  void CompleteReload();
  FTimerHandle PresentationTimer;
  FTimerHandle DestructionTimer;
  double LastDestructionUpdate = 0, DeathStartedAt = 0, CapsizeElapsed = 0;
  FVector WreckStartLLH = FVector::ZeroVector;
  FQuat WreckESURotation = FQuat::Identity;
  UPROPERTY(Transient) TObjectPtr<UCesiumGlobeAnchorComponent> WreckAnchor;
  void UpdateDestruction();
  void ProtectWreckCamera();
  void UpdatePresentation();
  void RefreshHealthUI();
};

/** Independent gun controller. Never writes ControlRotation, transforms,
 * throttle, or rudder. */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VDTPHASE2PROJECT_API UShipCombatComponent : public UActorComponent {
  GENERATED_BODY()
public:
  UShipCombatComponent();
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
  bool bEnableCombat = true;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
  int32 Faction = 1;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
            meta = (Units = "m"))
  float DetectionRadiusM = 1800;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
            meta = (Units = "m"))
  float EngagementRadiusM = 1200;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
            meta = (Units = "m"))
  float MinEngagementRadiusM = 100;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
            meta = (Units = "m"))
  float LoseTargetRadiusM = 2200;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat",
            meta = (ClampMin = "0.05"))
  float TargetScanInterval = .5f;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
  bool bRequireLineOfSight = true;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
  bool bEnablePredictiveAiming = true;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
  bool bAllowFriendlyFire = false;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
  bool bDefensiveFire = false;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
  float FireCooldownSeconds = 4;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy")
  EShipAISkill ShootingSkill = EShipAISkill::Normal;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
            meta = (ClampMin = "0", ClampMax = "1"))
  float AIAccuracy = .6f;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
            meta = (ClampMin = "0", ClampMax = "10", Units = "deg"))
  float AimErrorDegrees = 2.2f;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
            meta = (ClampMin = "0", ClampMax = "2"))
  float TargetTrackingDelay = .35f;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
            meta = (ClampMin = "0.05", ClampMax = "2"))
  float AimUpdateInterval = .4f;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
            meta = (ClampMin = "0", ClampMax = "2"))
  float TargetAcquisitionSeconds = .6f;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
            meta = (ClampMin = "0", ClampMax = "3"))
  float DistanceAccuracyPenalty = .7f;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
            meta = (ClampMin = "0", ClampMax = "3"))
  float MovementAccuracyPenalty = .5f;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy",
            meta = (ClampMin = "0", ClampMax = "3"))
  float OptionalSuppressionPenalty = 0;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy")
  int32 AccuracySeed = 1337;
  UPROPERTY(BlueprintReadOnly, Category = "Accuracy")
  FVector2D CurrentAimErrorDegrees = FVector2D::ZeroVector;
  UPROPERTY(BlueprintReadOnly, Category = "Accuracy") int32 AimSamples = 0;
  UFUNCTION(BlueprintCallable, Category = "Accuracy")
  void SetShootingSkill(EShipAISkill Skill);
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics")
  bool bEnableHitAndRun = true;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Retreat")
  bool bPreferBackwardRetreat = true;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Retreat",
            meta = (ClampMin = "0"))
  float LowHealthRetreatThreshold = 30;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Retreat",
            meta = (ClampMin = "0"))
  float CriticalHealthThreshold = 15;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Retreat",
            meta = (Units = "m", ClampMin = "80", ClampMax = "500"))
  float BackwardRetreatDistanceM = 300;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics",
            meta = (Units = "m"))
  float MinimumRetreatDistanceM = 250;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics",
            meta = (Units = "m"))
  float SafeReengagementDistanceM = 600;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics")
  float MinimumRetreatSeconds = 12;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics")
  float MaximumRetreatSeconds = 75;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics")
  float RetreatCooldownSeconds = 25;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Roles")
  bool bPromotePassiveOnDamage = true;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Roles")
  bool bPromotePassiveOnThreatAlert = false;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Roles",
            meta = (Units = "m", ClampMin = "0"))
  float ThreatAlertRadiusM = 800;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Hunter",
            meta = (Units = "m", ClampMin = "100"))
  float HunterMaxPursuitDistanceM = 1700;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Hunter",
            meta = (ClampMin = "3"))
  float HunterMaxPursuitSeconds = 30;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Hunter",
            meta = (ClampMin = "1"))
  float HunterLostTargetSeconds = 3;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Hunter",
            meta = (ClampMin = "1"))
  float HunterReplanSeconds = 3;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Hunter",
            meta = (ClampMin = "0", ClampMax = "1"))
  float HunterToFighterHealthRatio = .4f;
  UPROPERTY(BlueprintReadOnly, Category = "Tactics") int32 RoleTransitions = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Tactics")
  int32 HunterRoutesStarted = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Tactics") FString LastRoleTransition;
  UFUNCTION(BlueprintCallable, Category = "Tactics")
  void SetCombatRole(EShipEnemyRole NewRole);
  UFUNCTION(BlueprintCallable, Category = "Tactics")
  bool RaiseThreatAlert(AActor *Threat);
  void OnHostileHit(AActor *Source);
  UPROPERTY(BlueprintReadOnly, Category = "Tactics")
  EShipTacticalState TacticalState = EShipTacticalState::Patrol;
  UPROPERTY(BlueprintReadOnly, Category = "Tactics") int32 RetreatsStarted = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Tactics")
  int32 BackwardRetreatsStarted = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Tactics")
  int32 UnsafeReverseFallbacks = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Tactics") int32 PatrolsResumed = 0;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
  float TurretYawSpeedDegPerSec = 30;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
  float BarrelPitchSpeedDegPerSec = 15;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
  float MinTurretYaw = -135;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
  float MaxTurretYaw = 135;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
  float MinBarrelElevation = -10;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
  float MaxBarrelElevation = 40;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
  float AimYawTolerance = 1;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
  float AimPitchTolerance = 1;
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Debug")
  bool bDrawDebug = false;
  UPROPERTY(BlueprintReadOnly, Category = "Combat|Status")
  TObjectPtr<AActor> Target;
  UPROPERTY(BlueprintReadOnly, Category = "Combat|Status")
  float DistanceToTargetM = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Combat|Status") float TurretYaw = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Combat|Status")
  float DesiredTurretYaw = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Combat|Status")
  float BarrelPitch = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Combat|Status")
  float DesiredBarrelPitch = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Combat|Status")
  float CooldownSecondsRemaining = 0;
  UPROPERTY(BlueprintReadOnly, Category = "Combat|Status")
  bool bHasFiringSolution = false;
  UPROPERTY(BlueprintReadOnly, Category = "Combat|Status")
  bool bTargetVisible = false;
  UPROPERTY(BlueprintReadOnly, Category = "Combat|Status")
  FVector PredictedIntercept = FVector::ZeroVector;
  UPROPERTY(BlueprintReadOnly, Category = "Combat|Status")
  FVector LaunchVelocity = FVector::ZeroVector;
  UPROPERTY(BlueprintReadOnly, Category = "Combat|Status")
  FString FiringStatus = TEXT("Initializing");
  UFUNCTION(BlueprintPure, Category = "Combat")
  bool IsHostile(AActor *Candidate) const;
  /** Mission memory may record only a hostile visible to the existing sensor.
   */
  bool CanObserveHostile(AActor *Candidate) const {
    return IsHostile(Candidate) && Visible(Candidate);
  }
  UFUNCTION(BlueprintCallable, Category = "Combat") void ScanTargets();
  /** Runtime target assignment uses the shared hostility filter; does not
   * rebuild Blueprint components. */
  UFUNCTION(BlueprintCallable, Category = "Combat")
  bool SetHostileTarget(AActor *Candidate);
  static UPrimitiveComponent *PhysicalHull(AActor *Ship);

protected:
  virtual void BeginPlay() override;
  virtual void TickComponent(float Dt, ELevelTick Type,
                             FActorComponentTickFunction *Tick) override;

private:
  bool Visible(AActor *Candidate) const;
  bool Solve(FVector Start, FVector Aim, FVector Velocity, FVector &Launch,
             FVector &Intercept) const;
  bool UpdateAimSolution(FVector Start);
  bool ClearArc(FVector Start, FVector Launch, float FlightTime,
                FString &Reason) const;
  void UpdateTactics();
  void ChangeRole(EShipEnemyRole NewRole, const TCHAR *Reason);
  FVector AimPoint(AActor *Ship) const;
  UPROPERTY(Transient) TObjectPtr<UShipWeaponComponent> Weapon;
  UPROPERTY(Transient) TObjectPtr<UShipEnemyNavigationComponent> Navigation;
  UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Visual;
  float ScanClock = 1000, FireCheckClock = 0;
  double LastFire = -1000, LastLog = -1000;
  double StateSince = 0, LastRetreatEnded = -1000;
  // Navigation remembers the threat while the gun correctly drops an
  // occluded target. A brief LOS change must not reverse an escape turn.
  TWeakObjectPtr<AActor> RetreatThreat;
  float RetreatStartSeparationM = 0;
  double HunterSince = -1, HunterLostSince = -1, LastHunterPlan = -1000;
  bool bHunterPursuitActive = false;
  bool bVerifiedAxes = false;
  struct FAimObservation {
    double Time;
    FVector Position, Velocity;
  };
  TArray<FAimObservation> AimHistory;
  FRandomStream AimRandom;
  TWeakObjectPtr<AActor> ObservedTarget;
  double TargetObservedAt = 0, LastAimUpdate = -1000;
  bool bCachedAimSolution = false;
};

UCLASS()
class VDTPHASE2PROJECT_API UShipCombatLibrary
    : public UBlueprintFunctionLibrary {
  GENERATED_BODY()
public:
  /** Player input is returned unchanged; AI angles feed the SAME two existing
   * bone nodes. */
  UFUNCTION(BlueprintPure, Category = "Combat")
  static FRotator SelectGunAim(APawn *Ship, FRotator ManualAim);
  UFUNCTION(BlueprintCallable, Category = "Combat|Editor",
            meta = (DevelopmentOnly))
  static FString InstallCombatIntegration();
  UFUNCTION(BlueprintCallable, Category = "Combat|Editor",
            meta = (DevelopmentOnly))
  static FString VerifyMagazineFailurePath(APawn *TemplateShip);
  UFUNCTION(BlueprintCallable, Category = "Combat|Editor",
            meta = (DevelopmentOnly, WorldContext = "WorldContextObject"))
  static FString
      CaptureGameplayHUD(UObject *WorldContextObject, const FString &FilePath);
  UFUNCTION(BlueprintCallable, Category = "Combat|Editor",
            meta = (DevelopmentOnly))
  static FString InstallBattlePresentation();
  UFUNCTION(BlueprintCallable, Category = "Combat|Editor",
            meta = (DevelopmentOnly))
  static FString DisableSensorDebugDefaults();
  /** Editor diagnostics only; includes the same physical waves sampled by
   * buoyancy. */
  UFUNCTION(BlueprintCallable, Category = "Combat|Editor",
            meta = (DevelopmentOnly, WorldContext = "WorldContextObject"))
  static bool SampleOceanWaves(UObject *WorldContextObject, FVector Location,
                               FVector &Surface, FVector &Normal);
  /** Audits first; optionally bypasses only cosmetic impact spheres, preserving
   * execution flow. Caller saves. */
  UFUNCTION(BlueprintCallable, Category = "Combat|Editor",
            meta = (DevelopmentOnly))
  static FString InspectImpactDebug(bool bDisable = false);
  /** Replaces only the old unfiltered radial-damage node; keeps impact FX. */
  UFUNCTION(BlueprintCallable, Category = "Combat")
  static void ApplyProjectileImpact(AActor *Projectile, FHitResult Impact);
  /** Resolves a ship actor's designer-authored name for minimap markers. */
  UFUNCTION(BlueprintPure, Category = "Ship|Identity")
  static FString GetShipNameForObject(UObject *Object);
};
