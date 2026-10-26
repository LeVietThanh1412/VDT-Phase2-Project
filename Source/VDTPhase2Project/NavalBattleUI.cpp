#include "NavalBattleUI.h"
#include "CustomPlayerController.h"
#include "ShipMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "ShipCombatComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace
{
    FLinearColor HPColor(float P) { return P > .6f ? FLinearColor(.08f, .85f, .22f) : P > .3f ? FLinearColor(1, .78f, .03f)
                                                                                              : FLinearColor(.95f, .06f, .05f); }
    UTextBlock *Text(UWidgetTree *Tree, UVerticalBox *Box, int32 Size, FLinearColor Color)
    {
        auto *T = Tree->ConstructWidget<UTextBlock>();
        FSlateFontInfo Font = T->GetFont();
        Font.Size = Size;
        T->SetFont(Font);
        T->SetColorAndOpacity(Color);
        auto *Slot = Box->AddChildToVerticalBox(T);
        Slot->SetPadding(FMargin(0, 3));
        return T;
    }
    UProgressBar *Progress(UWidgetTree *Tree, UVerticalBox *Box, float Height)
    {
        auto *Size = Tree->ConstructWidget<USizeBox>();
        Size->SetHeightOverride(Height);
        Box->AddChildToVerticalBox(Size);
        auto *Bar = Tree->ConstructWidget<UProgressBar>();
        Bar->SetPercent(1);
        Size->AddChild(Bar);
        return Bar;
    }
}
void UShipMinimapWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshNavalMarkers();
    GetWorld()->GetTimerManager().SetTimer(MarkerTimer, this, &UShipMinimapWidget::RefreshNavalMarkers, .1f, true);
}
void UShipMinimapWidget::NativeDestruct()
{
    if (GetWorld())
        GetWorld()->GetTimerManager().ClearTimer(MarkerTimer);
    Super::NativeDestruct();
}
void UShipMinimapWidget::RefreshNavalMarkers()
{
    // Keep the teammate's geographic projection, texture, canvas and update timer.
    // These three parallel arrays are authored by WBP_MainMinimap's Construct graph.
    auto *ActorsProperty = FindFProperty<FArrayProperty>(GetClass(), TEXT("ShipActors"));
    auto *MarkersProperty = FindFProperty<FArrayProperty>(GetClass(), TEXT("ShipMarkerWidgets"));
    auto *SlotsProperty = FindFProperty<FArrayProperty>(GetClass(), TEXT("ShipMarkerSlots"));
    if (!ActorsProperty || !MarkersProperty || !SlotsProperty)
        return;
    auto *ActorInner = CastField<FObjectPropertyBase>(ActorsProperty->Inner);
    auto *MarkerInner = CastField<FObjectPropertyBase>(MarkersProperty->Inner);
    if (!ActorInner || !MarkerInner)
        return;
    FScriptArrayHelper Actors(ActorsProperty, ActorsProperty->ContainerPtrToValuePtr<void>(this));
    FScriptArrayHelper Markers(MarkersProperty, MarkersProperty->ContainerPtrToValuePtr<void>(this));
    FScriptArrayHelper Slots(SlotsProperty, SlotsProperty->ContainerPtrToValuePtr<void>(this));
    if (Actors.Num() != Markers.Num() || Actors.Num() != Slots.Num())
        return;
    for (int32 Index = Actors.Num() - 1; Index >= 0; --Index)
    {
        AActor *Ship = Cast<AActor>(ActorInner->GetObjectPropertyValue(Actors.GetRawPtr(Index)));
        UUserWidget *Marker = Cast<UUserWidget>(MarkerInner->GetObjectPropertyValue(Markers.GetRawPtr(Index)));
        const UShipWeaponComponent *Weapon = IsValid(Ship) ? Ship->FindComponentByClass<UShipWeaponComponent>() : nullptr;
        if (!Weapon || Weapon->bIsDestroyed)
        {
            if (IsValid(Marker))
                Marker->RemoveFromParent();
            Actors.RemoveValues(Index);
            Markers.RemoveValues(Index);
            Slots.RemoveValues(Index);
        }
        else if (IsValid(Marker))
        {
            Marker->SetColorAndOpacity(Weapon->Team() == 0 ? FLinearColor(.15f, .55f, 1) : FLinearColor(1, .18f, .18f));
        }
    }
}
TSharedRef<SWidget> UShipHealthBarWidget::RebuildWidget()
{
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
void UShipHealthBarWidget::Refresh(UShipWeaponComponent *Ship)
{
    BoundShip = Ship;
    if (!Ship || !Bar)
        return;
    DisplayedFraction = FMath::Clamp(Ship->CurrentHealth / FMath::Max(Ship->MaxHealth, 1.f), 0.f, 1.f);
    HealthColor = HPColor(DisplayedFraction);
    Bar->SetPercent(DisplayedFraction);
    Bar->SetFillColorAndOpacity(HealthColor);
    Label->SetText(FText::FromString(FString::Printf(TEXT("%s  %.0f / %.0f"), Ship->Team() == 0 ? TEXT("BLUE") : TEXT("RED"), Ship->CurrentHealth, Ship->MaxHealth)));
    Label->SetColorAndOpacity(Ship->Team() == 0 ? FLinearColor(.15f, .55f, 1) : FLinearColor(1, .18f, .18f));
    SetVisibility(Ship->bIsDestroyed ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}
void UShipHealthBarWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (BoundShip)
        Refresh(BoundShip);
}
TSharedRef<SWidget> UShipHUDWidget::RebuildWidget()
{
    if (!WidgetTree)
        WidgetTree = NewObject<UWidgetTree>(this);
    auto *Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Canvas;
    auto *Frame = WidgetTree->ConstructWidget<UBorder>();
    Frame->SetBrushColor(FLinearColor(.008f, .015f, .03f, .88f));
    Frame->SetPadding(FMargin(18, 12));
    auto *FrameSlot = Canvas->AddChildToCanvas(Frame);
    FrameSlot->SetPosition(FVector2D(24, 24));
    FrameSlot->SetSize(FVector2D(440, 190));
    auto *Box = WidgetTree->ConstructWidget<UVerticalBox>();
    Frame->AddChild(Box);
    Text(WidgetTree, Box, 12, FLinearColor(.3f, .7f, 1))->SetText(FText::FromString(TEXT("NAVAL BATTLE  /  BLUE FLEET")));
    Score = Text(WidgetTree, Box, 23, FLinearColor::White);
    Health = Text(WidgetTree, Box, 16, FLinearColor::White);
    Bar = Progress(WidgetTree, Box, 16);
    Notice = Text(WidgetTree, Box, 14, FLinearColor(1, .8f, .35f));
    Turn = Text(WidgetTree, Box, 14, FLinearColor(.55f,.85f,1));
    Result = WidgetTree->ConstructWidget<UTextBlock>();
    FSlateFontInfo Font = Result->GetFont();
    Font.Size = 38;
    Result->SetFont(Font);
    Result->SetJustification(ETextJustify::Center);
    Result->SetShadowColorAndOpacity(FLinearColor::Black);
    Result->SetShadowOffset(FVector2D(3, 3));
    auto *Center = Canvas->AddChildToCanvas(Result);
    Center->SetAnchors(FAnchors(.5f, .35f));
    Center->SetAlignment(FVector2D(.5f, .5f));
    Center->SetAutoSize(true);
    SetVisibility(ESlateVisibility::HitTestInvisible);
    return Super::RebuildWidget();
}
void UShipHUDWidget::Refresh(UNavalBattleSubsystem *Battle)
{
    if (!Battle || !Bar)
        return;
    ScoreText = FString::Printf(TEXT("BLUE %d   |   RED %d"), Battle->BlueAlive, Battle->RedAlive);
    Score->SetText(FText::FromString(ScoreText));
    if (auto *Ship = Battle->PlayerShip.Get())
    {
        HealthText = FString::Printf(TEXT("YOUR SHIP   %.0f / %.0f HP"), Ship->CurrentHealth, Ship->MaxHealth);
        Health->SetText(FText::FromString(HealthText));
        float Fraction = Ship->CurrentHealth / FMath::Max(Ship->MaxHealth, 1.f);
        Bar->SetPercent(Fraction);
        Bar->SetFillColorAndOpacity(HPColor(Fraction));
    }
    Notice->SetText(FText::FromString(Battle->Notification));
    if(Turn)
    {
        FString Status=TEXT("REAL TIME  |  P: match setup");
        if(Battle->BattleMode==ENavalBattleMode::TurnBased)
        {
            const auto* Active=IsValid(Battle->ActiveTurnShip)?Battle->ActiveTurnShip->FindComponentByClass<UShipWeaponComponent>():nullptr;
            Status=Active?FString::Printf(TEXT("TURN %d  |  %s  |  %.1fs"),Battle->TurnNumber,*Battle->ActiveTurnLabel,Battle->TurnSecondsRemaining()):TEXT("TURN BASED  |  Battle ended");
        }
        if(!Battle->ConfigurationError.IsEmpty())Status=Battle->ConfigurationError;
        Turn->SetText(FText::FromString(Status));
    }
    ResultText = Battle->Outcome;
    if(auto* PC=Cast<ACustomPlayerController>(GetOwningPlayer());PC && PC->bDeathSpectating)
    {
        HealthText=PC->FollowedTeammate?TEXT("SPECTATING BLUE  |  Tab: next  M: overview"):TEXT("BATTLEFIELD OVERVIEW  |  Tab: teammate");
        Health->SetText(FText::FromString(HealthText));
    }
    Result->SetText(FText::FromString(ResultText));
    Result->SetColorAndOpacity(Battle->Outcome == TEXT("VICTORY") ? FLinearColor(.1f, 1, .4f) : FLinearColor(1, .3f, .15f));
}
bool UNavalBattleSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game || Type == EWorldType::PIE; }
void UNavalBattleSubsystem::Deinitialize()
{
    GetWorld()->GetTimerManager().ClearTimer(NoticeTimer);
    GetWorld()->GetTimerManager().ClearTimer(MatchTimer);
    TurnRoster.Empty();ActiveTurnShip=nullptr;
    if (HUD)
        HUD->RemoveFromParent();
    HUD = nullptr;
    PlayerShip = nullptr;
    Registered.Empty();
    Fallen.Empty();
    Super::Deinitialize();
}
void UNavalBattleSubsystem::RegisterShip(UShipWeaponComponent *Ship)
{
    if (Registered.Contains(Ship))
        return;
    Registered.Add(Ship);
    if (Ship->Team() == 0)
        ++BlueAlive;
    else if (Ship->Team() == 1)
        ++RedAlive;
    EnsureHUD();
}
void UNavalBattleSubsystem::EnsureHUD()
{
    auto *PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC || !PC->IsLocalController())
        return;
    if (APawn *Pawn = PC->GetPawn())
        if (auto *W = Pawn->FindComponentByClass<UShipWeaponComponent>())
            PlayerShip = W;
    if (!HUD)
    {
        UClass *Class = LoadClass<UShipHUDWidget>(nullptr, TEXT("/Game/NavalBattle/UI/WBP_ShipHUD.WBP_ShipHUD_C"));
        if (!Class)
            Class = UShipHUDWidget::StaticClass();
        HUD = CreateWidget<UShipHUDWidget>(PC, Class);
        if (HUD)
            HUD->AddToViewport(20);
    }
    RefreshHUD();
}

