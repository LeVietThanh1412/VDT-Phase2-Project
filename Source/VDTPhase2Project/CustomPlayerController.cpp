#include "CustomPlayerController.h"

#include "Actor/Pawn/FreeCameraPawn.h"
#include "UI/ButtonTutorialsUI.h"
#include "UI/ShipDetailsUI.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "ShipCombatComponent.h"
#include "NavalMatchSettings.h"
#include "CesiumGlobeAnchorComponent.h"
#include "CesiumGeoreference.h"
#include "ShipSetupWidget.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

void ACustomPlayerController::BeginPlay()
{
    Super::BeginPlay();
    const auto* Settings = GetGameInstance()->GetSubsystem<UNavalMatchSettings>();
    // A fresh game opens the setup first; after StartMatch the settings are already explicit.
    if (IsLocalController() && Settings && !Settings->bExplicitSelection)
        GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() {ToggleMatchSetup(); }));
    ViewedShip = GetPawn();

    if (!IsLocalController())
    {
        return;
    }

    if (!IsValid(ShipDetailsUI))
    {
        ShipDetailsUI = CreateWidget<UShipDetailsUI>(this, UShipDetailsUI::StaticClass());
        if (IsValid(ShipDetailsUI))
        {
            ShipDetailsUI->AddToViewport();
            ShipDetailsUI->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (!IsValid(ButtonTutorialsUI))
    {
        ButtonTutorialsUI = CreateWidget<UButtonTutorialsUI>(this, UButtonTutorialsUI::StaticClass());
        if (IsValid(ButtonTutorialsUI))
        {
            ButtonTutorialsUI->AddToViewport(10);
        }
    }
}

void ACustomPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    check(InputComponent);
    InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ACustomPlayerController::CycleShipCamera);
    InputComponent->BindKey(EKeys::I, IE_Pressed, this, &ACustomPlayerController::ToggleShipDetailsUI);
    InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ACustomPlayerController::NextTeammate);
    InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ACustomPlayerController::ToggleBattlefieldOverview);
    InputComponent->BindKey(EKeys::Slash, IE_Pressed, this, &ACustomPlayerController::ToggleButtonTutorials);
    InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ACustomPlayerController::ToggleMatchSetup).bExecuteWhenPaused=true;
}

void ACustomPlayerController::ToggleMatchSetup()
{
    if(SetupWidget){CloseMatchSetup();return;}
    if(!IsLocalController())return;
    SetupWidget=CreateWidget<UShipSetupWidget>(this,UShipSetupWidget::StaticClass());
    if(!SetupWidget)return;
    bMenuPausedWorld=!GetWorld()->IsPaused();
    SetPause(true);SetupWidget->AddToViewport(100);bShowMouseCursor=true;
    FInputModeGameAndUI Mode;Mode.SetWidgetToFocus(SetupWidget->TakeWidget());Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);SetupWidget->SetKeyboardFocus();
}

void ACustomPlayerController::CloseMatchSetup()
{
    if(SetupWidget)
    {
        if(auto* Settings=GetGameInstance()?GetGameInstance()->GetSubsystem<UNavalMatchSettings>():nullptr)
            Settings->bExplicitSelection=true;
        SetupWidget->RemoveFromParent();SetupWidget=nullptr;
    }
    bShowMouseCursor=false;SetInputMode(FInputModeGameOnly());
    if(bMenuPausedWorld)SetPause(false);bMenuPausedWorld=false;
}

void ACustomPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if(InPawn && InPawn->FindComponentByClass<UShipWeaponComponent>() && !bDeathSpectating)
        OriginalPlayerShip=InPawn;
}

void ACustomPlayerController::EnterDeathSpectator()
{
    bDeathSpectating=true;
    bBattlefieldOverview=false;
    ResetIgnoreMoveInput();ResetIgnoreLookInput();
    // Only our own inspection pawn is possessed. Teammate controllers never change.
    if(!IsValid(FreeCameraPawn))
    {
        FVector Eye;FRotator Rotation;GetPlayerViewPoint(Eye,Rotation);
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        FreeCameraPawn=GetWorld()->SpawnActor<AFreeCameraPawn>(Eye,Rotation,Params);
    }
    if(IsValid(FreeCameraPawn))Possess(FreeCameraPawn);
    PreviousPawn=nullptr;
    NextTeammate();
}

