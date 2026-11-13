#include "UI/NavalBattleUI.h"
#include "Layout/Geometry.h"
#include "Input/Reply.h"
#include "Input/Events.h"
#include "AIController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Gameplay/CustomPlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/TargetPoint.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystems/NavalBattlefieldSubsystem.h"
#include "Combat/ShipCombatComponent.h"
#include "Navigation/ShipEnemyNavigationComponent.h"
#include "Ships/ShipMovementComponent.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace {
FLinearColor HPColor(float P) {
  return P > .6f   ? FLinearColor(.08f, .85f, .22f)
         : P > .3f ? FLinearColor(1, .78f, .03f)
                   : FLinearColor(.95f, .06f, .05f);
}
UTextBlock *Text(UWidgetTree *Tree, UVerticalBox *Box, int32 Size,
                 FLinearColor Color) {
  auto *T = Tree->ConstructWidget<UTextBlock>();
  FSlateFontInfo Font = T->GetFont();
  Font.Size = Size;
  T->SetFont(Font);
  T->SetColorAndOpacity(Color);
  auto *Slot = Box->AddChildToVerticalBox(T);
  Slot->SetPadding(FMargin(0, 3));
  return T;
}
UProgressBar *Progress(UWidgetTree *Tree, UVerticalBox *Box, float Height) {
  auto *Size = Tree->ConstructWidget<USizeBox>();
  Size->SetHeightOverride(Height);
  Box->AddChildToVerticalBox(Size);
  auto *Bar = Tree->ConstructWidget<UProgressBar>();
  Bar->SetPercent(1);
  Size->AddChild(Bar);
  return Bar;
}
AActor *MarkerShip(const UUserWidget *Marker) {
  if (!Marker)
    return nullptr;
  const FObjectPropertyBase *ShipActorProperty =
      FindFProperty<FObjectPropertyBase>(Marker->GetClass(), TEXT("ShipActor"));
  return ShipActorProperty
             ? Cast<AActor>(
                   ShipActorProperty->GetObjectPropertyValue_InContainer(
                       Marker))
             : nullptr;
}
UTexture2D *TeamMarkerIcon(const FString &TeamName,
                           const UShipWeaponComponent *Weapon) {
  bool bRed = TeamName.Equals(TEXT("Red"), ESearchCase::IgnoreCase) ||
              TeamName.Equals(TEXT("Enemy"), ESearchCase::IgnoreCase);
  if (!bRed && !TeamName.Equals(TEXT("Blue"), ESearchCase::IgnoreCase) &&
      Weapon)
    bRed = Weapon->Team() != 0;
  static TWeakObjectPtr<UTexture2D> BlueIcon, RedIcon;
  TWeakObjectPtr<UTexture2D> &Icon = bRed ? RedIcon : BlueIcon;
  if (!Icon.IsValid())
    Icon = LoadObject<UTexture2D>(
        nullptr, bRed ? TEXT("/Game/Game/UI/Minimap/Textures/ObjectTextures/"
                             "ShipMarkerIcon-2.ShipMarkerIcon-2")
                      : TEXT("/Game/Game/UI/Minimap/Textures/ObjectTextures/"
                             "ShipMarkerIcon-1.ShipMarkerIcon-1"));
  return Icon.Get();
}
} // namespace
void UShipMinimapWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if(UWorld* World=GetWorld())
        World->GetTimerManager().SetTimer(MarkerTimer,this,&UShipMinimapWidget::RemoveDestroyedMarkers,.1f,true);

    BorderWrap = GetWidgetFromName(TEXT("Border_Wrap_Minimap"));
    if (BorderWrap)
    {
        BorderWrap->SetClipping(EWidgetClipping::ClipToBounds);
        BorderWrap->SetVisibility(ESlateVisibility::Visible);
    }
    ContentOverlay = GetWidgetFromName(TEXT("Overlay_Minimap"));
    if (ContentOverlay)
    {
        ContentOverlay->SetRenderTransformPivot(FVector2D::ZeroVector);
    }
    ApplyMinimapTransform();
}
void UShipMinimapWidget::NativeDestruct()
{
    if(UWorld* World=GetWorld())World->GetTimerManager().ClearTimer(MarkerTimer);
    bIsDragging = false;
    Super::NativeDestruct();
}
void UShipMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (CurrentZoom > 1.0f)
    {
        UpdateMarkerScales();
    }
}
void UShipMinimapWidget::SetInteractiveMode(bool bActive)
{
    bInteractiveActive = bActive;
    if (!bActive)
    {
        bIsDragging = false;
    }
}
void UShipMinimapWidget::ToggleZoom()
{
    if (CurrentZoom > 1.1f)
    {
        CurrentZoom = 1.0f;
        PanOffset = FVector2D::ZeroVector;
    }
    else
    {
        CurrentZoom = 9.0f;
        FVector2D Center(195.0f, 182.5f);
        if (BorderWrap)
        {
            Center = BorderWrap->GetCachedGeometry().GetLocalSize() * 0.5f;
        }
        PanOffset = Center - Center * CurrentZoom;
    }
    ApplyMinimapTransform();
}
void UShipMinimapWidget::SetMinimapZoom(float InZoom)
{
    CurrentZoom = (InZoom >= 4.0f) ? 9.0f : 1.0f;
    if (CurrentZoom <= 1.0f)
    {
        PanOffset = FVector2D::ZeroVector;
    }
    ApplyMinimapTransform();
}
FReply UShipMinimapWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    const float WheelDelta = InMouseEvent.GetWheelDelta();
    if (FMath::IsNearlyZero(WheelDelta))
    {
        return FReply::Unhandled();
    }

    if (!BorderWrap)
    {
        BorderWrap = GetWidgetFromName(TEXT("Border_Wrap_Minimap"));
    }

    FVector2D LocalCursor(195.0f, 182.5f);
    if (BorderWrap)
    {
        const FGeometry& BorderGeom = BorderWrap->GetCachedGeometry();
        LocalCursor = BorderGeom.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        const FVector2D Size = BorderGeom.GetLocalSize();
        if (LocalCursor.X < 0.f || LocalCursor.X > Size.X || LocalCursor.Y < 0.f || LocalCursor.Y > Size.Y)
        {
            return FReply::Unhandled();
        }
    }

    const float MaxZoom = 9.0f;
    const float MinZoom = 1.0f;
    const float OldZoom = CurrentZoom;

    float NewZoom = OldZoom;
    if (WheelDelta > 0.0f)
    {
        // Tăng 20% mỗi nấc cuộn chuột lên
        NewZoom = FMath::Min(OldZoom * 1.2f, MaxZoom);
    }
    else
    {
        // Giảm 20% mỗi nấc cuộn chuột xuống
        NewZoom = OldZoom / 1.2f;
        if (NewZoom <= 1.05f)
        {
            NewZoom = MinZoom;
        }
    }

    if (FMath::IsNearlyEqual(NewZoom, OldZoom, 0.001f))
    {
        return FReply::Handled();
    }

    if (NewZoom <= 1.0f)
    {
        CurrentZoom = 1.0f;
        PanOffset = FVector2D::ZeroVector;
    }
    else
    {
        // Giữ nguyên điểm trên bản đồ ngay dưới con trỏ chuột
        const FVector2D MapPoint = (LocalCursor - PanOffset) / OldZoom;
        CurrentZoom = NewZoom;
        PanOffset = LocalCursor - MapPoint * CurrentZoom;
    }

    ApplyMinimapTransform();
    return FReply::Handled();
}
FReply UShipMinimapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        if (!BorderWrap)
        {
            BorderWrap = GetWidgetFromName(TEXT("Border_Wrap_Minimap"));
        }
        if (BorderWrap)
        {
            const FGeometry& BorderGeom = BorderWrap->GetCachedGeometry();
            const FVector2D LocalPos = BorderGeom.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
            const FVector2D LocalSize = BorderGeom.GetLocalSize();
            if (LocalPos.X < 0.f || LocalPos.X > LocalSize.X || LocalPos.Y < 0.f || LocalPos.Y > LocalSize.Y)
            {
                return FReply::Unhandled();
            }
        }

        if (CurrentZoom > 1.0f)
        {
            bIsDragging = true;
            DragStartMouse = InMouseEvent.GetScreenSpacePosition();
            DragStartPan = PanOffset;
            return FReply::Handled().CaptureMouse(TakeWidget());
        }
    }
    return FReply::Unhandled();
}
FReply UShipMinimapWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsDragging && CurrentZoom > 1.0f)
    {
        const FVector2D CurrentScreenMouse = InMouseEvent.GetScreenSpacePosition();
        const FVector2D Delta = CurrentScreenMouse - DragStartMouse;
        PanOffset = DragStartPan + Delta;
        ApplyMinimapTransform();
        return FReply::Handled();
    }
    return FReply::Unhandled();
}
FReply UShipMinimapWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsDragging && (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton))
    {
        bIsDragging = false;
        return FReply::Handled().ReleaseMouseCapture();
    }
    return FReply::Unhandled();
}
FReply UShipMinimapWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        if (!BorderWrap)
        {
            BorderWrap = GetWidgetFromName(TEXT("Border_Wrap_Minimap"));
        }
        if (BorderWrap)
        {
            const FGeometry& BorderGeom = BorderWrap->GetCachedGeometry();
            const FVector2D LocalPos = BorderGeom.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
            const FVector2D LocalSize = BorderGeom.GetLocalSize();
            if (LocalPos.X >= 0.f && LocalPos.X <= LocalSize.X && LocalPos.Y >= 0.f && LocalPos.Y <= LocalSize.Y)
            {
                if (CurrentZoom <= 1.1f)
                {
                    CurrentZoom = 9.0f;
                    PanOffset = LocalPos - LocalPos * CurrentZoom;
                }
                else
                {
                    CurrentZoom = 1.0f;
                    PanOffset = FVector2D::ZeroVector;
                }
                ApplyMinimapTransform();
                return FReply::Handled();
            }
        }
    }
    return FReply::Unhandled();
}
void UShipMinimapWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
    bIsDragging = false;
    Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}
