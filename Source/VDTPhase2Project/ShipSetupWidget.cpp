#include "ShipSetupWidget.h"
#include "CustomPlayerController.h"
#include "NavalMatchSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
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
    auto RowStyle=Combo->GetItemStyle();
    RowStyle.TextColor=FSlateColor(FLinearColor::White);
    RowStyle.SelectedTextColor=FSlateColor(FLinearColor::White);
    Combo->SetItemStyle(RowStyle);
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    Combo->ForegroundColor=FSlateColor(FLinearColor::White);
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
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
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(700);Size->SetHeightOverride(900);Scale->AddChild(Size);
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
            HullSelectors[Team*3+I]=Choices(WidgetTree,Row,{TEXT("Small"),TEXT("Large"),TEXT("Medium")});
        }
    }
    auto* Options=WidgetTree->ConstructWidget<UHorizontalBox>();Box->AddChildToVerticalBox(Options);
    for(int32 I=0;I<3;++I)
    {
        auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();auto* LayoutSlot=Options->AddChildToHorizontalBox(Column);LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        SetupText(WidgetTree,Column,I==0?TEXT("AI difficulty"):I==1?TEXT("Battle mode"):TEXT("Player team"),13);
        if(I==0)DifficultySelector=Choices(WidgetTree,Column,{TEXT("Easy"),TEXT("Normal"),TEXT("Hard")});
        else if(I==1)ModeSelector=Choices(WidgetTree,Column,{TEXT("Real-Time"),TEXT("Turn-Based (10s)")});
        else PlayerTeamSelector=Choices(WidgetTree,Column,{TEXT("Blue Team"),TEXT("Red Team")});
    }
    SetupText(WidgetTree,Box,TEXT("Battlefield seed / 0 creates a new layout"),12,FLinearColor(.65f,.75f,.85f));
    SeedInput=WidgetTree->ConstructWidget<UEditableTextBox>();SeedInput->SetText(FText::FromString(TEXT("0")));Box->AddChildToVerticalBox(SeedInput)->SetPadding(FMargin(0,3,0,6));
    SetupText(WidgetTree,Box,TEXT("Control"),13);
    ControlSelector=Choices(WidgetTree,Box,{TEXT("Player + AI"),TEXT("Full AI Demo / spectator")});
    BalanceSelector=Choices(WidgetTree,Box,{TEXT("Standard demo: 100 HP / 10 damage"),TEXT("Class balance: Small 38/6, Medium & Large 100/10")});
    NavigationSelector=Choices(WidgetTree,Box,{TEXT("Island defense demo"),TEXT("Placed ships and targets / Player + AI")});
    Error=SetupText(WidgetTree,Box,TEXT(""),12,FLinearColor(1,.4f,.3f));
    auto* Buttons=WidgetTree->ConstructWidget<UHorizontalBox>();Box->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(0,7,0,0));
    for(int32 I=0;I<2;++I)
    {
        auto* Button=WidgetTree->ConstructWidget<UButton>();Button->SetBackgroundColor(I?FLinearColor(.12f,.16f,.22f):FLinearColor(.03f,.3f,.5f));
        auto* LayoutSlot=Buttons->AddChildToHorizontalBox(Button);LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LayoutSlot->SetPadding(FMargin(I?8:0,0,0,0));
        auto* ButtonLabel=SetupText(WidgetTree,Button,I?TEXT("QUIT"):TEXT("START NEW MATCH"),16);
        if(I){QuitLabel=ButtonLabel;Button->OnClicked.AddDynamic(this,&UShipSetupWidget::OnQuitClicked);}
        else Button->OnClicked.AddDynamic(this,&UShipSetupWidget::StartMatch);
    }
    SetIsFocusable(true);return Super::RebuildWidget();
}

void UShipSetupWidget::NativeConstruct()
{
    Super::NativeConstruct();
    auto* Settings=GetGameInstance()?GetGameInstance()->GetSubsystem<UNavalMatchSettings>():nullptr;
    bInitialSetup=!Settings || !Settings->bExplicitSelection;
    if(QuitLabel)QuitLabel->SetText(FText::FromString(bInitialSetup?TEXT("QUIT"):TEXT("BACK TO GAME")));
    if(Settings)
    {
        BlueCountSelector->SetSelectedIndex(Settings->BlueCount-1);RedCountSelector->SetSelectedIndex(Settings->RedCount-1);
        DifficultySelector->SetSelectedIndex(static_cast<int32>(Settings->Difficulty));
        if(ModeSelector)ModeSelector->SetSelectedIndex(static_cast<int32>(Settings->Mode));
        if(PlayerTeamSelector)PlayerTeamSelector->SetSelectedIndex(FMath::Clamp(Settings->PlayerTeam,0,1));
        if(ControlSelector)ControlSelector->SetSelectedIndex(Settings->bFullAIDemo?1:0);
        if(BalanceSelector)BalanceSelector->SetSelectedIndex(Settings->bStandardDemoBalance?0:1);
        if(NavigationSelector)NavigationSelector->SetSelectedIndex(Settings->bUseSectorBattlefield?0:1);
        for(int32 I=0;I<6;++I)HullSelectors[I]->SetSelectedIndex(static_cast<int32>(Settings->HullClasses[I]));
        if(SeedInput)SeedInput->SetText(FText::FromString(FString::FromInt(Settings->BattlefieldSeed)));
    }
    RefreshSlots();SetKeyboardFocus();
}

