#include "CustomPlayerController.h"

#include "Actor/Pawn/FreeCameraPawn.h"
#include "UI/ButtonTutorialsUI.h"
#include "UI/LoadingScreenWidget.h"
#include "UI/MatchSummaryWidget.h"
#include "UI/ShipDetailsUI.h"
#include "UI/TeamCountBarWidget.h"
#include "UI/KillFeedWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "NavalBattleUI.h"
#include "NavalBattlefieldSubsystem.h"
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
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"

namespace
{
    TMap<TWeakObjectPtr<UUserWidget>, ESlateVisibility> GLoadingHiddenWidgets;

    void HideLoadingWidgets(UObject* WorldContext)
    {
        for (auto It = GLoadingHiddenWidgets.CreateIterator(); It; ++It)
        {
            if (!It.Key().IsValid())
            {
                It.RemoveCurrent();
            }
        }

        TArray<UUserWidget*> Minimaps;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(WorldContext, Minimaps, UShipMinimapWidget::StaticClass(), true);
        for (UUserWidget* Minimap : Minimaps)
        {
            if (!IsValid(Minimap) || Minimap->GetVisibility() == ESlateVisibility::Collapsed)
            {
                continue;
            }

            const TWeakObjectPtr<UUserWidget> Key(Minimap);
            if (!GLoadingHiddenWidgets.Contains(Key))
            {
                GLoadingHiddenWidgets.Add(Key, Minimap->GetVisibility());
            }
            Minimap->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    void RestoreLoadingWidgets()
    {
        for (const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Pair : GLoadingHiddenWidgets)
        {
            if (UUserWidget* Minimap = Pair.Key.Get())
            {
                Minimap->SetVisibility(Pair.Value);
            }
        }
        GLoadingHiddenWidgets.Reset();
    }

    /** Passed as the team filter when every ship, regardless of team, is a candidate. */
    constexpr int32 AnyTeam = INDEX_NONE;

    void CollectShipPawns(const UWorld* World, int32 TeamFilter, TArray<APawn*>& OutShipPawns)
    {
        OutShipPawns.Reset();
        if (!World)
        {
            return;
        }

        for (TActorIterator<APawn> It(World); It; ++It)
        {
            APawn* Candidate = *It;
            const UShipWeaponComponent* Weapon = IsValid(Candidate)
                ? Candidate->FindComponentByClass<UShipWeaponComponent>()
                : nullptr;
            if (Weapon && !Weapon->bIsDestroyed && (TeamFilter == AnyTeam || Weapon->Team() == TeamFilter))
            {
                OutShipPawns.Add(Candidate);
            }
        }

        OutShipPawns.Sort([](const APawn& Left, const APawn& Right)
        {
            return Left.GetPathName() < Right.GetPathName();
        });
    }
}

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

    if (Settings && Settings->bExplicitSelection)
    {
        ShowLoadingScreen();
    }

    if (!IsValid(ShipDetailsUI))
    {
        ShipDetailsUI = CreateWidget<UShipDetailsUI>(this, UShipDetailsUI::StaticClass());
        if (IsValid(ShipDetailsUI))
        {
            ShipDetailsUI->AddToViewport();
            // The details panel is opened with I; do not cover the battlefield by default.
            ShipDetailsUI->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (!IsValid(TeamCountBarUI))
    {
        TeamCountBarUI = CreateWidget<UTeamCountBarWidget>(this, UTeamCountBarWidget::StaticClass());
        if (IsValid(TeamCountBarUI))
        {
            TeamCountBarUI->AddToViewport();
            TeamCountBarUI->SetVisibility(IsValid(LoadingScreenUI)
                ? ESlateVisibility::Collapsed
                : ESlateVisibility::HitTestInvisible);
        }
    }

    if (!IsValid(KillFeedUI))
    {
        KillFeedUI = CreateWidget<UKillFeedWidget>(this, UKillFeedWidget::StaticClass());
        if (IsValid(KillFeedUI))
        {
            KillFeedUI->AddToViewport();
            KillFeedUI->SetVisibility(ESlateVisibility::HitTestInvisible);
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

void ACustomPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LoadingWatchTimer);
        World->GetTimerManager().ClearTimer(LoadingTravelTimer);
    }

    for (TObjectPtr<ACameraActor>& Camera : ShipViewCameras)
    {
        if (IsValid(Camera))
        {
            Camera->Destroy();
        }
    }
    ShipViewCameras.Reset();
    ActiveShipViewCamera = nullptr;

    if (IsValid(FreeCameraPawn))
    {
        FreeCameraPawn->Destroy();
        FreeCameraPawn = nullptr;
    }
    SavedSpringArmRollInheritance.Reset();
    Super::EndPlay(EndPlayReason);
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
    InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ACustomPlayerController::ToggleMinimapInteractiveMode);
    InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ACustomPlayerController::ToggleMatchSetup).bExecuteWhenPaused=true;
}

void ACustomPlayerController::ToggleMinimapInteractiveMode()
{
    bMinimapInteractiveMode = !bMinimapInteractiveMode;

    TArray<UUserWidget*> Minimaps;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Minimaps, UShipMinimapWidget::StaticClass(), true);
    UShipMinimapWidget* Minimap = Minimaps.Num() > 0 ? Cast<UShipMinimapWidget>(Minimaps[0]) : nullptr;