void UShipMinimapWidget::ClampPanOffset()
{
    FVector2D MapSize(390.0f, 365.0f);
    if (const FStructProperty* SizeProp = FindFProperty<FStructProperty>(GetClass(), TEXT("MinimapSize")))
    {
        if (SizeProp->Struct == TBaseStructure<FVector2D>::Get())
        {
            MapSize = *SizeProp->ContainerPtrToValuePtr<FVector2D>(this);
        }
    }
    if (MapSize.X <= 0.0f || MapSize.Y <= 0.0f)
    {
        MapSize = FVector2D(390.0f, 365.0f);
    }

    if (CurrentZoom <= 1.0f)
    {
        PanOffset = FVector2D::ZeroVector;
        return;
    }

    const float MinX = -MapSize.X * (CurrentZoom - 1.0f);
    const float MinY = -MapSize.Y * (CurrentZoom - 1.0f);
    PanOffset.X = FMath::Clamp(PanOffset.X, MinX, 0.0f);
    PanOffset.Y = FMath::Clamp(PanOffset.Y, MinY, 0.0f);
}
void UShipMinimapWidget::ApplyMinimapTransform()
{
    if (!ContentOverlay)
    {
        ContentOverlay = GetWidgetFromName(TEXT("Overlay_Minimap"));
    }
    if (!BorderWrap)
    {
        BorderWrap = GetWidgetFromName(TEXT("Border_Wrap_Minimap"));
    }
    if (BorderWrap)
    {
        BorderWrap->SetClipping(EWidgetClipping::ClipToBounds);
        BorderWrap->SetVisibility(ESlateVisibility::Visible);
    }

    ClampPanOffset();

    if (ContentOverlay)
    {
        ContentOverlay->SetRenderTransformPivot(FVector2D::ZeroVector);
        ContentOverlay->SetRenderScale(FVector2D(CurrentZoom, CurrentZoom));
        ContentOverlay->SetRenderTranslation(PanOffset);
    }

    UpdateMarkerScales();
}
void UShipMinimapWidget::UpdateMarkerScales()
{
    auto* MarkersProperty = FindFProperty<FArrayProperty>(GetClass(), TEXT("ShipMarkerWidgets"));
    if (!MarkersProperty) return;
    auto* MarkerInner = CastField<FObjectPropertyBase>(MarkersProperty->Inner);
    if (!MarkerInner) return;

    FScriptArrayHelper Markers(MarkersProperty, MarkersProperty->ContainerPtrToValuePtr<void>(this));

    // Phóng to marker thêm ~30% (từ 1.0x ở zoom 1x lên tối đa 1.3x khi zoom vào)
    const float Alpha = FMath::Clamp((CurrentZoom - 1.0f) / 3.0f, 0.0f, 1.0f);
    const float TargetVisualScale = FMath::Lerp(1.0f, 1.3f, Alpha);
    const float MarkerScale = CurrentZoom > 0.001f ? (TargetVisualScale / CurrentZoom) : 1.0f;
    const FVector2D ScaleVec(MarkerScale, MarkerScale);
    const FVector2D Pivot(0.5f, 0.5f);

    for (int32 i = 0; i < Markers.Num(); ++i)
    {
        if (UUserWidget* Marker = Cast<UUserWidget>(MarkerInner->GetObjectPropertyValue(Markers.GetRawPtr(i))))
        {
            Marker->SetRenderTransformPivot(Pivot);
            Marker->SetRenderScale(ScaleVec);
        }
    }
}
void UShipMinimapWidget::RemoveDestroyedMarkers() {
  auto *ActorsProperty =
      FindFProperty<FArrayProperty>(GetClass(), TEXT("ShipActors"));
  auto *MarkersProperty =
      FindFProperty<FArrayProperty>(GetClass(), TEXT("ShipMarkerWidgets"));
  auto *SlotsProperty =
      FindFProperty<FArrayProperty>(GetClass(), TEXT("ShipMarkerSlots"));
  auto *MarkerInner =
      MarkersProperty ? CastField<FObjectPropertyBase>(MarkersProperty->Inner)
                      : nullptr;
  if (!ActorsProperty || !MarkersProperty || !SlotsProperty || !MarkerInner) {
    if (!bWarnedMissingArrays) {
      UE_LOG(LogTemp, Warning, TEXT("%s: minimap marker arrays not found."),
             *GetName());
      bWarnedMissingArrays = true;
    }
    return;
  }
  auto *ActorInner = CastField<FObjectPropertyBase>(ActorsProperty->Inner);
  FScriptArrayHelper Actors(ActorsProperty,
                            ActorsProperty->ContainerPtrToValuePtr<void>(this));
  FScriptArrayHelper Markers(
      MarkersProperty, MarkersProperty->ContainerPtrToValuePtr<void>(this));
  FScriptArrayHelper Slots(SlotsProperty,
                           SlotsProperty->ContainerPtrToValuePtr<void>(this));
  if (Actors.Num() != Markers.Num() || Actors.Num() != Slots.Num())
    return;
  for (int32 Index = Actors.Num() - 1; Index >= 0; --Index) {
    UUserWidget *Marker = Cast<UUserWidget>(
        MarkerInner->GetObjectPropertyValue(Markers.GetRawPtr(Index)));
    UObject *ShipObject =
        ActorInner ? ActorInner->GetObjectPropertyValue(Actors.GetRawPtr(Index))
                   : nullptr;
    AActor *Ship = Cast<AActor>(ShipObject);
    if (!Ship && ShipObject)
      if (auto *Component = Cast<UActorComponent>(ShipObject))
        Ship = Component->GetOwner();
    if (!IsValid(Ship))
      Ship = MarkerShip(Marker);
    const UShipWeaponComponent *Weapon =
        IsValid(Ship) ? Ship->FindComponentByClass<UShipWeaponComponent>()
                      : nullptr;
    if (!IsValid(Ship) || !Weapon || Weapon->bIsDestroyed ||
        Weapon->CurrentHealth <= 0.f) {
      if (IsValid(Marker))
        Marker->RemoveFromParent();
      Actors.RemoveValues(Index);
      Markers.RemoveValues(Index);
      Slots.RemoveValues(Index);
    }
  }
  if (CurrentZoom > 1.0f) {
    UpdateMarkerScales();
  }
}
void UShipMarkerWidget::NativeTick(const FGeometry &MyGeometry,
                                   float InDeltaTime) {
  Super::NativeTick(MyGeometry, InDeltaTime);
  if (!MarkerImage)
    MarkerImage = Cast<UImage>(GetWidgetFromName(TEXT("Image_ShipMarker")));
  const AActor *Ship = MarkerShip(this);
  const UShipWeaponComponent *Weapon =
      IsValid(Ship) ? Ship->FindComponentByClass<UShipWeaponComponent>()
                    : nullptr;
  if (!MarkerImage || !Weapon)
    return;
  if (UTexture2D *Icon = TeamMarkerIcon(Weapon->GetTeamName(), Weapon))
    if (MarkerImage->GetBrush().GetResourceObject() != Icon)
      MarkerImage->SetBrushFromTexture(Icon, false);
}
TSharedRef<SWidget> UShipHealthBarWidget::RebuildWidget() {
  if (!WidgetTree)
    WidgetTree = NewObject<UWidgetTree>(this);
  auto *Frame = WidgetTree->ConstructWidget<UBorder>();
  Frame->SetBrushColor(FLinearColor(.008f, .015f, .03f, .88f));
  Frame->SetPadding(FMargin(8, 3));
  WidgetTree->RootWidget = Frame;
  auto *Box = WidgetTree->ConstructWidget<UVerticalBox>();
  Frame->AddChild(Box);
  Label = Text(WidgetTree, Box, 13, FLinearColor::White);
  Bar = Progress(WidgetTree, Box, 10);
  SetVisibility(ESlateVisibility::HitTestInvisible);
  return Super::RebuildWidget();
}
void UShipHealthBarWidget::Refresh(UShipWeaponComponent *Ship) {
  BoundShip = Ship;
  if (!Ship || !Bar)
    return;
  DisplayedFraction = FMath::Clamp(
      Ship->CurrentHealth / FMath::Max(Ship->MaxHealth, 1.f), 0.f, 1.f);
  HealthColor = HPColor(DisplayedFraction);
  Bar->SetPercent(DisplayedFraction);
  Bar->SetFillColorAndOpacity(HealthColor);
  Label->SetText(FText::FromString(FString::Printf(
      TEXT("%s  %.0f / %.0f"), Ship->Team() == 0 ? TEXT("BLUE") : TEXT("RED"),
      Ship->CurrentHealth, Ship->MaxHealth)));
  Label->SetColorAndOpacity(Ship->Team() == 0 ? FLinearColor(.15f, .55f, 1)
                                              : FLinearColor(1, .18f, .18f));
  SetVisibility(Ship->bIsDestroyed ? ESlateVisibility::Collapsed
                                   : ESlateVisibility::HitTestInvisible);
}
void UShipHealthBarWidget::NativeConstruct() {
  Super::NativeConstruct();
  if (BoundShip)
    Refresh(BoundShip);
}
bool UNavalBattleSubsystem::DoesSupportWorldType(EWorldType::Type Type) const {
  return Type == EWorldType::Game || Type == EWorldType::PIE;
}
void UNavalBattleSubsystem::Deinitialize() {
  GetWorld()->GetTimerManager().ClearTimer(MatchTimer);
  TurnRoster.Empty();
  ActiveTurnShip = nullptr;
  PlayerShip = nullptr;
  DemoCommandShip = nullptr;
  Registered.Empty();
  Fallen.Empty();
  Super::Deinitialize();
}
void UNavalBattleSubsystem::RegisterShip(UShipWeaponComponent *Ship) {
  if (Registered.Contains(Ship))
    return;
  Registered.Add(Ship);
  if (Ship->Team() == 0)
    ++BlueAlive;
  else if (Ship->Team() == 1)
    ++RedAlive;
}

