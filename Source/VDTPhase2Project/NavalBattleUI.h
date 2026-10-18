#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/WorldSubsystem.h"
#include "NavalBattleUI.generated.h"

class UShipWeaponComponent;
class UProgressBar;
class UTextBlock;

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
private:
    UPROPERTY(Transient) TObjectPtr<UProgressBar> Bar;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Label;
};

UCLASS()
class VDTPHASE2PROJECT_API UShipHUDWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Refresh(class UNavalBattleSubsystem* Battle);
    UPROPERTY(BlueprintReadOnly) FString HealthText;
    UPROPERTY(BlueprintReadOnly) FString ScoreText;
    UPROPERTY(BlueprintReadOnly) FString ResultText;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UPROPERTY(Transient) TObjectPtr<UProgressBar> Bar;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Health;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Score;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Notice;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Result;
};

UCLASS()
class VDTPHASE2PROJECT_API UNavalBattleSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
    virtual void Deinitialize() override;
    void RegisterShip(UShipWeaponComponent* Ship);
    void ShipDestroyed(UShipWeaponComponent* Ship);
    void RefreshHUD();
    UFUNCTION(BlueprintCallable) void EnsureHUD();
    UPROPERTY(BlueprintReadOnly) int32 BlueAlive=0;
    UPROPERTY(BlueprintReadOnly) int32 RedAlive=0;
    UPROPERTY(BlueprintReadOnly) FString Notification;
    UPROPERTY(BlueprintReadOnly) FString Outcome;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UShipHUDWidget> HUD;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UShipWeaponComponent> PlayerShip;
private:
    TSet<TWeakObjectPtr<UShipWeaponComponent>> Registered;
    TSet<TWeakObjectPtr<UShipWeaponComponent>> Fallen;
    FTimerHandle NoticeTimer;
};
