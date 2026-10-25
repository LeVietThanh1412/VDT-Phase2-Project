#include "FreeCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "CesiumGlobeAnchorComponent.h"
#include "CesiumGeoreference.h"

namespace
{
    constexpr float MinFreeCameraSpeed = 25000.0f;
    constexpr float MaxFreeCameraSpeed = 500000.0f;
    constexpr float FreeCameraSpeedStep = 5000.0f;
}

AFreeCameraPawn::AFreeCameraPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    bUseControllerRotationPitch = true;
    bUseControllerRotationYaw = true;
    bUseControllerRotationRoll = true;

    CollisionRoot = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionRoot"));
    CollisionRoot->InitSphereRadius(16.0f);
    CollisionRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RootComponent = CollisionRoot;
    GlobeAnchor=CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("GlobeAnchor"));
    GlobeAnchor->SetAdjustOrientationForGlobeWhenMoving(false);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(RootComponent);
    Camera->bUsePawnControlRotation = true;

    Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
    Movement->MaxSpeed = 250000.0f;
    Movement->Acceleration = 100000.0f;
    Movement->Deceleration = 500000.0f;
}

void AFreeCameraPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    FVector Location = GetActorLocation();
    if(bGeographicHeightLimits && GlobeAnchor)
        if(auto* Geo=GlobeAnchor->ResolveGeoreference())
        {
            FVector LLH=Geo->TransformUnrealPositionToLongitudeLatitudeHeight(Location);
            const double Height=FMath::Clamp(LLH.Z,MinimumHeightM,MaximumHeightM);
            if(!FMath::IsNearlyEqual(LLH.Z,Height,.01))
            {LLH.Z=Height;SetActorLocation(Geo->TransformLongitudeLatitudeHeightPositionToUnreal(LLH),false,nullptr,ETeleportType::TeleportPhysics);}
            return;
        }
    const float ClampedZ = FMath::Clamp(Location.Z, MinimumZ, MaximumZ);
    if (!FMath::IsNearlyEqual(Location.Z, ClampedZ))
    {
        Location.Z = ClampedZ;
        SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
    }
}

void AFreeCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    check(PlayerInputComponent);

    PlayerInputComponent->BindAxis(TEXT("FreeCamera_MoveForward"), this, &AFreeCameraPawn::MoveForward);
    PlayerInputComponent->BindAxis(TEXT("FreeCamera_MoveRight"), this, &AFreeCameraPawn::MoveRight);
    PlayerInputComponent->BindAxis(TEXT("FreeCamera_AdjustSpeed"), this, &AFreeCameraPawn::AdjustSpeed);
    PlayerInputComponent->BindAxis(TEXT("FreeCamera_Turn"), this, &AFreeCameraPawn::Turn);
    PlayerInputComponent->BindAxis(TEXT("FreeCamera_LookUp"), this, &AFreeCameraPawn::LookUp);
}

void AFreeCameraPawn::SetHeightLimits(float InMinimumZ, float InMaximumZ)
{
    MinimumZ = FMath::Min(InMinimumZ, InMaximumZ);
    MaximumZ = FMath::Max(InMinimumZ, InMaximumZ);
    if(GlobeAnchor)if(auto* Geo=GlobeAnchor->ResolveGeoreference())
    {
        FVector Position=GetActorLocation();Position.Z=MinimumZ;
        MinimumHeightM=Geo->TransformUnrealPositionToLongitudeLatitudeHeight(Position).Z;
        Position.Z=MaximumZ;MaximumHeightM=Geo->TransformUnrealPositionToLongitudeLatitudeHeight(Position).Z;
        bGeographicHeightLimits=true;
    }
}

void AFreeCameraPawn::MoveForward(float Value)
{
    if (!FMath::IsNearlyZero(Value))
    {
        // Follow the complete view direction so looking up/down changes the flight path.
        AddMovementInput(GetControlRotation().Vector(), Value);
    }
}

void AFreeCameraPawn::MoveRight(float Value)
{
    if (!FMath::IsNearlyZero(Value))
    {
        AddMovementInput(FRotationMatrix(FRotator(0.0f, GetControlRotation().Yaw, 0.0f)).GetUnitAxis(EAxis::Y), Value);
    }
}

void AFreeCameraPawn::AdjustSpeed(float Value)
{
    if (!Movement || FMath::IsNearlyZero(Value))
    {
        return;
    }

    const float NewSpeed = Movement->MaxSpeed + FMath::Sign(Value) * FreeCameraSpeedStep;
    Movement->MaxSpeed = FMath::Clamp(NewSpeed, MinFreeCameraSpeed, MaxFreeCameraSpeed);
}

void AFreeCameraPawn::Turn(float Value)
{
    AddControllerYawInput(Value);
}

void AFreeCameraPawn::LookUp(float Value)
{
    AddControllerPitchInput(Value);
}