    if (bMinimapInteractiveMode)
    {
        bShowMouseCursor = true;
        FInputModeGameAndUI Mode;
        if (Minimap)
        {
            Mode.SetWidgetToFocus(Minimap->TakeWidget());
        }
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        SetInputMode(Mode);

        if (Minimap)
        {
            Minimap->SetInteractiveMode(true);
        }
    }
    else
    {
        bShowMouseCursor = false;
        SetInputMode(FInputModeGameOnly());

        if (Minimap)
        {
            Minimap->SetInteractiveMode(false);
        }
    }
}

void ACustomPlayerController::ToggleMatchSetup()
{
    if(SetupWidget){CloseMatchSetup();return;}
    if(!IsLocalController())return;
    bMinimapInteractiveMode=false;
    SetupWidget=CreateWidget<UShipSetupWidget>(this,UShipSetupWidget::StaticClass());
    if(!SetupWidget)return;
    bMenuPausedWorld=!GetWorld()->IsPaused();
    SetPause(true);SetupWidget->AddToViewport(300);bShowMouseCursor=true;
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
    bMinimapInteractiveMode=false;
    bShowMouseCursor=false;SetInputMode(FInputModeGameOnly());
    if(bMenuPausedWorld)SetPause(false);bMenuPausedWorld=false;
    if(const auto* Battle=GetWorld()?GetWorld()->GetSubsystem<UNavalBattleSubsystem>():nullptr;Battle && !Battle->Outcome.IsEmpty())
        ShowMatchResult();
}

void ACustomPlayerController::StartNewMatch()
{
    if (!IsLocalController() || bTravelPending)
    {
        return;
    }

    // Keep the loading overlay visible while the level travels; the new
    // controller's BeginPlay shows it again until the battlefield is ready.
    // A requested restart must progress even if the result/setup was paused.
    SetPause(false);bMenuPausedWorld=false;
    bTravelPending = true;
    ShowLoadingScreen();
    GetWorld()->GetTimerManager().SetTimer(LoadingTravelTimer, this, &ACustomPlayerController::OpenMatchLevel, 0.25f, false);
}

void ACustomPlayerController::OpenMatchLevel()
{
    bTravelPending = false;
    UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Levels/MainMapLevel")));
}

void ACustomPlayerController::RestartMatch()
{
    if(auto* Settings=GetGameInstance()?GetGameInstance()->GetSubsystem<UNavalMatchSettings>():nullptr)
        if(const auto* Field=GetWorld()?GetWorld()->GetSubsystem<UNavalBattlefieldSubsystem>():nullptr; Field && Field->ActualSeed>0)
            Settings->BattlefieldSeed=Field->ActualSeed;
    StartNewMatch();
}

void ACustomPlayerController::ShowLoadingScreen()
{
    if (!IsLocalController() || IsValid(LoadingScreenUI))
    {
        return;
    }

    LoadingScreenUI = CreateWidget<ULoadingScreenWidget>(this, ULoadingScreenWidget::StaticClass());
    if (!IsValid(LoadingScreenUI))
    {
        return;
    }

    LoadingScreenUI->AddToViewport(1000);
    LoadingStartedAt=FPlatformTime::Seconds();
    HideLoadingWidgets(this);
    if (IsValid(TeamCountBarUI))
    {
        TeamCountBarUI->SetVisibility(ESlateVisibility::Collapsed);
    }
    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);
    bShowMouseCursor = false;
    SetInputMode(FInputModeUIOnly());

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(LoadingWatchTimer, this, &ACustomPlayerController::CheckLoadingComplete, 0.1f, true);
    }
}

void ACustomPlayerController::HideLoadingScreen()
{
    if (IsValid(LoadingScreenUI))
    {
        LoadingScreenUI->RemoveFromParent();
        LoadingScreenUI = nullptr;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LoadingWatchTimer);
    }

    RestoreLoadingWidgets();
    if (IsValid(TeamCountBarUI))
    {
        TeamCountBarUI->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    ResetIgnoreMoveInput();
    ResetIgnoreLookInput();
    bShowMouseCursor = false;
    SetInputMode(FInputModeGameOnly());
}

