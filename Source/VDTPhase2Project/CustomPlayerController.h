#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CustomPlayerController.generated.h"

class ACameraActor;
class AFreeCameraPawn;
class UShipDetailsUI;
class USpringArmComponent;

/** Views ships with Q without taking their AI controllers; F toggles free flight. */
UCLASS()
class VDTPHASE2PROJECT_API ACustomPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual void SetupInputComponent() override;

    /** Returns the ship selected by the controller, including while free camera is active. */
    APawn* GetSelectedShip() const;

private:
    void ToggleFreeCamera();
    UFUNCTION(BlueprintCallable, Category="Camera")
    void CycleShipCamera();
    void ToggleShipDetailsUI();

    /** Makes the viewed ship the selection used by the camera and the details panel. */
    void ApplyShipView(APawn* InShip);

    /** Uses an orbit camera for AI ships so the player can look around without possessing them. */
    void SetShipViewTarget(APawn* InShip, float BlendTime);
    void UpdateShipViewCamera(ACameraActor* Camera);
    ACameraActor* GetNextShipViewCamera();
    ACameraActor* CreateShipViewCamera();

    /**
     * AI controllers keep the hull's roll in their control rotation, and the ship's
     * spring arms inherit it, which tilts the view. Level only the arms of the ship
     * being observed, then restore their authored values when the player looks away.
     */
    void SetHorizonLock(APawn* InShip, bool bEnabled);

private:
    UPROPERTY()
    TObjectPtr<APawn> PreviousPawn;

    /** Ship the player is currently watching with Q (not necessarily the possessed pawn). */
    UPROPERTY()
    TObjectPtr<APawn> ViewedShip;

    /**
     * Orbit cameras alternate as view targets. Reusing one camera for every
     * AI ship makes SetViewTargetWithBlend a no-op, so enemy-to-enemy switches
     * would cut instead of blending. Three cameras also let a rapid Q press
     * replace a blend that is still running.
     */
    UPROPERTY()
    TArray<TObjectPtr<ACameraActor>> ShipViewCameras;

    /** Incoming camera while a blend is running; the outgoing camera must stay put. */
    UPROPERTY()
    TObjectPtr<ACameraActor> ActiveShipViewCamera;

    TMap<TWeakObjectPtr<USpringArmComponent>, bool> SavedSpringArmRollInheritance;

    UPROPERTY()
    TObjectPtr<AFreeCameraPawn> FreeCameraPawn;

    UPROPERTY(Transient)
    TObjectPtr<UShipDetailsUI> ShipDetailsUI;

    // The normal pawn's camera rotation must survive a round trip through free camera.
    FRotator PreviousControlRotation = FRotator::ZeroRotator;

    // Free-camera state survives toggling back to the normal pawn during this play session.
    bool bHasSavedFreeCameraTransform = false;
    FVector SavedFreeCameraLocation = FVector::ZeroVector;
    FRotator SavedFreeCameraRotation = FRotator::ZeroRotator;
    float SavedFreeCameraMinimumZ = 0.0f;
    float SavedFreeCameraMaximumZ = 0.0f;

    /** The camera cannot rise more than 50 m above the location where it was activated. */
    UPROPERTY(EditDefaultsOnly, Category = "Free Camera", meta = (ClampMin = "0.0"))
    float MaximumHeightAboveActivation = 50000.0f;

    /** Allows the player to descend slightly, without seeing underneath the mapped area. */
    UPROPERTY(EditDefaultsOnly, Category = "Free Camera", meta = (ClampMin = "0.0"))
    float MaximumDepthBelowActivation = 250.0f;
};