void UNavalBattleSubsystem::OnWorldBeginPlay(UWorld &World) {
  Super::OnWorldBeginPlay(World);
  World.GetTimerManager().SetTimer(
      MatchTimer, this, &UNavalBattleSubsystem::UpdateMatch, .1f, true);
}

bool UNavalBattleSubsystem::ConfigureRoster() {
  auto *PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
  auto *Settings =
      GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>();
  const bool FullAI = Settings && Settings->bFullAIDemo;
  ATargetPoint *AuthoredPlayerA = nullptr;
  ATargetPoint *AuthoredPlayerB = nullptr;
  if (FullAI && !Settings->bUseSectorBattlefield) {
    int32 CountA = 0, CountB = 0;
    for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It) {
      if (It->ActorHasTag(TEXT("Naval.PlayerPatrol.A"))) {
        AuthoredPlayerA = *It;
        ++CountA;
      }
      if (It->ActorHasTag(TEXT("Naval.PlayerPatrol.B"))) {
        AuthoredPlayerB = *It;
        ++CountB;
      }
    }
    if (CountA != 1 || CountB != 1 || AuthoredPlayerA == AuthoredPlayerB) {
      ConfigurationError =
          FString::Printf(TEXT("MY MAP / Full AI: tag exactly one TargetPoint "
                               "Naval.PlayerPatrol.A and one different "
                               "TargetPoint Naval.PlayerPatrol.B (found A=%d, "
                               "B=%d). Fix the Actor Tags before starting."),
                          CountA, CountB);
      return false;
    }
  }
  if (FullAI && !IsValid(DemoCommandShip)) {
    // Reuse the production AI Blueprint and its original physics graph. The
    // normal player Blueprint remains unchanged; no AI ever loses possession.
    const bool Small = Settings->HullClasses.Num() == 6 &&
                       Settings->HullClasses[0] == EShipHullClass::Small;
    UClass *Class = LoadClass<APawn>(
        nullptr,
        Small
            ? TEXT("/Game/ShipSmall/BluePrint/BP_ShipSmallAI.BP_ShipSmallAI_C")
            : TEXT("/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy_C"));
    FActorSpawnParameters Params;
    Params.Name = TEXT("Demo_Blue_Flagship");
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FTransform Spawn = PC && PC->GetPawn()
                                 ? PC->GetPawn()->GetActorTransform()
                                 : FTransform::Identity;
    DemoCommandShip =
        Class ? GetWorld()->SpawnActor<APawn>(Class, Spawn, Params) : nullptr;
    if (!DemoCommandShip) {
      ConfigurationError = TEXT("Full AI: production AI ship could not spawn");
      return false;
    }
    if (auto *C = DemoCommandShip->FindComponentByClass<UShipCombatComponent>())
      C->Faction = 0;
    if (auto *W = DemoCommandShip->FindComponentByClass<UShipWeaponComponent>())
      W->Faction = 0;
    if (auto *N = DemoCommandShip
                      ->FindComponentByClass<UShipEnemyNavigationComponent>()) {
      if (Settings->bUseSectorBattlefield) {
        N->PatrolActorA = GetWorld()->SpawnActor<ATargetPoint>();
        N->PatrolActorB = GetWorld()->SpawnActor<ATargetPoint>();
      } else {
        // The unique authored pair was validated before spawning, so a
        // failed setup cannot leave a sixth hull that bypasses validation.
        N->PatrolActorA = AuthoredPlayerA;
        N->PatrolActorB = AuthoredPlayerB;
      }
      N->bNewNavigationEnabled = true;
      N->Role = EShipEnemyRole::Fighter;
    }
    DemoCommandShip->SpawnDefaultController();
    // The placed-ship Blueprint has no spawned-pawn controller class.
    // Give the sixth hull the same native controller used by placed AI.
    if (!DemoCommandShip->GetController())
      if (auto *AI = GetWorld()->SpawnActor<AAIController>())
        AI->Possess(DemoCommandShip);
    if (!DemoCommandShip->GetController()) {
      ConfigurationError = TEXT("Full AI: sixth ship has no AI controller");
      return false;
    }
  }
  APawn *Player =
      FullAI ? DemoCommandShip.Get() : (PC ? PC->GetPawn().Get() : nullptr);
  if (!Player || !Player->FindComponentByClass<UShipWeaponComponent>())
    return false;
  TArray<APawn *> Teams[2];
  for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    if (auto *W = It->FindComponentByClass<UShipWeaponComponent>();
        W && W->Team() >= 0 && W->Team() < 2)
      Teams[W->Team()].Add(*It);
  // Wait for the existing always-loaded five AI, never spawn replacements.
  if (Teams[0].Num() != 3 || Teams[1].Num() != 3)
    return false;
  for (auto &Team : Teams)
    Team.Sort([](const APawn &A, const APawn &B) {
      return A.GetFName().LexicalLess(B.GetFName());
    });
  const bool Selected = Settings && Settings->bExplicitSelection;
  const int32 PlayerTeam =
      Selected && !FullAI ? FMath::Clamp(Settings->PlayerTeam, 0, 1) : 0;
  auto *PlayerWeapon = Player->FindComponentByClass<UShipWeaponComponent>();
  const int32 OldPlayerTeam = PlayerWeapon->Team();
  if (OldPlayerTeam != PlayerTeam) {
    // Swap poses only for generated placement. An authored map keeps every
    // placed pose/PlayerStart; faction changes still follow the selected team.
    APawn *Other = Teams[PlayerTeam][0];
    const FTransform PlayerTransform = Player->GetActorTransform();
    if (Settings->bUseSectorBattlefield) {
      Player->SetActorTransform(Other->GetActorTransform(), false, nullptr,
                                ETeleportType::TeleportPhysics);
      Other->SetActorTransform(PlayerTransform, false, nullptr,
                               ETeleportType::TeleportPhysics);
    }
    if (auto *Combat = Player->FindComponentByClass<UShipCombatComponent>())
      Combat->Faction = PlayerTeam;
    if (auto *Combat = Other->FindComponentByClass<UShipCombatComponent>())
      Combat->Faction = 1 - PlayerTeam;
    PlayerWeapon->Faction = PlayerTeam;
    if (auto *OtherWeapon = Other->FindComponentByClass<UShipWeaponComponent>())
      OtherWeapon->Faction = 1 - PlayerTeam;
    Teams[PlayerTeam].Remove(Other);
    Teams[PlayerTeam].Add(Player);
    Teams[OldPlayerTeam].Remove(Player);
    Teams[OldPlayerTeam].Add(Other);
    for (auto &Team : Teams)
      Team.Sort([](const APawn &A, const APawn &B) {
        return A.GetFName().LexicalLess(B.GetFName());
      });
  }
  Teams[PlayerTeam].Remove(Player);
  Teams[PlayerTeam].Insert(Player, 0);
  TurnRoster.Empty();
  TurnLabels.Empty();
  Registered.Empty();
  Fallen.Empty();
  BlueAlive = 0;
  RedAlive = 0;
  Outcome.Empty();
  ScoreEntries.Reset();
  ScoreIndexByShip.Reset();
  for (int32 Team = 0; Team < 2; ++Team)
    for (int32 Slot = 0; Slot < 3; ++Slot) {
      APawn *Pawn = Teams[Team][Slot];
      auto *W = Pawn->FindComponentByClass<UShipWeaponComponent>();
      const int32 Count =
          Selected ? (Team == 0 ? Settings->BlueCount : Settings->RedCount) : 3;
      if (Slot >= Count) {
        AController *Controller = Pawn->GetController();
        Pawn->DetachFromControllerPendingDestroy();
        if (IsValid(Controller) && !Controller->IsPlayerController())
          Controller->Destroy();
        Pawn->Destroy();
        continue;
      }
      if (Settings) {
        if (auto *Move = Pawn->FindComponentByClass<UShipMovementComponent>())
          Move->ApplyShipClass(Settings->HullClasses[Team * 3 + Slot]);
        if (auto *Nav =
                Pawn->FindComponentByClass<UShipEnemyNavigationComponent>())
          Nav->bKeepAuthoredPatrol = !Settings->bUseSectorBattlefield;
        if (auto *Combat = Pawn->FindComponentByClass<UShipCombatComponent>())
          Combat->SetShootingSkill(Settings->Difficulty);
        if (Settings->bStandardDemoBalance) {
          W->MaxHealth = W->CurrentHealth = 100;
          W->BaseProjectileDamage = 10;
          if (auto *N =
                  Pawn->FindComponentByClass<UShipEnemyNavigationComponent>()) {
            N->MaxHealth = N->Health = 100;
          }
        }
      }
      // Name the ship from the hull class the player selected for this slot.
      // Naming lives here, not in the battlefield placement, because this is
      // the same slot index that applied ApplyShipClass above. The human
      // player keeps the "Player Ship" fallback from GetDisplayName().
      if (Settings && Settings->bUseIslandPatrolCircuit &&
          !Pawn->IsPlayerControlled())
        if (const auto *Field =
                GetWorld()->GetSubsystem<UNavalBattlefieldSubsystem>();
            Field && Field->AreTacticsEnabled())
          if (auto *Identity =
                  Pawn->FindComponentByClass<UShipIdentityComponent>();
              Identity && Identity->ShipName.IsEmpty()) {
            const int32 ClassIndex = Team * 3 + Slot;
            const EShipHullClass Hull =
                Settings->HullClasses.IsValidIndex(ClassIndex)
                    ? Settings->HullClasses[ClassIndex]
                    : EShipHullClass::Medium;
            const TCHAR *Role = Hull == EShipHullClass::Small ? TEXT("Scout")
                                : Hull == EShipHullClass::Large
                                    ? TEXT("Escort")
                                    : TEXT("Defender");
            Identity->ShipName = FString::Printf(
                TEXT("%s %s %d"), Team == 0 ? TEXT("Blue") : TEXT("Red"), Role,
                Slot + 1);
          }
      if (auto *Identity = Pawn->FindComponentByClass<UShipIdentityComponent>())
        Identity->TeamName = Team == 0 ? TEXT("Blue") : TEXT("Red");
      W->RefreshShipTags();
      TurnRoster.Add(W);
      Registered.Add(W);
      const int32 ScoreIndex = ScoreEntries.AddDefaulted();
      FNavalShipScore &Score = ScoreEntries[ScoreIndex];
      Score.ShipName = W->GetShipName();
      Score.Team = Team;
      ScoreIndexByShip.Add(W, ScoreIndex);
      TurnLabels.Add(FString::Printf(
          TEXT("%s %d (%s)"), Team == 0 ? TEXT("BLUE") : TEXT("RED"), Slot + 1,
          !FullAI && Team == PlayerTeam && Slot == 0 ? TEXT("YOU")
                                                     : TEXT("AI")));
      if (W->bIsDestroyed)
        Fallen.Add(W);
      else if (Team == 0)
        ++BlueAlive;
      else
        ++RedAlive;
    }
  PlayerShip = Player->FindComponentByClass<UShipWeaponComponent>();
  bMatchConfigured = true;
  if (FullAI)
    if (auto *Custom = Cast<ACustomPlayerController>(PC))
      Custom->BeginFullAIDemo(Player);
  if (auto *Field = GetWorld()->GetSubsystem<UNavalBattlefieldSubsystem>()) {
    TArray<APawn *> Fleet;
    for (const auto &Item : TurnRoster)
      if (auto *W = Item.Get())
        if (auto *P = Cast<APawn>(W->GetOwner()))
          Fleet.Add(P);
    Field->InitializeFleet(Fleet);
  }
  SetBattleMode(Settings ? Settings->Mode : ENavalBattleMode::RealTime);
  return true;
}