void ACustomPlayerController::CheckLoadingComplete()
{
    if (bTravelPending || !IsValid(LoadingScreenUI))
    {
        return;
    }

    // The authored minimap can be created after the loading overlay; keep it hidden
    // until the battlefield is ready.
    HideLoadingWidgets(this);
    if (IsValid(TeamCountBarUI))
    {
        TeamCountBarUI->SetVisibility(ESlateVisibility::Collapsed);
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        HideLoadingScreen();
        return;
    }

    auto* Battle = World->GetSubsystem<UNavalBattleSubsystem>();
    const auto* Field = World->GetSubsystem<UNavalBattlefieldSubsystem>();
    const bool bReady = Battle && Battle->bMatchConfigured && (!Field || Field->CanBeginCombat());
    bool bFailed = (Field && !Field->InitializationError.IsEmpty()) ||
        (Battle && !Battle->bMatchConfigured && !Battle->ConfigurationError.IsEmpty());
    if(!bReady && !bFailed && FPlatformTime::Seconds()-LoadingStartedAt>90)
    {
        if(Battle)Battle->ConfigurationError=TEXT("Match setup timed out. Check loaded ships, patrol markers and terrain, then start a new match.");
        bFailed=true;
    }
    if (bReady || bFailed)
    {
        HideLoadingScreen();
        // Reuse the teammate's menu and its existing error field. A preparation
        // failure must offer a correction/retry instead of hiding the only UI.
        if(bFailed && !SetupWidget)ToggleMatchSetup();
    }
}

void ACustomPlayerController::ReturnToMatchSetup()
{
    const auto* Battle=GetWorld()?GetWorld()->GetSubsystem<UNavalBattleSubsystem>():nullptr;
    if(!Battle || Battle->Outcome.IsEmpty() || SetupWidget)return;
    if(IsValid(MatchSummaryUI))
    {
        MatchSummaryUI->RemoveFromParent();
        MatchSummaryUI=nullptr;
    }
    ToggleMatchSetup();
}

void ACustomPlayerController::ShowMatchResult()
{
    if(!IsLocalController() || bTravelPending)return;
    // A very early result can precede the loading watcher's final tick. Cancel
    // that transition first so it cannot later replace the result's cursor/mode.
    if(IsValid(LoadingScreenUI))HideLoadingScreen();
    // The summary is its own full-screen widget so the live ship HUD stays independent.
    if(IsValid(MatchSummaryUI))
    {
        MatchSummaryUI->RemoveFromParent();
        MatchSummaryUI=nullptr;
    }
    MatchSummaryUI=CreateWidget<UMatchSummaryWidget>(this,UMatchSummaryWidget::StaticClass());
    if(IsValid(MatchSummaryUI))
    {
        MatchSummaryUI->AddToViewport(200);
    }
    bShowMouseCursor=true;
    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    SetInputMode(Mode);
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
void ACustomPlayerController::BeginFullAIDemo(APawn* InitialShip)
{
    bFullAISpectating=true;bDeathSpectating=false;OriginalPlayerShip=nullptr;
    FreeCameraPawn=Cast<AFreeCameraPawn>(GetPawn());
    ApplyShipView(InitialShip);
    // The authored minimap constructs before the sixth demo pawn is spawned.
    // Rebuild that same widget once from the now-complete roster, preserving
    // its projection, artwork and marker behavior instead of adding an overlay.
    TArray<UUserWidget*> Maps;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Maps,UShipMinimapWidget::StaticClass(),true);
    for(auto* Existing:Maps)if(auto* Replacement=CreateWidget<UUserWidget>(this,Existing->GetClass()))
    {Existing->RemoveFromParent();Replacement->AddToViewport();}
}

void ACustomPlayerController::NextTeammate()
{
    // Tab cycles the teammates of whichever team the camera is currently on, so
    // watching a red ship only ever steps through other red ships.
    const int32 Team=ResolveViewedTeam();

    APawn* NextShip=CycleShipView(Team);
    if(bDeathSpectating && IsValid(NextShip))
        FollowedTeammate=NextShip;

    if(!IsValid(NextShip))
    {
        // No living teammate is left on that team; fall back to the overview.
        TArray<APawn*> TeamShips;
        CollectShipPawns(GetWorld(),Team,TeamShips);
        if(TeamShips.IsEmpty())ShowBattlefieldOverview();
    }
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
    const FVector Eye=Focus+Up*50000-FVector::VectorPlaneProject(FVector(1,0,0),Up).GetSafeNormal()*65000;
    FreeCameraPawn->SetActorLocation(Eye,false,nullptr,ETeleportType::TeleportPhysics);
    FreeCameraPawn->SetHeightLimits(Eye.Z-50000,Eye.Z+50000);
    SetControlRotation((Focus-Eye).Rotation());FollowedTeammate=nullptr;
    SetViewTargetWithBlend(FreeCameraPawn,.25f);
    bBattlefieldOverview=true;
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
    if(IsValid(FreeCameraPawn) && !bFullAISpectating)ToggleFreeCamera();
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
    // Keep the live details panel from stealing focus while the summary is open.
    if(IsValid(MatchSummaryUI))
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
    if(bBattlefieldOverview)
    {
        bBattlefieldOverview=false;
        RestoreRecentShipView();
        return;
    }

    // Q switches sides: blue -> red, red -> blue. Tab still cycles teammates
    // inside the team that is currently being watched.
    if(APawn* NextShip=CycleOpposingTeam())
        if(bDeathSpectating)FollowedTeammate=NextShip;
}

