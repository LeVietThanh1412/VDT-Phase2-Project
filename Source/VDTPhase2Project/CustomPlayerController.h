#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CustomPlayerController.generated.h"

class AFreeCameraPawn;
class UShipDetailsUI;

/** Views ships with Q without taking their AI controllers; F toggles free flight. */
UCLASS()
class VDTPHASE2PROJECT_API ACustomPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    /** Returns the ship selected by the controller, including while free camera is active. */
    APawn* GetSelectedShip() const;

private:
    void ToggleFreeCamera();
    UFUNCTION(BlueprintCallable, Category="Camera")
    void CycleShipCamera();
    void ToggleShipDetailsUI();

private:
    UPROPERTY()
    TObjectPtr<APawn> PreviousPawn;

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
