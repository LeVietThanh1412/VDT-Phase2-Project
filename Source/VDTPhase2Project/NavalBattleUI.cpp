#include "NavalBattleUI.h"
#include "ShipMovementComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "ShipCombatComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "Kismet/GameplayStatics.h"

namespace
{
FLinearColor HPColor(float P) { return P>.6f?FLinearColor(.08f,.85f,.22f):P>.3f?FLinearColor(1,.78f,.03f):FLinearColor(.95f,.06f,.05f); }
UTextBlock* Text(UWidgetTree* Tree,UVerticalBox* Box,int32 Size,FLinearColor Color)
{
    auto* T=Tree->ConstructWidget<UTextBlock>();FSlateFontInfo Font=T->GetFont();Font.Size=Size;T->SetFont(Font);T->SetColorAndOpacity(Color);
    auto* Slot=Box->AddChildToVerticalBox(T);Slot->SetPadding(FMargin(0,3));return T;
}
UProgressBar* Progress(UWidgetTree* Tree,UVerticalBox* Box,float Height)
{
    auto* Size=Tree->ConstructWidget<USizeBox>();Size->SetHeightOverride(Height);Box->AddChildToVerticalBox(Size);
    auto* Bar=Tree->ConstructWidget<UProgressBar>();Bar->SetPercent(1);Size->AddChild(Bar);return Bar;
}
AActor* MarkerShip(const UUserWidget* Marker)
{
    if(!Marker)return nullptr;
    const FObjectPropertyBase* ShipActorProperty=FindFProperty<FObjectPropertyBase>(Marker->GetClass(),TEXT("ShipActor"));
    return ShipActorProperty?Cast<AActor>(ShipActorProperty->GetObjectPropertyValue_InContainer(Marker)):nullptr;
}
UTexture2D* TeamMarkerIcon(const FString& TeamName,const UShipWeaponComponent* Weapon)
{
    bool bRed=TeamName.Equals(TEXT("Red"),ESearchCase::IgnoreCase) || TeamName.Equals(TEXT("Enemy"),ESearchCase::IgnoreCase);
    if(!bRed && !TeamName.Equals(TEXT("Blue"),ESearchCase::IgnoreCase) && Weapon)bRed=Weapon->Team()!=0;
    static TWeakObjectPtr<UTexture2D> BlueIcon,RedIcon;
    TWeakObjectPtr<UTexture2D>& Icon=bRed?RedIcon:BlueIcon;
    if(!Icon.IsValid())Icon=LoadObject<UTexture2D>(nullptr,bRed?TEXT("/Game/Game/UI/Minimap/Textures/ObjectTextures/ShipMarkerIcon-2.ShipMarkerIcon-2"):TEXT("/Game/Game/UI/Minimap/Textures/ObjectTextures/ShipMarkerIcon-1.ShipMarkerIcon-1"));
    return Icon.Get();
}
}
void UShipMinimapWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if(UWorld* World=GetWorld())
        World->GetTimerManager().SetTimer(MarkerTimer,this,&UShipMinimapWidget::RemoveDestroyedMarkers,.1f,true);
}
void UShipMinimapWidget::NativeDestruct()
{
    if(UWorld* World=GetWorld())World->GetTimerManager().ClearTimer(MarkerTimer);
    Super::NativeDestruct();
}
void UShipMinimapWidget::RemoveDestroyedMarkers()
{
    auto* ActorsProperty=FindFProperty<FArrayProperty>(GetClass(),TEXT("ShipActors"));
    auto* MarkersProperty=FindFProperty<FArrayProperty>(GetClass(),TEXT("ShipMarkerWidgets"));
    auto* SlotsProperty=FindFProperty<FArrayProperty>(GetClass(),TEXT("ShipMarkerSlots"));
    auto* MarkerInner=MarkersProperty?CastField<FObjectPropertyBase>(MarkersProperty->Inner):nullptr;
    if(!ActorsProperty || !MarkersProperty || !SlotsProperty || !MarkerInner)
    {
        if(!bWarnedMissingArrays)
        {
            UE_LOG(LogTemp,Warning,TEXT("%s: minimap marker arrays not found."),*GetName());
            bWarnedMissingArrays=true;
        }
        return;
    }
    auto* ActorInner=CastField<FObjectPropertyBase>(ActorsProperty->Inner);
    FScriptArrayHelper Actors(ActorsProperty,ActorsProperty->ContainerPtrToValuePtr<void>(this));
    FScriptArrayHelper Markers(MarkersProperty,MarkersProperty->ContainerPtrToValuePtr<void>(this));
    FScriptArrayHelper Slots(SlotsProperty,SlotsProperty->ContainerPtrToValuePtr<void>(this));
    if(Actors.Num()!=Markers.Num() || Actors.Num()!=Slots.Num())return;
    for(int32 Index=Actors.Num()-1;Index>=0;--Index)
    {
        UUserWidget* Marker=Cast<UUserWidget>(MarkerInner->GetObjectPropertyValue(Markers.GetRawPtr(Index)));
        UObject* ShipObject=ActorInner?ActorInner->GetObjectPropertyValue(Actors.GetRawPtr(Index)):nullptr;
        AActor* Ship=Cast<AActor>(ShipObject);
        if(!Ship && ShipObject)
            if(auto* Component=Cast<UActorComponent>(ShipObject))Ship=Component->GetOwner();
        if(!IsValid(Ship))Ship=MarkerShip(Marker);
        const UShipWeaponComponent* Weapon=IsValid(Ship)?Ship->FindComponentByClass<UShipWeaponComponent>():nullptr;
        if(!IsValid(Ship) || !Weapon || Weapon->bIsDestroyed || Weapon->CurrentHealth<=0.f)
        {
            if(IsValid(Marker))Marker->RemoveFromParent();
            Actors.RemoveValues(Index);
            Markers.RemoveValues(Index);
            Slots.RemoveValues(Index);
        }
    }
}
void UShipMarkerWidget::NativeTick(const FGeometry& MyGeometry,float InDeltaTime)
{
    Super::NativeTick(MyGeometry,InDeltaTime);
    if(!MarkerImage)MarkerImage=Cast<UImage>(GetWidgetFromName(TEXT("Image_ShipMarker")));
    const AActor* Ship=MarkerShip(this);
    const UShipWeaponComponent* Weapon=IsValid(Ship)?Ship->FindComponentByClass<UShipWeaponComponent>():nullptr;
    if(!MarkerImage || !Weapon)return;
    if(UTexture2D* Icon=TeamMarkerIcon(Weapon->GetTeamName(),Weapon))
        if(MarkerImage->GetBrush().GetResourceObject()!=Icon)
            MarkerImage->SetBrushFromTexture(Icon,false);
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
bool UNavalBattleSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game || Type == EWorldType::PIE; }
void UNavalBattleSubsystem::Deinitialize()
{
    GetWorld()->GetTimerManager().ClearTimer(MatchTimer);
    TurnRoster.Empty();ActiveTurnShip=nullptr;
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
    auto* Settings=GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>();
    const bool Selected=Settings && Settings->bExplicitSelection;
    const int32 PlayerTeam=Selected?FMath::Clamp(Settings->PlayerTeam,0,1):0;
    auto* PlayerWeapon=Player->FindComponentByClass<UShipWeaponComponent>();
    const int32 OldPlayerTeam=PlayerWeapon->Team();
    if(OldPlayerTeam!=PlayerTeam)
    {
        // Move the player to the chosen side: swap hulls with an AI of that fleet and
        // flip both factions so targeting and markers follow the new team.
        APawn* Other=Teams[PlayerTeam][0];
        const FTransform PlayerTransform=Player->GetActorTransform();
        Player->SetActorTransform(Other->GetActorTransform(),false,nullptr,ETeleportType::TeleportPhysics);
        Other->SetActorTransform(PlayerTransform,false,nullptr,ETeleportType::TeleportPhysics);
        if(auto* Combat=Player->FindComponentByClass<UShipCombatComponent>())Combat->Faction=PlayerTeam;
        if(auto* Combat=Other->FindComponentByClass<UShipCombatComponent>())Combat->Faction=1-PlayerTeam;
        PlayerWeapon->Faction=PlayerTeam;
        if(auto* OtherWeapon=Other->FindComponentByClass<UShipWeaponComponent>())OtherWeapon->Faction=1-PlayerTeam;
        Teams[PlayerTeam].Remove(Other);Teams[PlayerTeam].Add(Player);
        Teams[OldPlayerTeam].Remove(Player);Teams[OldPlayerTeam].Add(Other);
        for(auto& Team:Teams)Team.Sort([](const APawn& A,const APawn& B){return A.GetFName().LexicalLess(B.GetFName());});
    }
    Teams[PlayerTeam].Remove(Player);Teams[PlayerTeam].Insert(Player,0);
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
        TurnLabels.Add(FString::Printf(TEXT("%s %d (%s)"),Team==0?TEXT("BLUE"):TEXT("RED"),Slot+1,Team==PlayerTeam && Slot==0?TEXT("YOU"):TEXT("AI")));
        if(W->bIsDestroyed)Fallen.Add(W);
        else if(Team==0)++BlueAlive;else ++RedAlive;
    }
    PlayerShip=Player->FindComponentByClass<UShipWeaponComponent>();bMatchConfigured=true;
    SetBattleMode(Settings?Settings->Mode:ENavalBattleMode::RealTime);
    return true;
}

