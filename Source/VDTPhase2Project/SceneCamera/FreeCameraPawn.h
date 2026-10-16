#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FreeCameraPawn.generated.h"

class UCameraComponent;
class UFloatingPawnMovement;
class USphereComponent;

/** A temporary, collision-free inspection camera used by AFreeCameraPlayerController. */
UCLASS()
class VDTPHASE2PROJECT_API AFreeCameraPawn : public APawn
{
    GENERATED_BODY()

public:
    AFreeCameraPawn();

    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    /** Sets the world-space height range, normally relative to the player's location when F is pressed. */
    void SetHeightLimits(float InMinimumZ, float InMaximumZ);

private:
    void MoveForward(float Value);
    void MoveRight(float Value);
    void AdjustSpeed(float Value);
    void Turn(float Value);
    void LookUp(float Value);

    UPROPERTY(VisibleAnywhere, Category = "Free Camera")
    TObjectPtr<USphereComponent> CollisionRoot;

    UPROPERTY(VisibleAnywhere, Category = "Free Camera")
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(VisibleAnywhere, Category = "Free Camera")
    TObjectPtr<UFloatingPawnMovement> Movement;

    float MinimumZ = -TNumericLimits<float>::Max();
    float MaximumZ = TNumericLimits<float>::Max();
};
