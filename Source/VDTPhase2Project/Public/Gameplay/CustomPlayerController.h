#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/PlayerController.h"
#include "CustomPlayerController.generated.h"

class ACameraActor;
class AFreeCameraPawn;
class UButtonTutorialsUI;
class ULoadingScreenWidget;
class UMatchSummaryWidget;
class UShipDetailsUI;
class UTeamCountBarWidget;
class UKillFeedWidget;
class USpringArmComponent;
class UShipSetupWidget;

/** Views ships with Q without taking their AI controllers; F toggles free
 * flight. */
UCLASS()
class VDTPHASE2PROJECT_API ACustomPlayerController : public APlayerController {
  GENERATED_BODY()

public:
  virtual void BeginPlay() override;
  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
  virtual void PlayerTick(float DeltaTime) override;
  virtual void SetupInputComponent() override;
  UFUNCTION(BlueprintCallable, Category = "Match") void ToggleMatchSetup();
  UFUNCTION(BlueprintCallable, Category = "Match") void CloseMatchSetup();
  UFUNCTION(BlueprintCallable, Category = "Match") void ReturnToMatchSetup();
  /** Shows the loading screen, then travels to the demo level. */
  void StartNewMatch();
  /** Reloads the level with the same settings and the exact same battlefield
   * seed. */
  void RestartMatch();
  void ShowMatchResult();
  UPROPERTY(BlueprintReadOnly, Category = "Match")
  TObjectPtr<UShipSetupWidget> SetupWidget;
  virtual void OnPossess(APawn *InPawn) override;
  UFUNCTION(BlueprintCallable, Category = "Spectator") void NextTeammate();
  UFUNCTION(BlueprintCallable, Category = "Spectator")
  void ShowBattlefieldOverview();
  void BeginFullAIDemo(APawn *InitialShip);
  UPROPERTY(BlueprintReadOnly, Category = "Spectator")
  bool bFullAISpectating = false;
  UPROPERTY(BlueprintReadOnly, Category = "Spectator")
  bool bDeathSpectating = false;
  UPROPERTY(BlueprintReadOnly, Category = "Spectator")
  TObjectPtr<APawn> FollowedTeammate;
  UFUNCTION(BlueprintCallable, Category = "Camera")

  /** Returns the ship selected by the controller, including while free camera
   * is active. */
  APawn *GetSelectedShip() const;

private:
  void ToggleFreeCamera();
  void ToggleBattlefieldOverview();
  void RestoreRecentShipView();
  UFUNCTION(BlueprintCallable, Category = "Camera")
  void CycleShipCamera();

  /**
   * Moves the view onto the next ship matching TeamFilter (INDEX_NONE = any
   * team). Returns the ship the camera was moved to, or nullptr when nothing
   * changed.
   */
  APawn *CycleShipView(int32 TeamFilter);
  /** Ship the camera is currently following, falling back to the
   * viewed/previous pawn. */
  APawn *ResolveViewedShip() const;
  /** Team of the followed ship, or the player's team when no ship is followed.
   */
  int32 ResolveViewedTeam() const;
  /** Q: jumps to the opposing team and advances through that team on repeat
   * presses. */
  APawn *CycleOpposingTeam();
  /** Shared view switch used by both Q (team switch) and Tab (teammate cycle).
   */
  void FocusShip(APawn *InShip);
  void ToggleShipDetailsUI();
  void ToggleButtonTutorials();
  void ShowLoadingScreen();
  void HideLoadingScreen();
  void CheckLoadingComplete();
  void OpenMatchLevel();

  /** Makes the viewed ship the selection used by the camera and the details
   * panel. */
  void ApplyShipView(APawn *InShip);

  /** Uses an orbit camera for AI ships so the player can look around without
   * possessing them. */
  void SetShipViewTarget(APawn *InShip, float BlendTime);
  void UpdateShipViewCamera(ACameraActor *Camera);
  ACameraActor *GetNextShipViewCamera();
  ACameraActor *CreateShipViewCamera();

  /**
   * AI controllers keep the hull's roll in their control rotation, and the
   * ship's spring arms inherit it, which tilts the view. Level only the arms of
   * the ship being observed, then restore their authored values when the player
   * looks away.
   */
  void SetHorizonLock(APawn *InShip, bool bEnabled);

private:
  void EnterDeathSpectator();
  bool bMenuPausedWorld = false;
  bool bBattlefieldOverview = false;
  UPROPERTY(Transient) TObjectPtr<APawn> OriginalPlayerShip;
  UPROPERTY()
  TObjectPtr<APawn> PreviousPawn;

  /** Ship the player is currently watching with Q (not necessarily the
   * possessed pawn). */
  UPROPERTY()
  TObjectPtr<APawn> ViewedShip;

  /**
   * Orbit cameras alternate as view targets. Reusing one camera for every
   * AI ship makes SetViewTargetWithBlend a no-op, so enemy-to-enemy switches
   * would cut instead of blending. Three cameras also let a rapid Q press
   * replace a blend that is still running.
   */
  UPROPERTY()
  TArray<TObjectPtr<ACameraActor>> ShipViewCameras;

  /** Incoming camera while a blend is running; the outgoing camera must stay
   * put. */
  UPROPERTY()
  TObjectPtr<ACameraActor> ActiveShipViewCamera;

  TMap<TWeakObjectPtr<USpringArmComponent>, bool> SavedSpringArmRollInheritance;

  UPROPERTY()
  TObjectPtr<AFreeCameraPawn> FreeCameraPawn;

  UPROPERTY(Transient)
  TObjectPtr<UShipDetailsUI> ShipDetailsUI;

  UPROPERTY(Transient)
  TObjectPtr<UTeamCountBarWidget> TeamCountBarUI;

  UPROPERTY(Transient)
  TObjectPtr<UKillFeedWidget> KillFeedUI;

  UPROPERTY(Transient)
  TObjectPtr<UButtonTutorialsUI> ButtonTutorialsUI;

  UPROPERTY(Transient)
  TObjectPtr<ULoadingScreenWidget> LoadingScreenUI;

  UPROPERTY(Transient)
  TObjectPtr<UMatchSummaryWidget> MatchSummaryUI;

  FTimerHandle LoadingWatchTimer;
  FTimerHandle LoadingTravelTimer;
  bool bTravelPending = false;
  double LoadingStartedAt = 0;

  /** Next ship to pick per team on successive Q presses (0 = Blue, 1 = Red). */
  int32 NextTeamShipIndex[2] = {0, 0};

  // The normal pawn's camera rotation must survive a round trip through free
  // camera.
  FRotator PreviousControlRotation = FRotator::ZeroRotator;

  // Free-camera state survives toggling back to the normal pawn during this
  // play session.
  bool bHasSavedFreeCameraTransform = false;
  FVector SavedFreeCameraLocation = FVector::ZeroVector;
  FRotator SavedFreeCameraRotation = FRotator::ZeroRotator;
  float SavedFreeCameraMinimumZ = 0.0f;
  float SavedFreeCameraMaximumZ = 0.0f;

  /** The camera cannot rise more than 50 m above the location where it was
   * activated. */
  UPROPERTY(EditDefaultsOnly, Category = "Free Camera",
            meta = (ClampMin = "0.0"))
  float MaximumHeightAboveActivation = 50000.0f;

  /** Allows the player to descend slightly, without seeing underneath the
   * mapped area. */
  UPROPERTY(EditDefaultsOnly, Category = "Free Camera",
            meta = (ClampMin = "0.0"))
  float MaximumDepthBelowActivation = 250.0f;
};