void UNavalBattleSubsystem::SetBattleMode(ENavalBattleMode Mode)
{
    BattleMode=Mode;ActiveTurnShip=nullptr;ActiveTurnLabel.Empty();TurnNumber=0;NextTeam=0;TeamCursor[0]=TeamCursor[1]=0;
    if(bMatchConfigured && BattleMode==ENavalBattleMode::TurnBased && Outcome.IsEmpty())AdvanceTurn();
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
            ActiveTurnShip=W->GetOwner();ActiveTurnLabel=TurnLabels[Index];TurnEndsAt=GetWorld()->GetTimeSeconds()+10.;++TurnNumber;return;
        }
    }
}

void UNavalBattleSubsystem::UpdateMatch()
{
    if(!bMatchConfigured)
    {
        ConfigureRoster();
        return;
    }
    if(BattleMode==ENavalBattleMode::TurnBased)
    {
        const auto* W=IsValid(ActiveTurnShip)?ActiveTurnShip->FindComponentByClass<UShipWeaponComponent>():nullptr;
        if(!Outcome.IsEmpty())ActiveTurnShip=nullptr;
        else if(!W || W->bIsDestroyed || GetWorld()->GetTimeSeconds()>=TurnEndsAt)AdvanceTurn();
    }
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
    const auto* Settings=GetWorld()->GetGameInstance()?GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>():nullptr;
    const int32 PlayerTeam=Settings && Settings->bExplicitSelection?FMath::Clamp(Settings->PlayerTeam,0,1):0;
    if ((PlayerTeam==0?RedAlive:BlueAlive) == 0)
        Outcome = TEXT("VICTORY");
    else if ((PlayerTeam==0?BlueAlive:RedAlive) == 0)
        Outcome = TEXT("DEFEAT");
}
