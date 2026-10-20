#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CustomPlayerController.generated.h"

class AFreeCameraPawn;

/** Switches between ship pawns with Q and a bounded free-flight camera with F. */
UCLASS()
class VDTPHASE2PROJECT_API ACustomPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void SetupInputComponent() override;

private:
    void ToggleFreeCamera();
    void CycleShipCamera();

    UPROPERTY()
    TObjectPtr<APawn> PreviousPawn;

    UPROPERTY()
    TObjectPtr<AFreeCameraPawn> FreeCameraPawn;

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