void UNavalBattleSubsystem::SetBattleMode(ENavalBattleMode Mode) {
  BattleMode = Mode;
  ActiveTurnShip = nullptr;
  ActiveTurnLabel.Empty();
  TurnNumber = 0;
  NextTeam = 0;
  TeamCursor[0] = TeamCursor[1] = 0;
  if (bMatchConfigured && BattleMode == ENavalBattleMode::TurnBased &&
      Outcome.IsEmpty())
    AdvanceTurn();
}

float UNavalBattleSubsystem::TurnSecondsRemaining() const {
  return ActiveTurnShip
             ? FMath::Max(0., TurnEndsAt - GetWorld()->GetTimeSeconds())
             : 0.f;
}

bool UNavalBattleSubsystem::IsShipActive(AActor *Ship) const {
  if (!IsValid(Ship) || !Outcome.IsEmpty())
    return false;
  if (const auto *Field =
          GetWorld()->GetSubsystem<UNavalBattlefieldSubsystem>();
      Field && !Field->CanBeginCombat())
    return false;
  if (!bMatchConfigured) {
    if (!ConfigurationError.IsEmpty())
      return false;
    const auto *Settings =
        GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>();
    return !Settings || Settings->Mode == ENavalBattleMode::RealTime;
  }
  return BattleMode == ENavalBattleMode::RealTime || ActiveTurnShip == Ship;
}

