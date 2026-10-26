#include "NavalBattleUI.h"
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
    RefreshNavalMarkers();
    GetWorld()->GetTimerManager().SetTimer(MarkerTimer,this,&UShipMinimapWidget::RefreshNavalMarkers,.1f,true);
}
void UShipMinimapWidget::NativeDestruct()
{
    if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(MarkerTimer);
    Super::NativeDestruct();
}
void UShipMinimapWidget::RefreshNavalMarkers()
{
    // Keep the teammate's geographic projection, texture, canvas and update timer.
    // These three parallel arrays are authored by WBP_MainMinimap's Construct graph.
    auto* ActorsProperty=FindFProperty<FArrayProperty>(GetClass(),TEXT("ShipActors"));
    auto* MarkersProperty=FindFProperty<FArrayProperty>(GetClass(),TEXT("ShipMarkerWidgets"));
    auto* SlotsProperty=FindFProperty<FArrayProperty>(GetClass(),TEXT("ShipMarkerSlots"));
    if(!ActorsProperty || !MarkersProperty || !SlotsProperty)return;
    auto* ActorInner=CastField<FObjectPropertyBase>(ActorsProperty->Inner);
    auto* MarkerInner=CastField<FObjectPropertyBase>(MarkersProperty->Inner);
    if(!ActorInner || !MarkerInner)return;
    FScriptArrayHelper Actors(ActorsProperty,ActorsProperty->ContainerPtrToValuePtr<void>(this));
    FScriptArrayHelper Markers(MarkersProperty,MarkersProperty->ContainerPtrToValuePtr<void>(this));
    FScriptArrayHelper Slots(SlotsProperty,SlotsProperty->ContainerPtrToValuePtr<void>(this));
    if(Actors.Num()!=Markers.Num() || Actors.Num()!=Slots.Num())return;
    for(int32 Index=Actors.Num()-1;Index>=0;--Index)
    {
        AActor* Ship=Cast<AActor>(ActorInner->GetObjectPropertyValue(Actors.GetRawPtr(Index)));
        UUserWidget* Marker=Cast<UUserWidget>(MarkerInner->GetObjectPropertyValue(Markers.GetRawPtr(Index)));
        const UShipWeaponComponent* Weapon=IsValid(Ship)?Ship->FindComponentByClass<UShipWeaponComponent>():nullptr;
        if(!Weapon || Weapon->bIsDestroyed)
        {
            if(IsValid(Marker))Marker->RemoveFromParent();
            Actors.RemoveValues(Index);Markers.RemoveValues(Index);Slots.RemoveValues(Index);
        }
        else if(IsValid(Marker))
        {
            // The two marker textures already carry their own team colours; a white
            // widget tint preserves them instead of multiplying the old blue/red tint.
            Marker->SetColorAndOpacity(FLinearColor::White);
            if(UImage* MarkerImage=Cast<UImage>(Marker->GetWidgetFromName(TEXT("Image_ShipMarker"))))
                if(UTexture2D* Icon=TeamMarkerIcon(Weapon->GetTeamName(),Weapon))
                    if(MarkerImage->GetBrush().GetResourceObject()!=Icon)
                        MarkerImage->SetBrushFromTexture(Icon,false);
            if(UTextBlock* NameText=Cast<UTextBlock>(Marker->GetWidgetFromName(TEXT("Text_ShipName"))))
                NameText->SetText(FText::FromString(Weapon->GetShipName()));
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
    if(!WidgetTree)WidgetTree=NewObject<UWidgetTree>(this);
    auto* Frame=WidgetTree->ConstructWidget<UBorder>();Frame->SetBrushColor(FLinearColor(.008f,.015f,.03f,.88f));Frame->SetPadding(FMargin(8,3));WidgetTree->RootWidget=Frame;
    auto* Box=WidgetTree->ConstructWidget<UVerticalBox>();Frame->AddChild(Box);
    Label=Text(WidgetTree,Box,13,FLinearColor::White);Bar=Progress(WidgetTree,Box,10);
    SetVisibility(ESlateVisibility::HitTestInvisible);return Super::RebuildWidget();
}
void UShipHealthBarWidget::Refresh(UShipWeaponComponent* Ship)
{
    BoundShip=Ship;
    if(!Ship || !Bar)return;
    DisplayedFraction=FMath::Clamp(Ship->CurrentHealth/FMath::Max(Ship->MaxHealth,1.f),0.f,1.f);HealthColor=HPColor(DisplayedFraction);
    Bar->SetPercent(DisplayedFraction);Bar->SetFillColorAndOpacity(HealthColor);
    Label->SetText(FText::FromString(FString::Printf(TEXT("%s  %.0f / %.0f"),Ship->Team()==0?TEXT("BLUE"):TEXT("RED"),Ship->CurrentHealth,Ship->MaxHealth)));
    Label->SetColorAndOpacity(Ship->Team()==0?FLinearColor(.15f,.55f,1):FLinearColor(1,.18f,.18f));
    SetVisibility(Ship->bIsDestroyed?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
}
void UShipHealthBarWidget::NativeConstruct(){Super::NativeConstruct();if(BoundShip)Refresh(BoundShip);}