void ACustomPlayerController::NextTeammate()
{
    if(!bDeathSpectating)return;
    const auto* Settings=GetGameInstance()?GetGameInstance()->GetSubsystem<UNavalMatchSettings>():nullptr;
    const int32 PlayerTeam=Settings && Settings->bExplicitSelection?FMath::Clamp(Settings->PlayerTeam,0,1):0;
    TArray<APawn*> Friends;
    for(TActorIterator<APawn> It(GetWorld());It;++It)
        if(const auto* W=It->FindComponentByClass<UShipWeaponComponent>();W && W->Team()==PlayerTeam && !W->bIsDestroyed)Friends.Add(*It);
    Friends.Sort([](const APawn& A,const APawn& B){return A.GetPathName()<B.GetPathName();});
    if(Friends.IsEmpty()){ShowBattlefieldOverview();return;}
    const int32 Index=Friends.IndexOfByKey(FollowedTeammate);
    FollowedTeammate=Friends[(Index+1)%Friends.Num()];
    SetViewTargetWithBlend(FollowedTeammate,.25f);
}

void ACustomPlayerController::ShowBattlefieldOverview()
{
    // The overview is also available while the player's ship is still afloat:
    // borrow the inspection camera, then let ToggleBattlefieldOverview restore.
    if(!IsValid(FreeCameraPawn))
    {
        if(bDeathSpectating)return;
        ToggleFreeCamera();
        if(!IsValid(FreeCameraPawn))return;
    }
    FVector Focus=FVector::ZeroVector;int32 Count=0;
    for(TActorIterator<APawn> It(GetWorld());It;++It)
        if(const auto* W=It->FindComponentByClass<UShipWeaponComponent>();W && !W->bIsDestroyed){Focus+=It->GetActorLocation();++Count;}
    if(Count)Focus/=Count;
    else if(IsValid(OriginalPlayerShip))Focus=OriginalPlayerShip->GetActorLocation();
    else Focus=FreeCameraPawn->GetActorLocation();
    FVector Up=FVector::UpVector;
    if(auto* Anchor=FreeCameraPawn->FindComponentByClass<UCesiumGlobeAnchorComponent>())
        if(auto* Geo=Anchor->ResolveGeoreference())
        {
            const FVector LLH=Geo->TransformUnrealPositionToLongitudeLatitudeHeight(Focus);
            Up=(Geo->TransformLongitudeLatitudeHeightPositionToUnreal(LLH+FVector(0,0,1))-Focus).GetSafeNormal();
        }
    const FVector Eye=Focus+Up*180000-FVector::VectorPlaneProject(FVector(1,0,0),Up).GetSafeNormal()*65000;
    FreeCameraPawn->SetActorLocation(Eye,false,nullptr,ETeleportType::TeleportPhysics);
    FreeCameraPawn->SetHeightLimits(Eye.Z-180000,Eye.Z+180000);
    SetControlRotation((Focus-Eye).Rotation());FollowedTeammate=nullptr;
    SetViewTargetWithBlend(FreeCameraPawn,.25f);
}

void ACustomPlayerController::ToggleBattlefieldOverview()
{
    // Death spectator already owns the inspection camera; re-center the overview.
    if(bDeathSpectating)
    {
        ShowBattlefieldOverview();
        return;
    }

    if(bBattlefieldOverview)
    {
        bBattlefieldOverview=false;
        RestoreRecentShipView();
        return;
    }

    ShowBattlefieldOverview();
    bBattlefieldOverview=IsValid(FreeCameraPawn);
}

void ACustomPlayerController::RestoreRecentShipView()
{
    // Q and F both leave the overview by returning to the ship the player was
    // watching before the overview, not to whichever pawn was possessed.
    APawn* RecentShip=ViewedShip.Get();
    if(IsValid(FreeCameraPawn))ToggleFreeCamera();
    if(IsValid(RecentShip))
    {
        ApplyShipView(RecentShip);
        SetShipViewTarget(RecentShip,.25f);
    }
}

