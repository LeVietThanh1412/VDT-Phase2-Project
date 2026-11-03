#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/WorldSubsystem.h"
#include "NavalMatchSettings.h"
#include "NavalBattleUI.generated.h"

class UShipWeaponComponent;
class UImage;
class UProgressBar;
class UTextBlock;

/** Persistent per-ship scoreboard row; survives the wreck's staged cleanup. */
struct FNavalShipScore
{
    FString ShipName;
    int32 Team=0;
    int32 Slot=0;
    float DamageDealt=0;
    float DamageTaken=0;
    int32 Eliminations=0;
    bool bSunk=false;
};

/** Native base for WBP_MainMinimap: removes a ship's marker once it is sunk. */
UCLASS()
class VDTPHASE2PROJECT_API UShipMinimapWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
private:
    void RemoveDestroyedMarkers();
    FTimerHandle MarkerTimer;
    bool bWarnedMissingArrays=false;
};

/** Native marker base: keeps WBP_ShipMarker's icon in sync with its ship's TeamName. */
UCLASS(Blueprintable)
class VDTPHASE2PROJECT_API UShipMarkerWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
private:
    UPROPERTY(Transient) TObjectPtr<UImage> MarkerImage;
};

/** Presentation is inherited by the saved WBP assets; all values are event driven. */
UCLASS()
class VDTPHASE2PROJECT_API UShipHealthBarWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Refresh(UShipWeaponComponent* Ship);
    UPROPERTY(BlueprintReadOnly) float DisplayedFraction=1;
    UPROPERTY(BlueprintReadOnly) FLinearColor HealthColor=FLinearColor::Green;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
private:
    UPROPERTY(Transient) TObjectPtr<UShipWeaponComponent> BoundShip;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> Bar;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Label;
};

UCLASS()
class VDTPHASE2PROJECT_API UNavalBattleSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
    virtual void OnWorldBeginPlay(UWorld& World) override;
    virtual void Deinitialize() override;
    void RegisterShip(UShipWeaponComponent* Ship);
    void ShipDestroyed(UShipWeaponComponent* Ship);
    /** Damage is recorded in the subsystem so the summary survives wreck cleanup. */
    void RecordDamage(UShipWeaponComponent* Source,UShipWeaponComponent* Victim,float Damage);
    void RecordElimination(UShipWeaponComponent* Killer);
    const TArray<FNavalShipScore>& GetScoreEntries() const { return ScoreEntries; }
    UFUNCTION(BlueprintCallable) void SetBattleMode(ENavalBattleMode Mode);
    UFUNCTION(BlueprintPure) float TurnSecondsRemaining() const;
    UFUNCTION(BlueprintPure) bool IsShipActive(AActor* Ship) const;
    static bool CanShipAct(AActor* Ship);
    UPROPERTY(BlueprintReadOnly) ENavalBattleMode BattleMode=ENavalBattleMode::RealTime;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> ActiveTurnShip;
    UPROPERTY(BlueprintReadOnly) int32 TurnNumber=0;
    UPROPERTY(BlueprintReadOnly) FString ActiveTurnLabel;
    UPROPERTY(BlueprintReadOnly) bool bMatchConfigured=false;
    UPROPERTY(BlueprintReadOnly) int32 BlueAlive=0;
    UPROPERTY(BlueprintReadOnly) int32 RedAlive=0;
    UPROPERTY(BlueprintReadOnly) FString Outcome;
    UPROPERTY(BlueprintReadOnly) FString ConfigurationError;
    UPROPERTY(BlueprintReadOnly) FString Notification;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UShipWeaponComponent> PlayerShip;
private:
    TSet<TWeakObjectPtr<UShipWeaponComponent>> Registered;
    TSet<TWeakObjectPtr<UShipWeaponComponent>> Fallen;
    TArray<FNavalShipScore> ScoreEntries;
    TMap<TWeakObjectPtr<UShipWeaponComponent>,int32> ScoreIndexByShip;
    UPROPERTY(Transient) TObjectPtr<APawn> DemoCommandShip;
    FTimerHandle MatchTimer;
    TArray<TWeakObjectPtr<UShipWeaponComponent>> TurnRoster;
    TArray<FString> TurnLabels;
    int32 NextTeam=0;
    int32 TeamCursor[2]={0,0};
    double TurnEndsAt=0;
    void UpdateMatch();
    bool ConfigureRoster();
    void AdvanceTurn();
};
