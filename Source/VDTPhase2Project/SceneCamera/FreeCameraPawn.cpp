#include "FreeCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/FloatingPawnMovement.h"

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

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(RootComponent);
    Camera->bUsePawnControlRotation = true;

    Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
    Movement->MaxSpeed = 500000.0f;
    Movement->Acceleration = 100000.0f;
    Movement->Deceleration = 500000.0f;
}

void AFreeCameraPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    FVector Location = GetActorLocation();
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
    PlayerInputComponent->BindAxis(TEXT("FreeCamera_MoveUp"), this, &AFreeCameraPawn::MoveUp);
    PlayerInputComponent->BindAxis(TEXT("FreeCamera_Turn"), this, &AFreeCameraPawn::Turn);
    PlayerInputComponent->BindAxis(TEXT("FreeCamera_LookUp"), this, &AFreeCameraPawn::LookUp);
}

void AFreeCameraPawn::SetHeightLimits(float InMinimumZ, float InMaximumZ)
{
    MinimumZ = FMath::Min(InMinimumZ, InMaximumZ);
    MaximumZ = FMath::Max(InMinimumZ, InMaximumZ);
}

void AFreeCameraPawn::MoveForward(float Value)
{
    if (!FMath::IsNearlyZero(Value))
    {
        AddMovementInput(FRotationMatrix(FRotator(0.0f, GetControlRotation().Yaw, 0.0f)).GetUnitAxis(EAxis::X), Value);
    }
}

void AFreeCameraPawn::MoveRight(float Value)
{
    if (!FMath::IsNearlyZero(Value))
    {
        AddMovementInput(FRotationMatrix(FRotator(0.0f, GetControlRotation().Yaw, 0.0f)).GetUnitAxis(EAxis::Y), Value);
    }
}

void AFreeCameraPawn::MoveUp(float Value)
{
    if (!FMath::IsNearlyZero(Value))
    {
        AddMovementInput(FVector::UpVector, Value);
    }
}

void AFreeCameraPawn::Turn(float Value)
{
    AddControllerYawInput(Value);
}

void AFreeCameraPawn::LookUp(float Value)
{
    AddControllerPitchInput(Value);
}