bool UNavalBattleSubsystem::CanShipAct(AActor *Ship) {
  if (!IsValid(Ship) || !Ship->GetWorld())
    return false;
  const auto *Battle = Ship->GetWorld()->GetSubsystem<UNavalBattleSubsystem>();
  return !Battle || Battle->IsShipActive(Ship);
}

void UNavalBattleSubsystem::AdvanceTurn() {
  ActiveTurnShip = nullptr;
  ActiveTurnLabel.Empty();
  if (!Outcome.IsEmpty())
    return;
  for (int32 Attempt = 0; Attempt < 2; ++Attempt) {
    const int32 Team = NextTeam;
    NextTeam = 1 - NextTeam;
    for (int32 I = 0; I < TurnRoster.Num(); ++I) {
      const int32 Index = TeamCursor[Team] % TurnRoster.Num();
      TeamCursor[Team] = (Index + 1) % TurnRoster.Num();
      auto *W = TurnRoster[Index].Get();
      if (!W || W->Team() != Team || !IsValid(W->GetOwner()) || W->bIsDestroyed)
        continue;
      ActiveTurnShip = W->GetOwner();
      ActiveTurnLabel = TurnLabels[Index];
      TurnEndsAt = GetWorld()->GetTimeSeconds() + 10.;
      ++TurnNumber;
      return;
    }
  }
}