APawn* ACustomPlayerController::GetSelectedShip() const
{
    // Q only changes the view target so AI stays in control of its ship. The details
    // panel must follow that view target, not the pawn the controller still possesses.
    if (IsValid(ViewedShip))
    {
        return ViewedShip.Get();
    }

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

void ACustomPlayerController::ToggleButtonTutorials()
{
    if (IsValid(ButtonTutorialsUI))
    {
        ButtonTutorialsUI->ToggleTutorial();
    }
}

void ACustomPlayerController::CycleShipCamera()
{
    if(bDeathSpectating){NextTeammate();return;}
    if(bBattlefieldOverview)
    {
        bBattlefieldOverview=false;
        RestoreRecentShipView();
        return;
    }
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // A camera change must not unpossess an independently controlled AI ship.
    APawn* CurrentShip = ViewedShip.Get();
    if (!IsValid(CurrentShip))
    {
        CurrentShip = IsValid(FreeCameraPawn) ? PreviousPawn.Get() : Cast<APawn>(GetViewTarget());
    }

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
        if (!IsValid(FreeCameraPawn) && NextShip == CurrentShip)
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
    bBattlefieldOverview=false;

    ApplyShipView(NextShip);
    SetShipViewTarget(NextShip, 0.25f);
}

void ACustomPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    const auto* Weapon = IsValid(OriginalPlayerShip) ? OriginalPlayerShip->FindComponentByClass<UShipWeaponComponent>() : nullptr;
    if (!bDeathSpectating && Weapon && Weapon->bIsDestroyed)EnterDeathSpectator();
    if (bDeathSpectating && FollowedTeammate)
    {
        const auto* FollowWeapon = IsValid(FollowedTeammate) ? FollowedTeammate->FindComponentByClass<UShipWeaponComponent>() : nullptr;
        if (!FollowWeapon || FollowWeapon->bIsDestroyed)NextTeammate();
    }
    if (!IsValid(ActiveShipViewCamera))
    {
        return;
    }

    if (!IsValid(ViewedShip) || ViewedShip->IsPlayerControlled())
    {
        // The observed AI wreck was cleaned up; never leave the player staring
        // at a camera that no longer follows anything.
        ActiveShipViewCamera = nullptr;
        if (APawn* PlayerPawn = GetPawn())
        {
            ApplyShipView(PlayerPawn);
            SetViewTargetWithBlend(PlayerPawn, 0.25f);
        }
        return;
    }

    // Update the incoming camera, not GetViewTarget(): while the camera manager
    // blends, the view target is still the outgoing camera and moving it would
    // collapse the transition into a cut.
    UpdateShipViewCamera(ActiveShipViewCamera);
}

ACameraActor* ACustomPlayerController::CreateShipViewCamera()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ACameraActor* Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, SpawnParameters);
    if (IsValid(Camera))
    {
        Camera->SetActorEnableCollision(false);
        Camera->GetCameraComponent()->SetFieldOfView(90.0f);
    }

    return Camera;
}

ACameraActor* ACustomPlayerController::GetNextShipViewCamera()
{
    // The view target must actually change for ViewTarget blending to play.
    // Keep three cameras so a rapid Q press never has to reuse the outgoing
    // camera (which would turn the next blend into a no-op).
    constexpr int32 CameraCount = 3;
    if (ShipViewCameras.Num() < CameraCount)
    {
        ShipViewCameras.SetNum(CameraCount);
    }

    for (TObjectPtr<ACameraActor>& Camera : ShipViewCameras)
    {
        if (!IsValid(Camera))
        {
            Camera = CreateShipViewCamera();
        }
    }

    AActor* CurrentViewTarget = GetViewTarget();
    for (const TObjectPtr<ACameraActor>& Camera : ShipViewCameras)
    {
        if (IsValid(Camera.Get()) && Camera.Get() != CurrentViewTarget && Camera.Get() != ActiveShipViewCamera.Get())
        {
            return Camera.Get();
        }
    }

    return nullptr;
}