void UNavalBattleSubsystem::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);
    World.GetTimerManager().SetTimer(MatchTimer,this,&UNavalBattleSubsystem::UpdateMatch,.1f,true);
}

bool UNavalBattleSubsystem::ConfigureRoster()
{
    auto* PC=UGameplayStatics::GetPlayerController(GetWorld(),0);
    APawn* Player=PC?PC->GetPawn():nullptr;
    if(!Player || !Player->FindComponentByClass<UShipWeaponComponent>())return false;
    TArray<APawn*> Teams[2];
    for(TActorIterator<APawn> It(GetWorld());It;++It)
        if(auto* W=It->FindComponentByClass<UShipWeaponComponent>();W && W->Team()>=0 && W->Team()<2)Teams[W->Team()].Add(*It);
    // Wait for the existing always-loaded five AI, never spawn replacements.
    if(Teams[0].Num()!=3 || Teams[1].Num()!=3)return false;
    for(auto& Team:Teams)Team.Sort([](const APawn& A,const APawn& B){return A.GetFName().LexicalLess(B.GetFName());});
    Teams[0].Remove(Player);Teams[0].Insert(Player,0);
    auto* Settings=GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>();
    const bool Selected=Settings && Settings->bExplicitSelection;
    TurnRoster.Empty();TurnLabels.Empty();Registered.Empty();Fallen.Empty();BlueAlive=0;RedAlive=0;Outcome.Empty();
    for(int32 Team=0;Team<2;++Team)for(int32 Slot=0;Slot<3;++Slot)
    {
        APawn* Pawn=Teams[Team][Slot];auto* W=Pawn->FindComponentByClass<UShipWeaponComponent>();
        const int32 Count=Selected?(Team==0?Settings->BlueCount:Settings->RedCount):3;
        if(Slot>=Count)
        {
            AController* Controller=Pawn->GetController();Pawn->DetachFromControllerPendingDestroy();
            if(IsValid(Controller) && !Controller->IsPlayerController())Controller->Destroy();
            Pawn->Destroy();continue;
        }
        if(Selected)
        {
            if(auto* Move=Pawn->FindComponentByClass<UShipMovementComponent>())Move->ApplyShipClass(Settings->HullClasses[Team*3+Slot]);
            if(auto* Combat=Pawn->FindComponentByClass<UShipCombatComponent>())Combat->SetShootingSkill(Settings->Difficulty);
        }
        TurnRoster.Add(W);Registered.Add(W);
        TurnLabels.Add(FString::Printf(TEXT("%s %d (%s)"),Team==0?TEXT("BLUE"):TEXT("RED"),Slot+1,Team==0 && Slot==0?TEXT("YOU"):TEXT("AI")));
        if(W->bIsDestroyed)Fallen.Add(W);
        else if(Team==0)++BlueAlive;else ++RedAlive;
    }
    PlayerShip=Player->FindComponentByClass<UShipWeaponComponent>();bMatchConfigured=true;
    SetBattleMode(Settings?Settings->Mode:ENavalBattleMode::RealTime);EnsureHUD();
    return true;
}

