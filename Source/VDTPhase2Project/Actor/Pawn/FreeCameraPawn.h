#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FreeCameraPawn.generated.h"

class UCameraComponent;
class UFloatingPawnMovement;
class USphereComponent;
class UCesiumGlobeAnchorComponent;

/** A temporary, collision-free inspection camera used by ACustomPlayerController. */
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

    UPROPERTY(VisibleAnywhere, Category="Free Camera") TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;
    bool bGeographicHeightLimits=false;
    double MinimumHeightM=0;
    double MaximumHeightM=0;

    float MinimumZ = -TNumericLimits<float>::Max();
    float MaximumZ = TNumericLimits<float>::Max();
};