void ACustomPlayerController::SetShipViewTarget(APawn* InShip, float BlendTime)
{
    if (!IsValid(InShip))
    {
        return;
    }

    // AI ships keep their own controller. Their spring arm follows the AI control
    // rotation, so a separate camera is needed to give the player mouse look.
    if (!InShip->IsPlayerControlled())
    {
        if (ACameraActor* Camera = GetNextShipViewCamera())
        {
            ActiveShipViewCamera = Camera;
            UpdateShipViewCamera(Camera);
            SetViewTargetWithBlend(Camera, BlendTime);
            return;
        }
    }

    ActiveShipViewCamera = nullptr;
    SetViewTargetWithBlend(InShip, BlendTime);
}

void ACustomPlayerController::UpdateShipViewCamera(ACameraActor* Camera)
{
    if (!IsValid(Camera) || !IsValid(ViewedShip) || ViewedShip->IsPlayerControlled())
    {
        return;
    }

    FVector Focus = ViewedShip->GetActorLocation();
    float PreferredDistance = 15000.0f;
    TArray<USpringArmComponent*> SpringArms;
    ViewedShip->GetComponents<USpringArmComponent>(SpringArms);
    for (USpringArmComponent* SpringArm : SpringArms)
    {
        if (IsValid(SpringArm) && !SpringArm->GetName().Contains(TEXT("Gun")))
        {
            Focus += ViewedShip->GetActorUpVector() * SpringArm->SocketOffset.Z;
            PreferredDistance = FMath::Clamp(SpringArm->TargetArmLength, 2000.0f, 30000.0f);
            break;
        }
    }

    FRotator ViewRotation = GetControlRotation();
    ViewRotation.Roll = 0.0f;
    // Keeping the orbit at or above the focus height avoids dropping the camera
    // under the sea when the player looks up. Controller pitch can arrive
    // unnormalised (for example 350 instead of -10), so normalise it first.
    ViewRotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(ViewRotation.Pitch), -80.0f, 0.0f);
    const FVector CameraLocation = Focus - ViewRotation.Vector() * PreferredDistance;
    Camera->SetActorLocationAndRotation(CameraLocation, ViewRotation);

    if (UCameraComponent* CameraComponent = Camera->GetCameraComponent())
    {
        CameraComponent->SetFieldOfView(90.0f);
    }
}

void ACustomPlayerController::ApplyShipView(APawn* InShip)
{
    if (!IsValid(InShip) || ViewedShip == InShip)
    {
        return;
    }

    SetHorizonLock(ViewedShip.Get(), false);
    ViewedShip = InShip;
    // The player's own spring arms already use the (roll-free) player control
    // rotation, so only observed AI ships need the horizon correction.
    SetHorizonLock(InShip, !InShip->IsPlayerControlled());
}

void ACustomPlayerController::SetHorizonLock(APawn* InShip, bool bEnabled)
{
    for (auto It = SavedSpringArmRollInheritance.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid())
        {
            It.RemoveCurrent();
        }
    }

    if (!IsValid(InShip))
    {
        return;
    }

    TArray<USpringArmComponent*> SpringArms;
    InShip->GetComponents<USpringArmComponent>(SpringArms);
    for (USpringArmComponent* SpringArm : SpringArms)
    {
        if (!IsValid(SpringArm))
        {
            continue;
        }

        if (bEnabled)
        {
            if (!SavedSpringArmRollInheritance.Contains(SpringArm))
            {
                SavedSpringArmRollInheritance.Add(SpringArm, SpringArm->bInheritRoll);
            }
            SpringArm->bInheritRoll = false;
        }
        else if (const bool* SavedInheritRoll = SavedSpringArmRollInheritance.Find(SpringArm))
        {
            SpringArm->bInheritRoll = *SavedInheritRoll;
            SavedSpringArmRollInheritance.Remove(SpringArm);
        }
    }
}

void ACustomPlayerController::ToggleFreeCamera()
{
    if(bDeathSpectating){ShowBattlefieldOverview();return;}
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
            // Free camera owns no ship: hand both the view and the details panel
            // back to the possessed pawn.
            ApplyShipView(PawnToRestore);
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
