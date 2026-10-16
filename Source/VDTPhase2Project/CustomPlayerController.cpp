#include "CustomPlayerController.h"

#include "Actor/Pawn/FreeCameraPawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

void ACustomPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    check(InputComponent);
    InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ACustomPlayerController::ToggleFreeCamera);
    InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ACustomPlayerController::CycleShipCamera);
}

void ACustomPlayerController::CycleShipCamera()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // When Q is pressed in free camera, continue the cycle from the ship that was
    // active before entering free camera.
    APawn* CurrentShip = IsValid(FreeCameraPawn) ? PreviousPawn.Get() : GetPawn().Get();

    TArray<APawn*> ShipPawns;
    for (TActorIterator<APawn> It(World); It; ++It)
    {
        APawn* Candidate = *It;
        if (IsValid(Candidate) && Candidate != FreeCameraPawn)
        {
            ShipPawns.Add(Candidate);
        }
    }

    ShipPawns.Sort([](const APawn& Left, const APawn& Right)
    {
        return Left.GetPathName() < Right.GetPathName();
    });

    if (ShipPawns.Num() < 2)
    {
        return;
    }

    const int32 CurrentIndex = ShipPawns.IndexOfByKey(CurrentShip);
    const int32 NextIndex = CurrentIndex == INDEX_NONE ? 0 : (CurrentIndex + 1) % ShipPawns.Num();
    APawn* NextShip = ShipPawns[NextIndex];
    if (!IsValid(NextShip) || NextShip == CurrentShip)
    {
        return;
    }

    if (IsValid(FreeCameraPawn))
    {
        SavedFreeCameraLocation = FreeCameraPawn->GetActorLocation();
        SavedFreeCameraRotation = GetControlRotation();
        bHasSavedFreeCameraTransform = true;
        UnPossess();
        FreeCameraPawn->Destroy();
        FreeCameraPawn = nullptr;
        PreviousPawn = nullptr;
        PreviousControlRotation = FRotator::ZeroRotator;
    }

    const FRotator UprightRotation(0.0f, NextShip->GetActorRotation().Yaw, 0.0f);
    Possess(NextShip);
    SetControlRotation(UprightRotation);
}

void ACustomPlayerController::ToggleFreeCamera()
{
    // Keep free-camera transform across toggles, but never possess the same pawn twice.
    if (IsValid(FreeCameraPawn))
    {
        APawn* PawnToRestore = PreviousPawn.Get();
        SavedFreeCameraLocation = FreeCameraPawn->GetActorLocation();
        SavedFreeCameraRotation = GetControlRotation();
        bHasSavedFreeCameraTransform = true;
        UnPossess();
        FreeCameraPawn->Destroy();
        FreeCameraPawn = nullptr;

        if (IsValid(PawnToRestore))
        {
            Possess(PawnToRestore);
            FRotator RestoredControlRotation = PreviousControlRotation;
            RestoredControlRotation.Roll = 0.0f;
            SetControlRotation(RestoredControlRotation);
        }
        PreviousPawn = nullptr;
        PreviousControlRotation = FRotator::ZeroRotator;
        return;
    }

    APawn* CurrentPawn = GetPawn();
    if (!IsValid(CurrentPawn) || !GetWorld())
    {
        return;
    }

    const FVector ActivationLocation = CurrentPawn->GetActorLocation();
    const bool bRestoreSavedCamera = bHasSavedFreeCameraTransform;
    const FVector FreeSpawnLocation = bRestoreSavedCamera
        ? SavedFreeCameraLocation
        : ActivationLocation + FVector(0.0f, 0.0f, 150.0f);
    const FRotator SpawnRotation = bRestoreSavedCamera
        ? SavedFreeCameraRotation
        : GetControlRotation();
    const float MinimumZ = bRestoreSavedCamera
        ? SavedFreeCameraMinimumZ
        : ActivationLocation.Z - MaximumDepthBelowActivation;
    const float MaximumZ = bRestoreSavedCamera
        ? SavedFreeCameraMaximumZ
        : ActivationLocation.Z + MaximumHeightAboveActivation;
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AFreeCameraPawn* SpawnedCamera = GetWorld()->SpawnActor<AFreeCameraPawn>(
        AFreeCameraPawn::StaticClass(),
        FreeSpawnLocation,
        SpawnRotation,
        SpawnParameters);

    if (!IsValid(SpawnedCamera))
    {
        return;
    }

    SpawnedCamera->SetHeightLimits(MinimumZ, MaximumZ);
    SavedFreeCameraMinimumZ = MinimumZ;
    SavedFreeCameraMaximumZ = MaximumZ;
    PreviousPawn = CurrentPawn;
    PreviousControlRotation = GetControlRotation();
    FreeCameraPawn = SpawnedCamera;
    Possess(FreeCameraPawn);
    SetControlRotation(SpawnRotation);
}