void UNavalBattleSubsystem::SetBattleMode(ENavalBattleMode Mode)
{
    BattleMode=Mode;ActiveTurnShip=nullptr;ActiveTurnLabel.Empty();TurnNumber=0;NextTeam=0;TeamCursor[0]=TeamCursor[1]=0;
    if(bMatchConfigured && BattleMode==ENavalBattleMode::TurnBased && Outcome.IsEmpty())AdvanceTurn();
    RefreshHUD();
}

float UNavalBattleSubsystem::TurnSecondsRemaining() const
{
    return ActiveTurnShip?FMath::Max(0.,TurnEndsAt-GetWorld()->GetTimeSeconds()):0.f;
}

bool UNavalBattleSubsystem::IsShipActive(AActor* Ship) const
{
    if(!IsValid(Ship) || !Outcome.IsEmpty())return false;
    if(!bMatchConfigured)
    {
        const auto* Settings=GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>();
        return !Settings || Settings->Mode==ENavalBattleMode::RealTime;
    }
    return BattleMode==ENavalBattleMode::RealTime || ActiveTurnShip==Ship;
}

bool UNavalBattleSubsystem::CanShipAct(AActor* Ship)
{
    if(!IsValid(Ship) || !Ship->GetWorld())return false;
    const auto* Battle=Ship->GetWorld()->GetSubsystem<UNavalBattleSubsystem>();
    return !Battle || Battle->IsShipActive(Ship);
}