void UShipSetupWidget::RefreshSlots()
{
    if(!BlueCountSelector || !RedCountSelector || !PlayerTeamSelector || SlotRows.Num()!=6)return;
    LastBlue=BlueCountSelector->GetSelectedIndex()+1;LastRed=RedCountSelector->GetSelectedIndex()+1;
    LastPlayerTeam=PlayerTeamSelector->GetSelectedIndex();
    LastControl=ControlSelector?ControlSelector->GetSelectedIndex():0;
    for(int32 Team=0;Team<2;++Team)
    {
        const int32 Count=Team==0?LastBlue:LastRed;
        for(int32 ShipIndex=0;ShipIndex<3;++ShipIndex)
        {
            const int32 Index=Team*3+ShipIndex;
            if(SlotRows[Index])SlotRows[Index]->SetVisibility(ShipIndex<Count?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
            if(SlotLabels[Index])SlotLabels[Index]->SetText(FText::FromString(FString::Printf(TEXT("Ship %d  /  %s"),ShipIndex+1,LastControl==0 && Team==LastPlayerTeam && ShipIndex==0?TEXT("PLAYER"):TEXT("AI"))));
        }
    }
}

void UShipSetupWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);
    if(BlueCountSelector && RedCountSelector && PlayerTeamSelector &&
        (LastBlue!=BlueCountSelector->GetSelectedIndex()+1 || LastRed!=RedCountSelector->GetSelectedIndex()+1 || LastPlayerTeam!=PlayerTeamSelector->GetSelectedIndex() || LastControl!=ControlSelector->GetSelectedIndex()))
        RefreshSlots();
}

bool UShipSetupWidget::ApplySelection()
{
    auto* Settings=GetGameInstance()->GetSubsystem<UNavalMatchSettings>();if(!Settings || HullSelectors.Num()!=6)return false;
    if(ControlSelector->GetSelectedIndex()==1 && NavigationSelector->GetSelectedIndex()==1)
    {if(Error)Error->SetText(FText::FromString(TEXT("Full AI Demo requires Island defense demo navigation.")));return false;}
    int32 Seed=0;
    if(!SeedInput || !LexTryParseString(Seed,*SeedInput->GetText().ToString()) || Seed<0)
    {if(Error)Error->SetText(FText::FromString(TEXT("Use 0 for a new seed, or a positive integer.")));return false;}
    TArray<EShipHullClass> Classes;
    for(const auto& Selector:HullSelectors)Classes.Add(static_cast<EShipHullClass>(Selector->GetSelectedIndex()));
    const bool OK=Settings->Configure(BlueCountSelector->GetSelectedIndex()+1,RedCountSelector->GetSelectedIndex()+1,Classes,
        static_cast<EShipAISkill>(DifficultySelector->GetSelectedIndex()),static_cast<ENavalBattleMode>(ModeSelector->GetSelectedIndex()),PlayerTeamSelector->GetSelectedIndex());
    if(!OK && Error)Error->SetText(FText::FromString(TEXT("Select 1-3 ships per fleet, difficulty and mode.")));
    if(OK)
    {
        Settings->SetBattlefieldSeed(Seed);
        Settings->bFullAIDemo=ControlSelector && ControlSelector->GetSelectedIndex()==1;
        Settings->bStandardDemoBalance=!BalanceSelector || BalanceSelector->GetSelectedIndex()==0;
        Settings->bUseSectorBattlefield=!NavigationSelector || NavigationSelector->GetSelectedIndex()==0;
    }
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

void UShipSetupWidget::OnQuitClicked()
{
    if(!bInitialSetup){Cancel();return;}
    if(auto* PC=Cast<ACustomPlayerController>(GetOwningPlayer()))PC->CloseMatchSetup();
    UKismetSystemLibrary::QuitGame(this,GetOwningPlayer(),EQuitPreference::Quit,false);
}

FReply UShipSetupWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if(Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::P){Cancel();return FReply::Handled();}
    return Super::NativeOnKeyDown(Geometry,Event);
}
