#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ShipCombatComponent.generated.h"

class USkeletalMeshComponent;
class UPrimitiveComponent;
class UProjectileMovementComponent;
class UShipEnemyNavigationComponent;

UENUM(BlueprintType)
enum class EShipTacticalState : uint8 { Patrol, Engage, Retreat, ReturnToPatrol };

/** Shared by player input and AI. Executes the ORIGINAL BP projectile/FX/recoil graph. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class VDTPHASE2PROJECT_API UShipWeaponComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UShipWeaponComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon") float MuzzleSpeedCmps=20000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon") float ShotCooldownSeconds=0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon") int32 Faction=0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon") FName MuzzleSocket=TEXT("front_gun_v_socket");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health",meta=(ClampMin="1")) float MaxHealth=500000;
    UPROPERTY(BlueprintReadOnly, Category="Health") float CurrentHealth=500000;
    UPROPERTY(BlueprintReadOnly, Category="Health") bool bIsDestroyed=false;
    UPROPERTY(BlueprintReadOnly, Category="Weapon|Status") int32 ShotsFired=0;
    UPROPERTY(BlueprintReadOnly, Category="Weapon|Status") int32 Hits=0;
    UPROPERTY(BlueprintReadOnly, Category="Weapon|Status") TObjectPtr<AActor> LastProjectile;
    UPROPERTY(BlueprintReadOnly, Category="Weapon|Status") TObjectPtr<AActor> LastHitActor;
    UPROPERTY(BlueprintReadOnly, Category="Weapon|Status") FVector LastMuzzle=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="Weapon|Status") FVector LastLaunchVelocity=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="Weapon|Status") FString RejectionReason;
    UFUNCTION(BlueprintCallable, Category="Weapon") bool FirePlayer();
    UFUNCTION(BlueprintCallable, Category="Weapon") bool TryFireWeapon(FVector TargetLocation,AActor* TargetActor,FVector OptionalLaunchVelocity,bool bHasLaunchVelocity);
    /** Called by the preserved BP spawn nodes, before their original FX/sound/recoil chain. */
    UFUNCTION(BlueprintCallable, Category="Weapon") void FinishShot(AActor* Projectile);
    UFUNCTION(BlueprintPure, Category="Weapon") float CooldownRemaining() const;
    UFUNCTION(BlueprintPure, Category="Weapon") int32 Team() const;
    USkeletalMeshComponent* Mesh() const;
protected:
    virtual void BeginPlay() override;
    UFUNCTION() void OnProjectileStop(const FHitResult& Hit);
    UFUNCTION() void OnDamaged(AActor* Actor,float Damage,const class UDamageType* Type,class AController* Instigator,AActor* Causer);
private:
    double LastShotTime=-1000;
    bool bDispatching=false;
};

/** Independent gun controller. Never writes ControlRotation, transforms, throttle, or rudder. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class VDTPHASE2PROJECT_API UShipCombatComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UShipCombatComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") bool bEnableCombat=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") int32 Faction=1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat",meta=(Units="m")) float DetectionRadiusM=1800;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat",meta=(Units="m")) float EngagementRadiusM=1200;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat",meta=(Units="m")) float MinEngagementRadiusM=100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat",meta=(Units="m")) float LoseTargetRadiusM=2200;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat",meta=(ClampMin="0.05")) float TargetScanInterval=.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") bool bRequireLineOfSight=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") bool bEnablePredictiveAiming=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") bool bAllowFriendlyFire=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") bool bDefensiveFire=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float FireCooldownSeconds=4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics") bool bEnableHitAndRun=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics",meta=(Units="m")) float MinimumRetreatDistanceM=250;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics",meta=(Units="m")) float SafeReengagementDistanceM=600;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics") float MinimumRetreatSeconds=12;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics") float MaximumRetreatSeconds=75;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics") float RetreatCooldownSeconds=25;
    UPROPERTY(BlueprintReadOnly, Category="Tactics") EShipTacticalState TacticalState=EShipTacticalState::Patrol;
    UPROPERTY(BlueprintReadOnly, Category="Tactics") int32 RetreatsStarted=0;
    UPROPERTY(BlueprintReadOnly, Category="Tactics") int32 PatrolsResumed=0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Turret") float TurretYawSpeedDegPerSec=30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Turret") float BarrelPitchSpeedDegPerSec=15;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Turret") float MinTurretYaw=-135;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Turret") float MaxTurretYaw=135;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Turret") float MinBarrelElevation=-10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Turret") float MaxBarrelElevation=40;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Turret") float AimYawTolerance=1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Turret") float AimPitchTolerance=1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Debug") bool bDrawDebug=false;
    UPROPERTY(BlueprintReadOnly, Category="Combat|Status") TObjectPtr<AActor> Target;
    UPROPERTY(BlueprintReadOnly, Category="Combat|Status") float DistanceToTargetM=0;
    UPROPERTY(BlueprintReadOnly, Category="Combat|Status") float TurretYaw=0;
    UPROPERTY(BlueprintReadOnly, Category="Combat|Status") float DesiredTurretYaw=0;
    UPROPERTY(BlueprintReadOnly, Category="Combat|Status") float BarrelPitch=0;
    UPROPERTY(BlueprintReadOnly, Category="Combat|Status") float DesiredBarrelPitch=0;
    UPROPERTY(BlueprintReadOnly, Category="Combat|Status") float CooldownSecondsRemaining=0;
    UPROPERTY(BlueprintReadOnly, Category="Combat|Status") bool bHasFiringSolution=false;
    UPROPERTY(BlueprintReadOnly, Category="Combat|Status") bool bTargetVisible=false;
    UPROPERTY(BlueprintReadOnly, Category="Combat|Status") FVector PredictedIntercept=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="Combat|Status") FVector LaunchVelocity=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="Combat|Status") FString FiringStatus=TEXT("Initializing");
    UFUNCTION(BlueprintPure, Category="Combat") bool IsHostile(AActor* Candidate) const;
    UFUNCTION(BlueprintCallable, Category="Combat") void ScanTargets();
    static UPrimitiveComponent* PhysicalHull(AActor* Ship);
protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick) override;
private:
    bool Visible(AActor* Candidate) const;
    bool Solve(FVector Start,FVector Aim,FVector Velocity,FVector& Launch,FVector& Intercept) const;
    bool ClearArc(FVector Start,FVector Launch,float FlightTime,FString& Reason) const;
    void UpdateTactics();
    FVector AimPoint(AActor* Ship) const;
    UPROPERTY(Transient) TObjectPtr<UShipWeaponComponent> Weapon;
    UPROPERTY(Transient) TObjectPtr<UShipEnemyNavigationComponent> Navigation;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Visual;
    float ScanClock=1000,FireCheckClock=0;
    double LastFire=-1000,LastLog=-1000;
    double StateSince=0,LastRetreatEnded=-1000;
    bool bVerifiedAxes=false;
};

UCLASS()
class VDTPHASE2PROJECT_API UShipCombatLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Player input is returned unchanged; AI angles feed the SAME two existing bone nodes. */
    UFUNCTION(BlueprintPure, Category="Combat") static FRotator SelectGunAim(APawn* Ship,FRotator ManualAim);
    UFUNCTION(BlueprintCallable, Category="Combat|Editor",meta=(DevelopmentOnly)) static FString InstallCombatIntegration();
};