void UNavalBattleSubsystem::UpdateMatch() {
  if (!bMatchConfigured) {
    ConfigureRoster();
    return;
  }
  if (const auto *Field =
          GetWorld()->GetSubsystem<UNavalBattlefieldSubsystem>();
      Field && !Field->CanBeginCombat()) {
    // Preparation does not spend a ship's first turn. Physics still ticks.
    if (BattleMode == ENavalBattleMode::TurnBased && ActiveTurnShip)
      TurnEndsAt = GetWorld()->GetTimeSeconds() + 10;
    return;
  }
  if (BattleMode == ENavalBattleMode::TurnBased) {
    const auto *W =
        IsValid(ActiveTurnShip)
            ? ActiveTurnShip->FindComponentByClass<UShipWeaponComponent>()
            : nullptr;
    if (!Outcome.IsEmpty())
      ActiveTurnShip = nullptr;
    else if (!W || W->bIsDestroyed ||
             GetWorld()->GetTimeSeconds() >= TurnEndsAt)
      AdvanceTurn();
  }
}
void UNavalBattleSubsystem::RecordDamage(UShipWeaponComponent *Source,
                                         UShipWeaponComponent *Victim,
                                         float Damage) {
  if (Damage <= 0)
    return;
  if (Source && Source != Victim)
    if (const int32 *Index = ScoreIndexByShip.Find(Source))
      ScoreEntries[*Index].DamageDealt += Damage;
  if (Victim)
    if (const int32 *Index = ScoreIndexByShip.Find(Victim))
      ScoreEntries[*Index].DamageTaken += Damage;
}