void UNavalBattleSubsystem::AdvanceTurn()
{
    ActiveTurnShip=nullptr;ActiveTurnLabel.Empty();
    if(!Outcome.IsEmpty())return;
    for(int32 Attempt=0;Attempt<2;++Attempt)
    {
        const int32 Team=NextTeam;NextTeam=1-NextTeam;
        for(int32 I=0;I<TurnRoster.Num();++I)
        {
            const int32 Index=TeamCursor[Team]%TurnRoster.Num();TeamCursor[Team]=(Index+1)%TurnRoster.Num();
            auto* W=TurnRoster[Index].Get();
            if(!W || W->Team()!=Team || !IsValid(W->GetOwner()) || W->bIsDestroyed)continue;
            ActiveTurnShip=W->GetOwner();ActiveTurnLabel=TurnLabels[Index];TurnEndsAt=GetWorld()->GetTimeSeconds()+10.;++TurnNumber;RefreshHUD();return;
        }
    }
}

void UNavalBattleSubsystem::UpdateMatch()
{
    if(!bMatchConfigured)
    {
        if(!ConfigureRoster() && GetWorld()->GetTimeSeconds()>15)
        {ConfigurationError=TEXT("Match setup: waiting for the placed six-ship roster");RefreshHUD();}
        return;
    }
    if(BattleMode==ENavalBattleMode::TurnBased)
    {
        const auto* W=IsValid(ActiveTurnShip)?ActiveTurnShip->FindComponentByClass<UShipWeaponComponent>():nullptr;
        if(!Outcome.IsEmpty())ActiveTurnShip=nullptr;
        else if(!W || W->bIsDestroyed || GetWorld()->GetTimeSeconds()>=TurnEndsAt)AdvanceTurn();
        RefreshHUD();
    }
}
void UNavalBattleSubsystem::RefreshHUD()
{
    if (HUD)
        HUD->Refresh(this);
}
void UNavalBattleSubsystem::ShipDestroyed(UShipWeaponComponent *Ship)
{
    if (Fallen.Contains(Ship) || !Registered.Contains(Ship))
        return;
    Fallen.Add(Ship);
    if (Ship->Team() == 0)
        BlueAlive = FMath::Max(0, BlueAlive - 1);
    else if (Ship->Team() == 1)
        RedAlive = FMath::Max(0, RedAlive - 1);
    Notification = Ship->Team() == 0 ? TEXT("Friendly ship destroyed") : TEXT("Enemy ship destroyed");
    if (RedAlive == 0)
        Outcome = TEXT("VICTORY");
    else if (BlueAlive == 0)
        Outcome = TEXT("DEFEAT");
    GetWorld()->GetTimerManager().SetTimer(NoticeTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
                                                                                         {Notification.Empty();RefreshHUD(); }),
                                           4, false);
    RefreshHUD();
}
