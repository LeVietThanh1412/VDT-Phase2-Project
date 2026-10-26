#include "CustomPlayerController.h"

#include "Actor/Pawn/FreeCameraPawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "ShipCombatComponent.h"
#include "NavalBattleUI.h"
#include "CesiumGlobeAnchorComponent.h"
#include "CesiumGeoreference.h"
#include "ShipSetupWidget.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

void ACustomPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    check(InputComponent);
    InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ACustomPlayerController::ToggleFreeCamera);
    InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ACustomPlayerController::CycleShipCamera);
    InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ACustomPlayerController::NextTeammate);
    InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ACustomPlayerController::ShowBattlefieldOverview);
    InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ACustomPlayerController::ToggleMatchSetup).bExecuteWhenPaused=true;
}

void ACustomPlayerController::BeginPlay()
{
    Super::BeginPlay();
    const auto* Settings=GetGameInstance()->GetSubsystem<UNavalMatchSettings>();
    // Direct PIE remains a development 3v3; standalone opens the setup first.
    if(IsLocalController() && GetWorld()->WorldType==EWorldType::Game && Settings && !Settings->bExplicitSelection)
        GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this](){ToggleMatchSetup();}));
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
    if(SetupWidget)SetupWidget->RemoveFromParent();SetupWidget=nullptr;
    bShowMouseCursor=false;SetInputMode(FInputModeGameOnly());
    if(bMenuPausedWorld)SetPause(false);bMenuPausedWorld=false;
}

void ACustomPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if(InPawn && InPawn->FindComponentByClass<UShipWeaponComponent>() && !bDeathSpectating)
        OriginalPlayerShip=InPawn;
}

void ACustomPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    const auto* Weapon=IsValid(OriginalPlayerShip)?OriginalPlayerShip->FindComponentByClass<UShipWeaponComponent>():nullptr;
    if(!bDeathSpectating && Weapon && Weapon->bIsDestroyed)EnterDeathSpectator();
    if(bDeathSpectating && FollowedTeammate)
    {
        const auto* FollowWeapon=IsValid(FollowedTeammate)?FollowedTeammate->FindComponentByClass<UShipWeaponComponent>():nullptr;
        if(!FollowWeapon || FollowWeapon->bIsDestroyed)NextTeammate();
    }
}

void ACustomPlayerController::EnterDeathSpectator()
{
    bDeathSpectating=true;
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
    TArray<APawn*> Friends;
    for(TActorIterator<APawn> It(GetWorld());It;++It)
        if(const auto* W=It->FindComponentByClass<UShipWeaponComponent>();W && W->Team()==0 && !W->bIsDestroyed)Friends.Add(*It);
    Friends.Sort([](const APawn& A,const APawn& B){return A.GetPathName()<B.GetPathName();});
    if(Friends.IsEmpty()){ShowBattlefieldOverview();return;}
    const int32 Index=Friends.IndexOfByKey(FollowedTeammate);
    FollowedTeammate=Friends[(Index+1)%Friends.Num()];
    SetViewTargetWithBlend(FollowedTeammate,.25f);
    if(auto* Battle=GetWorld()->GetSubsystem<UNavalBattleSubsystem>())Battle->RefreshHUD();
}

void ACustomPlayerController::ShowBattlefieldOverview()
{
    if(!bDeathSpectating || !IsValid(FreeCameraPawn))return;
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
    if(auto* Battle=GetWorld()->GetSubsystem<UNavalBattleSubsystem>())Battle->RefreshHUD();
}

void ACustomPlayerController::CycleShipCamera()
{
    if(bDeathSpectating){NextTeammate();return;}
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

    if (ShipPawns.IsEmpty())
    {
        return;
    }

    const int32 CurrentIndex = ShipPawns.IndexOfByKey(Cast<APawn>(CurrentShip));
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
        ToggleFreeCamera();
    }

    SetViewTargetWithBlend(NextShip, 0.25f);
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