void UNavalBattleSubsystem::RecordElimination(UShipWeaponComponent *Killer) {
  if (!Killer)
    return;
  if (const int32 *Index = ScoreIndexByShip.Find(Killer))
    ++ScoreEntries[*Index].Eliminations;
}

void UNavalBattleSubsystem::ShipDestroyed(UShipWeaponComponent *Ship) {
  if (Fallen.Contains(Ship) || !Registered.Contains(Ship))
    return;
  const bool bHadOutcome = !Outcome.IsEmpty();
  Fallen.Add(Ship);
  if (Ship->Team() == 0)
    BlueAlive = FMath::Max(0, BlueAlive - 1);
  else if (Ship->Team() == 1)
    RedAlive = FMath::Max(0, RedAlive - 1);
  const auto *Settings =
      GetWorld()->GetGameInstance()
          ? GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>()
          : nullptr;
  const int32 PlayerTeam = Settings && Settings->bExplicitSelection
                               ? FMath::Clamp(Settings->PlayerTeam, 0, 1)
                               : 0;
  if (Settings && Settings->bFullAIDemo) {
    if (RedAlive == 0)
      Outcome = TEXT("BLUE WINS");
    else if (BlueAlive == 0)
      Outcome = TEXT("RED WINS");
  } else if ((PlayerTeam == 0 ? RedAlive : BlueAlive) == 0)
    Outcome = TEXT("VICTORY");
  else if ((PlayerTeam == 0 ? BlueAlive : RedAlive) == 0)
    Outcome = TEXT("DEFEAT");
  if (!bHadOutcome && !Outcome.IsEmpty())
    if (auto *PC = Cast<ACustomPlayerController>(
            UGameplayStatics::GetPlayerController(GetWorld(), 0)))
      PC->ShowMatchResult();
}