APawn* ACustomPlayerController::ResolveViewedShip() const
{
    APawn* Ship = ViewedShip.Get();
    if (!IsValid(Ship))
    {
        Ship = IsValid(FreeCameraPawn) ? PreviousPawn.Get() : Cast<APawn>(GetViewTarget());
    }
    return Ship;
}

int32 ACustomPlayerController::ResolveViewedTeam() const
{
    if(const APawn* Ship=ResolveViewedShip())
        if(const auto* Weapon=Ship->FindComponentByClass<UShipWeaponComponent>())
            return Weapon->Team();
    const auto* Settings=GetGameInstance()?GetGameInstance()->GetSubsystem<UNavalMatchSettings>():nullptr;
    return Settings && Settings->bExplicitSelection?FMath::Clamp(Settings->PlayerTeam,0,1):0;
}

void ACustomPlayerController::FocusShip(APawn* InShip)
{
    if(!IsValid(InShip))return;
    if (IsValid(FreeCameraPawn) && !bFullAISpectating && !bDeathSpectating)
    {
        SavedFreeCameraLocation = FreeCameraPawn->GetActorLocation();
        SavedFreeCameraRotation = GetControlRotation();
        bHasSavedFreeCameraTransform = true;
        ToggleFreeCamera();
    }
    bBattlefieldOverview = false;
    ApplyShipView(InShip);
    SetShipViewTarget(InShip, 0.25f);
}

APawn* ACustomPlayerController::CycleOpposingTeam()
{
    UWorld* World = GetWorld();
    if (!World)return nullptr;

    const int32 TargetTeam = 1 - ResolveViewedTeam();
    TArray<APawn*> ShipPawns;
    CollectShipPawns(World, TargetTeam, ShipPawns);
    if (ShipPawns.Num() == 0)return nullptr;

    // Advance through the destination team on repeated Q presses so pressing Q
    // repeatedly still lets the player inspect every enemy ship.
    int32& NextIndex = NextTeamShipIndex[TargetTeam];
    if (!ShipPawns.IsValidIndex(NextIndex))NextIndex = 0;
    APawn* NextShip = ShipPawns[NextIndex];
    NextIndex = (NextIndex + 1) % ShipPawns.Num();

    FocusShip(NextShip);
    return NextShip;
}

APawn* ACustomPlayerController::CycleShipView(int32 TeamFilter)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    TArray<APawn*> ShipPawns;
    CollectShipPawns(World, TeamFilter, ShipPawns);
    if (ShipPawns.Num() == 0)
    {
        return nullptr;
    }

    // A camera change must not unpossess an independently controlled AI ship.
    APawn* CurrentShip = ResolveViewedShip();

    APawn* NextShip = nullptr;
    if (ShipPawns.Num() == 1)
    {
        // Only one candidate: there is no teammate to cycle to, but the camera
        // must still be pulled back onto that ship if it is somewhere else.
        NextShip = ShipPawns[0];
        if (!IsValid(FreeCameraPawn) && NextShip == CurrentShip)
        {
            return nullptr;
        }
    }
    else
    {
        const int32 CurrentIndex = ShipPawns.IndexOfByKey(CurrentShip);
        NextShip = ShipPawns[CurrentIndex == INDEX_NONE ? 0 : (CurrentIndex + 1) % ShipPawns.Num()];
    }

    if (!IsValid(NextShip) || NextShip == CurrentShip)
    {
        return nullptr;
    }

    FocusShip(NextShip);
    return NextShip;
}

void ACustomPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    const auto* Weapon = IsValid(OriginalPlayerShip) ? OriginalPlayerShip->FindComponentByClass<UShipWeaponComponent>() : nullptr;
    if (!bFullAISpectating && !bDeathSpectating && Weapon && Weapon->bIsDestroyed)EnterDeathSpectator();
    if(bFullAISpectating && IsValid(ViewedShip))
        if(const auto* W=ViewedShip->FindComponentByClass<UShipWeaponComponent>();W && W->bIsDestroyed)
        {ViewedShip=nullptr;ActiveShipViewCamera=nullptr;CycleShipView(AnyTeam);}
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
    if(bFullAISpectating){ShowBattlefieldOverview();return;}
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
