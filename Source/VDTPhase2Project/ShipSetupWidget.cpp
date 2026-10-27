#include "ShipSetupWidget.h"
#include "CustomPlayerController.h"
#include "NavalMatchSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"

namespace
{
UTextBlock* SetupText(UWidgetTree* Tree,UPanelWidget* Parent,const FString& Value,int32 Size=15,FLinearColor Color=FLinearColor::White)
{
    auto* Text=Tree->ConstructWidget<UTextBlock>();Text->SetText(FText::FromString(Value));
    auto Font=Text->GetFont();Font.Size=Size;Text->SetFont(Font);Text->SetColorAndOpacity(Color);
    Parent->AddChild(Text);return Text;
}
UComboBoxString* Choices(UWidgetTree* Tree,UVerticalBox* Parent,const TArray<FString>& Options)
{
    auto* Combo=Tree->ConstructWidget<UComboBoxString>();
    for(const auto& Option:Options)Combo->AddOption(Option);
    auto* Slot=Parent->AddChildToVerticalBox(Combo);Slot->SetPadding(FMargin(0,3,0,9));
    Combo->SetSelectedIndex(0);return Combo;
}
}

TSharedRef<SWidget> UShipSetupWidget::RebuildWidget()
{
    if(!WidgetTree)WidgetTree=NewObject<UWidgetTree>(this);
    auto* Background=WidgetTree->ConstructWidget<UBorder>();Background->SetBrushColor(FLinearColor(.005f,.01f,.025f,.96f));
    WidgetTree->RootWidget=Background;
    auto* Scale=WidgetTree->ConstructWidget<UScaleBox>();Scale->SetStretch(EStretch::ScaleToFit);Background->AddChild(Scale);
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(640);Size->SetHeightOverride(560);Scale->AddChild(Size);
    auto* Frame=WidgetTree->ConstructWidget<UBorder>();Frame->SetPadding(FMargin(22));Frame->SetBrushColor(FLinearColor(.015f,.03f,.055f,1));Size->AddChild(Frame);
    auto* Box=WidgetTree->ConstructWidget<UVerticalBox>();Frame->AddChild(Box);
    SetupText(WidgetTree,Box,TEXT("MATCH SETUP"),25,FLinearColor(.5f,.8f,1));
    auto* Teams=WidgetTree->ConstructWidget<UHorizontalBox>();Box->AddChildToVerticalBox(Teams)->SetPadding(FMargin(0,12,0,0));
    HullSelectors.Empty();HullSelectors.SetNum(6);
    SlotRows.Empty();SlotRows.SetNum(6);
    SlotLabels.Empty();SlotLabels.SetNum(6);
    for(int32 Team=0;Team<2;++Team)
    {
        auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();auto* LayoutSlot=Teams->AddChildToHorizontalBox(Column);
        LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LayoutSlot->SetPadding(FMargin(Team?12:0,0,Team?0:12,0));
        SetupText(WidgetTree,Column,Team==0?TEXT("BLUE TEAM"):TEXT("RED TEAM"),14,Team==0?FLinearColor(.2f,.6f,1):FLinearColor(1,.35f,.3f));
        auto* Count=Choices(WidgetTree,Column,{TEXT("1 ship"),TEXT("2 ships"),TEXT("3 ships")});
        if(Team==0)BlueCountSelector=Count;else RedCountSelector=Count;
        for(int32 I=0;I<3;++I)
        {
            auto* Row=WidgetTree->ConstructWidget<UVerticalBox>();Column->AddChildToVerticalBox(Row);
            SlotRows[Team*3+I]=Row;
            SlotLabels[Team*3+I]=SetupText(WidgetTree,Row,TEXT(""),13);
            HullSelectors[Team*3+I]=Choices(WidgetTree,Row,{TEXT("Small Ship"),TEXT("Big Ship")});
        }
    }
    auto* Difficulty=WidgetTree->ConstructWidget<UVerticalBox>();Box->AddChildToVerticalBox(Difficulty);
    SetupText(WidgetTree,Difficulty,TEXT("AI shooting difficulty"),13);
    DifficultySelector=Choices(WidgetTree,Difficulty,{TEXT("Easy"),TEXT("Normal"),TEXT("Hard")});
    auto* PlayerTeam=WidgetTree->ConstructWidget<UVerticalBox>();Box->AddChildToVerticalBox(PlayerTeam);
    SetupText(WidgetTree,PlayerTeam,TEXT("Choose player team"),13);
    PlayerTeamSelector=Choices(WidgetTree,PlayerTeam,{TEXT("Blue Team"),TEXT("Red Team")});
    Error=SetupText(WidgetTree,Box,TEXT(""),12,FLinearColor(1,.4f,.3f));
    auto* Buttons=WidgetTree->ConstructWidget<UHorizontalBox>();Box->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(0,7,0,0));
    for(int32 I=0;I<2;++I)
    {
        auto* Button=WidgetTree->ConstructWidget<UButton>();Button->SetBackgroundColor(I?FLinearColor(.12f,.16f,.22f):FLinearColor(.03f,.3f,.5f));
        auto* LayoutSlot=Buttons->AddChildToHorizontalBox(Button);LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LayoutSlot->SetPadding(FMargin(I?8:0,0,0,0));
        SetupText(WidgetTree,Button,I?TEXT("QUIT"):TEXT("START NEW MATCH"),16);
        if(I)Button->OnClicked.AddDynamic(this,&UShipSetupWidget::Cancel);else Button->OnClicked.AddDynamic(this,&UShipSetupWidget::StartMatch);
    }
    SetIsFocusable(true);return Super::RebuildWidget();
}

void UShipSetupWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if(auto* Settings=GetGameInstance()->GetSubsystem<UNavalMatchSettings>())
    {
        BlueCountSelector->SetSelectedIndex(Settings->BlueCount-1);RedCountSelector->SetSelectedIndex(Settings->RedCount-1);
        DifficultySelector->SetSelectedIndex(static_cast<int32>(Settings->Difficulty));
        if(PlayerTeamSelector)PlayerTeamSelector->SetSelectedIndex(FMath::Clamp(Settings->PlayerTeam,0,1));
        for(int32 I=0;I<6;++I)HullSelectors[I]->SetSelectedIndex(static_cast<int32>(Settings->HullClasses[I]));
    }
    RefreshSlots();SetKeyboardFocus();
}

void UShipSetupWidget::RefreshSlots()
{
    if(!BlueCountSelector || !RedCountSelector || !PlayerTeamSelector || SlotRows.Num()!=6)return;
    LastBlue=BlueCountSelector->GetSelectedIndex()+1;LastRed=RedCountSelector->GetSelectedIndex()+1;
    LastPlayerTeam=PlayerTeamSelector->GetSelectedIndex();
    for(int32 Team=0;Team<2;++Team)
    {
        const int32 Count=Team==0?LastBlue:LastRed;
        for(int32 ShipIndex=0;ShipIndex<3;++ShipIndex)
        {
            const int32 Index=Team*3+ShipIndex;
            if(SlotRows[Index])SlotRows[Index]->SetVisibility(ShipIndex<Count?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
            if(SlotLabels[Index])SlotLabels[Index]->SetText(FText::FromString(FString::Printf(TEXT("Ship %d  /  %s"),ShipIndex+1,Team==LastPlayerTeam && ShipIndex==0?TEXT("PLAYER"):TEXT("AI"))));
        }
    }
}

void UShipSetupWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);
    if(BlueCountSelector && RedCountSelector && PlayerTeamSelector &&
        (LastBlue!=BlueCountSelector->GetSelectedIndex()+1 || LastRed!=RedCountSelector->GetSelectedIndex()+1 || LastPlayerTeam!=PlayerTeamSelector->GetSelectedIndex()))
        RefreshSlots();
}

bool UShipSetupWidget::ApplySelection()
{
    auto* Settings=GetGameInstance()->GetSubsystem<UNavalMatchSettings>();if(!Settings || HullSelectors.Num()!=6)return false;
    TArray<EShipHullClass> Classes;
    for(const auto& Selector:HullSelectors)Classes.Add(Selector->GetSelectedIndex()==1?EShipHullClass::Large:EShipHullClass::Small);
    const bool OK=Settings->Configure(BlueCountSelector->GetSelectedIndex()+1,RedCountSelector->GetSelectedIndex()+1,Classes,
        static_cast<EShipAISkill>(DifficultySelector->GetSelectedIndex()),ENavalBattleMode::RealTime,PlayerTeamSelector->GetSelectedIndex());
    if(!OK && Error)Error->SetText(FText::FromString(TEXT("Select 1-3 ships per team and a difficulty.")));
    return OK;
}

void UShipSetupWidget::StartMatch()
{
    if(!ApplySelection())return;
    if(auto* PC=Cast<ACustomPlayerController>(GetOwningPlayer()))PC->CloseMatchSetup();
    UGameplayStatics::OpenLevel(this,FName(TEXT("/Game/Levels/MainMapLevel")));
}

void UShipSetupWidget::Cancel()
{
    if(auto* PC=Cast<ACustomPlayerController>(GetOwningPlayer()))PC->CloseMatchSetup();
}

FReply UShipSetupWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if(Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::P){Cancel();return FReply::Handled();}
    return Super::NativeOnKeyDown(Geometry,Event);
}
