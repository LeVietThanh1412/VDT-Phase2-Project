#include "CustomPlayerController.h"

#include "Actor/Pawn/FreeCameraPawn.h"
#include "UI/ShipDetailsUI.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "ShipCombatComponent.h"

void ACustomPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController() || IsValid(ShipDetailsUI))
    {
        return;
    }

    ShipDetailsUI = CreateWidget<UShipDetailsUI>(this, UShipDetailsUI::StaticClass());
    if (IsValid(ShipDetailsUI))
    {
        ShipDetailsUI->AddToViewport();
        ShipDetailsUI->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void ACustomPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    check(InputComponent);
    InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ACustomPlayerController::ToggleFreeCamera);
    InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ACustomPlayerController::CycleShipCamera);
    InputComponent->BindKey(EKeys::I, IE_Pressed, this, &ACustomPlayerController::ToggleShipDetailsUI);
}

APawn* ACustomPlayerController::GetSelectedShip() const
{
    return IsValid(FreeCameraPawn) ? PreviousPawn.Get() : GetPawn().Get();
}

void ACustomPlayerController::ToggleShipDetailsUI()
{
    if (!IsLocalController())
    {
        return;
    }

    const bool bCreatedWidget = !IsValid(ShipDetailsUI);
    if (bCreatedWidget)
    {
        ShipDetailsUI = CreateWidget<UShipDetailsUI>(this, UShipDetailsUI::StaticClass());
        if (!IsValid(ShipDetailsUI))
        {
            return;
        }

        ShipDetailsUI->AddToViewport();
    }

    ShipDetailsUI->SetSelectedShip(GetSelectedShip());
    const ESlateVisibility NewVisibility = bCreatedWidget || ShipDetailsUI->GetVisibility() != ESlateVisibility::Visible
        ? ESlateVisibility::Visible
        : ESlateVisibility::Collapsed;
    ShipDetailsUI->SetVisibility(NewVisibility);
}

void ACustomPlayerController::CycleShipCamera()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // A camera change must not unpossess an independently controlled AI ship.
    AActor* CurrentShip = IsValid(FreeCameraPawn) ? PreviousPawn.Get() : GetViewTarget();

    TArray<APawn*> ShipPawns;
    for (TActorIterator<APawn> It(World); It; ++It)
    {
        APawn* Candidate = *It;
        const UShipWeaponComponent* Weapon = Candidate->FindComponentByClass<UShipWeaponComponent>();
        if (IsValid(Candidate) && Weapon && !Weapon->bIsDestroyed)
        {
            ShipPawns.Add(Candidate);
        }
    }

    ShipPawns.Sort([](const APawn& Left, const APawn& Right)
    {
        return Left.GetPathName() < Right.GetPathName();
    });

    if (ShipPawns.Num() == 0)
    {
        return;
    }

    APawn* NextShip = nullptr;
    if (ShipPawns.Num() == 1)
    {
        // Only one ship exists: Q has no target to cycle to, but it must still pull the
        // camera back out of free camera and onto that ship.
        NextShip = ShipPawns[0];
        if (!IsValid(FreeCameraPawn))
        {
            return;
        }
    }
    else
    {
        const int32 CurrentIndex = ShipPawns.IndexOfByKey(CurrentShip);
        const int32 NextIndex = CurrentIndex == INDEX_NONE ? 0 : (CurrentIndex + 1) % ShipPawns.Num();
        NextShip = ShipPawns[NextIndex];
        if (!IsValid(NextShip) || NextShip == CurrentShip)
        {
            return;
        }
    }

    if (IsValid(FreeCameraPawn))
    {
        SavedFreeCameraLocation = FreeCameraPawn->GetActorLocation();
        SavedFreeCameraRotation = GetControlRotation();
        bHasSavedFreeCameraTransform = true;
        ToggleFreeCamera();
    }

    SetViewTargetWithBlend(NextShip, 0.25f);
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

    FVector ActivationLocation;
    FRotator ActivationRotation;
    GetPlayerViewPoint(ActivationLocation, ActivationRotation);
    const bool bRestoreSavedCamera = bHasSavedFreeCameraTransform;
    const FVector FreeSpawnLocation = bRestoreSavedCamera
        ? SavedFreeCameraLocation
        : ActivationLocation + FVector(0.0f, 0.0f, 150.0f);
    const FRotator SpawnRotation = bRestoreSavedCamera
        ? SavedFreeCameraRotation
        : ActivationRotation;
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
