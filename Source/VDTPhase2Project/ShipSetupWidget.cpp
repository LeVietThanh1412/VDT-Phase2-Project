#include "ShipSetupWidget.h"
#include "CustomPlayerController.h"
#include "NavalMatchSettings.h"
#include "NavalBattleUI.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
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
#include "Styling/SlateTypes.h"

namespace
{
const FLinearColor TextMuted(.55f,.68f,.82f);
const FLinearColor TextFaint(.42f,.55f,.68f);
const FLinearColor AccentCyan(.35f,.82f,1.f);
const FLinearColor AccentBlue(.45f,.65f,1.f);
const FLinearColor AccentTeam(.65f,.8f,1.f);
const FLinearColor CardFill(.025f,.05f,.085f,1.f);
const FLinearColor CardOutline(1.f,1.f,1.f,.05f);
const FLinearColor FieldFill(.02f,.045f,.075f,1.f);
const FLinearColor FieldOutline(.13f,.28f,.42f,.7f);
const FLinearColor FieldHover(.03f,.06f,.1f,1.f);
const FLinearColor FieldHoverOutline(.25f,.55f,.8f,.8f);

UTextBlock* MakeText(UWidgetTree* Tree,const FString& Value,int32 Size,FLinearColor Color,bool bBold=false)
{
    auto* Text=Tree->ConstructWidget<UTextBlock>();
    Text->SetText(FText::FromString(Value));
    auto Font=Text->GetFont();Font.Size=Size;
    if(bBold)Font.TypefaceFontName=FName(TEXT("Bold"));
    Text->SetFont(Font);Text->SetColorAndOpacity(Color);
    return Text;
}

UTextBlock* SetupText(UWidgetTree* Tree,UPanelWidget* Parent,const FString& Value,int32 Size=15,FLinearColor Color=FLinearColor::White)
{
    auto* Text=MakeText(Tree,Value,Size,Color);
    Parent->AddChild(Text);return Text;
}

void AddDivider(UWidgetTree* Tree,UVerticalBox* Parent,FMargin Padding)
{
    auto* Line=Tree->ConstructWidget<UBorder>();
    Line->SetBrushColor(FLinearColor(1.f,1.f,1.f,.06f));
    auto* LineSize=Tree->ConstructWidget<USizeBox>();
    LineSize->SetHeightOverride(1.f);LineSize->AddChild(Line);
    Parent->AddChildToVerticalBox(LineSize)->SetPadding(Padding);
}

/** A rounded card with an accent title, used to separate the setup groups. */
UVerticalBox* Section(UWidgetTree* Tree,UVerticalBox* Parent,const FString& Title,const FLinearColor& Accent)
{
    auto* Card=Tree->ConstructWidget<UBorder>();
    Card->SetBrush(FSlateRoundedBoxBrush(CardFill,8.f,CardOutline,1.f));
    Card->SetPadding(FMargin(16.f,12.f,16.f,13.f));
    Parent->AddChildToVerticalBox(Card)->SetPadding(FMargin(0.f,0.f,0.f,9.f));
    auto* Body=Tree->ConstructWidget<UVerticalBox>();Card->AddChild(Body);

    auto* Header=Tree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Header);
    auto* Bar=Tree->ConstructWidget<UBorder>();
    Bar->SetBrush(FSlateRoundedBoxBrush(Accent,2.f));
    auto* BarSize=Tree->ConstructWidget<USizeBox>();
    BarSize->SetWidthOverride(3.f);BarSize->SetHeightOverride(13.f);BarSize->AddChild(Bar);
    auto* BarSlot=Header->AddChildToHorizontalBox(BarSize);
    BarSlot->SetPadding(FMargin(0.f,1.f,8.f,0.f));BarSlot->SetVerticalAlignment(VAlign_Center);
    Header->AddChildToHorizontalBox(MakeText(Tree,Title,13,Accent,true));

    AddDivider(Tree,Body,FMargin(0.f,8.f,0.f,12.f));
    return Body;
}

/** Flat rounded action button; the background tint is the selected/unselected state. */
UButton* ActionButton(UWidgetTree* Tree,const FString& Label,int32 FontSize,const FLinearColor& Color)
{
    auto* Button=Tree->ConstructWidget<UButton>();
    FButtonStyle Style;
    Style.Normal=FSlateRoundedBoxBrush(FLinearColor::White,6.f);
    Style.Hovered=FSlateRoundedBoxBrush(FLinearColor(1.18f,1.18f,1.18f,1.f),6.f);
    Style.Pressed=FSlateRoundedBoxBrush(FLinearColor(.78f,.78f,.78f,1.f),6.f);
    Style.NormalPadding=FMargin(12.f,8.f);
    Style.PressedPadding=FMargin(12.f,8.f);
    Button->SetStyle(Style);
    Button->SetBackgroundColor(Color);
    Button->AddChild(MakeText(Tree,Label,FontSize,FLinearColor(.96f,.98f,1.f),true));
    return Button;
}

UComboBoxString* MakeChoices(UWidgetTree* Tree,const TArray<FString>& Options)
{
    auto* Combo=Tree->ConstructWidget<UComboBoxString>();

    auto Style=Combo->GetWidgetStyle();
    auto& ButtonStyle=Style.ComboButtonStyle.ButtonStyle;
    ButtonStyle.Normal=FSlateRoundedBoxBrush(FieldFill,5.f,FieldOutline,1.f);
    ButtonStyle.Hovered=FSlateRoundedBoxBrush(FieldHover,5.f,FieldHoverOutline,1.f);
    ButtonStyle.Pressed=FSlateRoundedBoxBrush(FieldFill,5.f,FieldHoverOutline,1.f);
    ButtonStyle.NormalPadding=FMargin(2.f);
    ButtonStyle.PressedPadding=FMargin(2.f);
    Style.ContentPadding=FMargin(10.f,5.f);
    Style.MenuRowPadding=FMargin(8.f,5.f);
    Style.ComboButtonStyle.MenuBorderBrush=FSlateRoundedBoxBrush(CardFill,6.f,FieldHoverOutline,1.f);
    Style.ComboButtonStyle.MenuBorderPadding=FMargin(4.f);
    Combo->SetWidgetStyle(Style);

    auto RowStyle=Combo->GetItemStyle();
    RowStyle.TextColor=FSlateColor(FLinearColor(.92f,.96f,1.f));
    RowStyle.SelectedTextColor=FSlateColor(FLinearColor(.55f,.9f,1.f));
    Combo->SetItemStyle(RowStyle);
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    Combo->ForegroundColor=FSlateColor(FLinearColor(.92f,.96f,1.f));
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    for(const auto& Option:Options)Combo->AddOption(Option);
    Combo->SetSelectedIndex(0);return Combo;
}

UComboBoxString* Choices(UWidgetTree* Tree,UVerticalBox* Parent,const TArray<FString>& Options)
{
    auto* Combo=MakeChoices(Tree,Options);
    auto* Slot=Parent->AddChildToVerticalBox(Combo);Slot->SetPadding(FMargin(0,3,0,7));
    return Combo;
}

/** Compact combo used inside a team header row. */
UComboBoxString* HeaderChoices(UWidgetTree* Tree,UHorizontalBox* Parent,const TArray<FString>& Options,float Width)
{
    auto* Combo=MakeChoices(Tree,Options);
    auto* Size=Tree->ConstructWidget<USizeBox>();Size->SetWidthOverride(Width);Size->AddChild(Combo);
    auto* Slot=Parent->AddChildToHorizontalBox(Size);Slot->SetVerticalAlignment(VAlign_Center);
    return Combo;
}
}

TSharedRef<SWidget> UShipSetupWidget::RebuildWidget()
{
    if(!WidgetTree)WidgetTree=NewObject<UWidgetTree>(this);
    auto* Background=WidgetTree->ConstructWidget<UBorder>();Background->SetBrushColor(FLinearColor(.004f,.008f,.018f,.97f));
    WidgetTree->RootWidget=Background;
    auto* Scale=WidgetTree->ConstructWidget<UScaleBox>();Scale->SetStretch(EStretch::ScaleToFit);Background->AddChild(Scale);
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(780);Size->SetHeightOverride(1010);Scale->AddChild(Size);
    auto* Frame=WidgetTree->ConstructWidget<UBorder>();
    Frame->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.012f,.025f,.045f,1.f),14.f,FLinearColor(.12f,.28f,.42f,.5f),1.f));
    Frame->SetPadding(FMargin(26.f,22.f,26.f,22.f));Size->AddChild(Frame);
    auto* Box=WidgetTree->ConstructWidget<UVerticalBox>();Frame->AddChild(Box);

    auto* HeaderRow=WidgetTree->ConstructWidget<UHorizontalBox>();Box->AddChildToVerticalBox(HeaderRow);
    HeaderRow->AddChildToHorizontalBox(MakeText(WidgetTree,TEXT("MATCH SETTINGS"),26,FLinearColor(.86f,.95f,1.f),true));
    auto* CloseHint=WidgetTree->ConstructWidget<UBorder>();
    CloseHint->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.04f,.1f,.16f,1.f),5.f,FLinearColor(.2f,.45f,.6f,.5f),1.f));
    CloseHint->SetPadding(FMargin(9.f,4.f));
    CloseHint->AddChild(MakeText(WidgetTree,TEXT("ESC / P TO CLOSE"),10,FLinearColor(.55f,.72f,.86f),true));
    auto* CloseSlot=HeaderRow->AddChildToHorizontalBox(CloseHint);
    CloseSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    CloseSlot->SetHorizontalAlignment(HAlign_Right);CloseSlot->SetVerticalAlignment(VAlign_Center);
    auto* AccentLine=WidgetTree->ConstructWidget<UBorder>();AccentLine->SetBrush(FSlateRoundedBoxBrush(AccentCyan,1.5f));
    auto* AccentSize=WidgetTree->ConstructWidget<USizeBox>();AccentSize->SetWidthOverride(56.f);AccentSize->SetHeightOverride(3.f);AccentSize->AddChild(AccentLine);
    auto* AccentSlot=Box->AddChildToVerticalBox(AccentSize);AccentSlot->SetPadding(FMargin(1.f,12.f,0.f,14.f));AccentSlot->SetHorizontalAlignment(HAlign_Left);

    // GENERAL: AI difficulty and battle mode.
    auto* General=Section(WidgetTree,Box,TEXT("GENERAL"),AccentCyan);
    auto* GeneralRow=WidgetTree->ConstructWidget<UHorizontalBox>();General->AddChildToVerticalBox(GeneralRow);
    for(int32 I=0;I<2;++I)
    {
        auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();auto* LayoutSlot=GeneralRow->AddChildToHorizontalBox(Column);
        LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LayoutSlot->SetPadding(FMargin(I?10.f:0.f,0.f,I?0.f:10.f,0.f));
        SetupText(WidgetTree,Column,I==0?TEXT("AI DIFFICULTY"):TEXT("BATTLE MODE"),11,TextMuted);
        if(I==0)DifficultySelector=Choices(WidgetTree,Column,{TEXT("Easy"),TEXT("Normal"),TEXT("Hard")});
        else ModeSelector=Choices(WidgetTree,Column,{TEXT("Real-Time"),TEXT("Turn-Based (10s)")});
    }

    // DEPLOY TROOPS: layout source buttons share a row; the seed field is the second row.
    auto* Deploy=Section(WidgetTree,Box,TEXT("DEPLOY TROOPS"),AccentBlue);
    auto* DeployRow=WidgetTree->ConstructWidget<UHorizontalBox>();Deploy->AddChildToVerticalBox(DeployRow);
    for(int32 I=0;I<2;++I)
    {
        auto* Button=ActionButton(WidgetTree,I==0?TEXT("GENERATE FROM SEED"):TEXT("GENERATE MANUALLY (A-B)"),12,FLinearColor(.06f,.11f,.18f));
        auto* LayoutSlot=DeployRow->AddChildToHorizontalBox(Button);
        LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LayoutSlot->SetPadding(FMargin(I?7.f:0.f,0.f,I?0.f:7.f,0.f));
        if(I){UseMyMapButton=Button;Button->OnClicked.AddDynamic(this,&UShipSetupWidget::UseMyMap);}
        else {GenerateFromSeedButton=Button;Button->OnClicked.AddDynamic(this,&UShipSetupWidget::GenerateFromSeed);}
    }
    SeedRow=WidgetTree->ConstructWidget<UVerticalBox>();
    Deploy->AddChildToVerticalBox(SeedRow)->SetPadding(FMargin(0.f,10.f,0.f,0.f));
    auto* SeedHeader=WidgetTree->ConstructWidget<UHorizontalBox>();SeedRow->AddChildToVerticalBox(SeedHeader);
    SeedHeader->AddChildToHorizontalBox(MakeText(WidgetTree,TEXT("BATTLEFIELD SEED"),11,TextMuted));
    auto* SeedHint=MakeText(WidgetTree,TEXT("0 = new random   /   positive = repeatable"),10,TextFaint);
    auto* HintSlot=SeedHeader->AddChildToHorizontalBox(SeedHint);
    HintSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));HintSlot->SetHorizontalAlignment(HAlign_Right);
    SeedInput=WidgetTree->ConstructWidget<UEditableTextBox>();
    auto SeedStyle=SeedInput->WidgetStyle;
    SeedStyle.BackgroundImageNormal=FSlateRoundedBoxBrush(FieldFill,6.f,FieldOutline,1.f);
    SeedStyle.BackgroundImageHovered=FSlateRoundedBoxBrush(FieldHover,6.f,FieldHoverOutline,1.f);
    SeedStyle.BackgroundImageFocused=FSlateRoundedBoxBrush(FLinearColor(.02f,.055f,.09f,1.f),6.f,FLinearColor(.35f,.75f,1.f,.9f),1.f);
    SeedStyle.BackgroundImageReadOnly=SeedStyle.BackgroundImageNormal;
    SeedStyle.Padding=FMargin(10.f,6.f);
    SeedStyle.ForegroundColor=FSlateColor(FLinearColor(.92f,.97f,1.f));
    SeedStyle.BackgroundColor=FSlateColor(FLinearColor::White);
    SeedInput->WidgetStyle=SeedStyle;
    SeedInput->SetText(FText::FromString(TEXT("0")));
    auto* SeedInputSize=WidgetTree->ConstructWidget<USizeBox>();
    SeedInputSize->SetWidthOverride(170.f);SeedInputSize->AddChild(SeedInput);
    auto* SeedInputSlot=SeedRow->AddChildToVerticalBox(SeedInputSize);
    SeedInputSlot->SetPadding(FMargin(0.f,5.f,0.f,0.f));SeedInputSlot->SetHorizontalAlignment(HAlign_Left);
    LayoutDescription=MakeText(WidgetTree,TEXT(""),11,FLinearColor(.55f,.8f,.9f));LayoutDescription->SetAutoWrapText(true);
    Deploy->AddChildToVerticalBox(LayoutDescription)->SetPadding(FMargin(0.f,10.f,0.f,0.f));

    // TEAM SETUP: control and ship mode, then the two fleets.
    auto* Team=Section(WidgetTree,Box,TEXT("TEAM SETUP"),AccentTeam);
    // CONTROL MODE row: player team sits beside it only while a player is present.
    auto* ControlRow=WidgetTree->ConstructWidget<UHorizontalBox>();Team->AddChildToVerticalBox(ControlRow);
    auto* ControlColumn=WidgetTree->ConstructWidget<UVerticalBox>();
    auto* ControlSlot=ControlRow->AddChildToHorizontalBox(ControlColumn);
    ControlSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));ControlSlot->SetPadding(FMargin(0.f,0.f,10.f,0.f));
    SetupText(WidgetTree,ControlColumn,TEXT("CONTROL MODE"),11,TextMuted);
    ControlSelector=Choices(WidgetTree,ControlColumn,{TEXT("Player + AI"),TEXT("Full AI / spectator")});
    PlayerTeamColumn=WidgetTree->ConstructWidget<UVerticalBox>();
    auto* PlayerSlot=ControlRow->AddChildToHorizontalBox(PlayerTeamColumn);
    PlayerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));PlayerSlot->SetPadding(FMargin(10.f,0.f,0.f,0.f));
    SetupText(WidgetTree,PlayerTeamColumn,TEXT("PLAYER TEAM"),11,TextMuted);
    PlayerTeamSelector=Choices(WidgetTree,PlayerTeamColumn,{TEXT("Blue Team"),TEXT("Red Team")});

    // SHIP MODE row.
    auto* BalanceColumn=WidgetTree->ConstructWidget<UVerticalBox>();Team->AddChildToVerticalBox(BalanceColumn);
    SetupText(WidgetTree,BalanceColumn,TEXT("SHIP MODE"),11,TextMuted);
    BalanceSelector=Choices(WidgetTree,BalanceColumn,{TEXT("Standard: 100 HP / 10 damage"),TEXT("Class balance: Small 38/6, others 100/10")});

    auto* Teams=WidgetTree->ConstructWidget<UHorizontalBox>();
    Team->AddChildToVerticalBox(Teams)->SetPadding(FMargin(0.f,6.f,0.f,0.f));
    HullSelectors.Empty();HullSelectors.SetNum(6);
    SlotRows.Empty();SlotRows.SetNum(6);
    SlotLabels.Empty();SlotLabels.SetNum(6);
    for(int32 TeamIndex=0;TeamIndex<2;++TeamIndex)
    {
        const FLinearColor TeamColor=TeamIndex==0?FLinearColor(.2f,.6f,1.f):FLinearColor(1.f,.35f,.3f);
        auto* TeamCard=WidgetTree->ConstructWidget<UBorder>();
        TeamCard->SetBrush(FSlateRoundedBoxBrush(TeamIndex==0?FLinearColor(.03f,.07f,.13f,1.f):FLinearColor(.11f,.04f,.05f,1.f),7.f,
            TeamIndex==0?FLinearColor(.15f,.4f,.65f,.4f):FLinearColor(.55f,.18f,.16f,.4f),1.f));
        TeamCard->SetPadding(FMargin(11.f,9.f,11.f,10.f));
        auto* LayoutSlot=Teams->AddChildToHorizontalBox(TeamCard);
        LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LayoutSlot->SetPadding(FMargin(TeamIndex?7.f:0.f,0.f,TeamIndex?0.f:7.f,0.f));
        auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();TeamCard->AddChild(Column);
        auto* TeamHeader=WidgetTree->ConstructWidget<UHorizontalBox>();Column->AddChildToVerticalBox(TeamHeader)->SetPadding(FMargin(0.f,0.f,0.f,5.f));
        auto* Dot=WidgetTree->ConstructWidget<UBorder>();Dot->SetBrush(FSlateRoundedBoxBrush(TeamColor,4.f));
        auto* DotSize=WidgetTree->ConstructWidget<USizeBox>();DotSize->SetWidthOverride(8.f);DotSize->SetHeightOverride(8.f);DotSize->AddChild(Dot);
        auto* DotSlot=TeamHeader->AddChildToHorizontalBox(DotSize);
        DotSlot->SetPadding(FMargin(0.f,0.f,7.f,0.f));DotSlot->SetVerticalAlignment(VAlign_Center);
        TeamHeader->AddChildToHorizontalBox(MakeText(WidgetTree,TeamIndex==0?TEXT("BLUE TEAM"):TEXT("RED TEAM"),13,TeamColor,true));
        auto* HeaderSpacer=WidgetTree->ConstructWidget<UVerticalBox>();
        TeamHeader->AddChildToHorizontalBox(HeaderSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        auto* Count=HeaderChoices(WidgetTree,TeamHeader,{TEXT("1 ship"),TEXT("2 ships"),TEXT("3 ships")},112.f);
        if(TeamIndex==0)BlueCountSelector=Count;else RedCountSelector=Count;
        for(int32 ShipIndex=0;ShipIndex<3;++ShipIndex)
        {
            auto* Row=WidgetTree->ConstructWidget<UVerticalBox>();Column->AddChildToVerticalBox(Row);
            SlotRows[TeamIndex*3+ShipIndex]=Row;
            SlotLabels[TeamIndex*3+ShipIndex]=SetupText(WidgetTree,Row,TEXT(""),11,FLinearColor(.62f,.75f,.88f));
            HullSelectors[TeamIndex*3+ShipIndex]=Choices(WidgetTree,Row,{TEXT("Small"),TEXT("Large"),TEXT("Medium")});
        }
    }

    // Retain the selector API for existing Blueprint/tools, without displaying the
    // old scenario dropdown. Authored and generated layouts are explicit actions.
    NavigationSelector=WidgetTree->ConstructWidget<UComboBoxString>();
    NavigationSelector->AddOption(TEXT("Generated from seed"));NavigationSelector->AddOption(TEXT("My map / A-B patrol"));
    NavigationSelector->SetSelectedIndex(1);

    Error=MakeText(WidgetTree,TEXT(""),11,FLinearColor(1.f,.42f,.35f));Error->SetAutoWrapText(true);
    Box->AddChildToVerticalBox(Error)->SetPadding(FMargin(2.f,8.f,2.f,0.f));
    auto* Buttons=WidgetTree->ConstructWidget<UHorizontalBox>();Box->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(0.f,10.f,0.f,0.f));
    for(int32 I=0;I<2;++I)
    {
        auto* Button=ActionButton(WidgetTree,I?TEXT("QUIT"):TEXT("START NEW MATCH"),15,
            I?FLinearColor(.08f,.13f,.2f):FLinearColor(.02f,.42f,.55f));
        auto* LayoutSlot=Buttons->AddChildToHorizontalBox(Button);
        LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LayoutSlot->SetPadding(FMargin(I?8.f:0.f,0.f,I?0.f:8.f,0.f));
        if(I){QuitLabel=Cast<UTextBlock>(Button->GetContent());Button->OnClicked.AddDynamic(this,&UShipSetupWidget::OnQuitClicked);}
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
    if(const auto* Battle=GetWorld()->GetSubsystem<UNavalBattleSubsystem>();Battle && Error && !Battle->ConfigurationError.IsEmpty())Error->SetText(FText::FromString(Battle->ConfigurationError));
    RefreshSlots();RefreshLayout();SetKeyboardFocus();
}

void UShipSetupWidget::UseMyMap()
{
    if(NavigationSelector)NavigationSelector->SetSelectedIndex(1);
    RefreshLayout();
}

void UShipSetupWidget::GenerateFromSeed()
{
    if(NavigationSelector)NavigationSelector->SetSelectedIndex(0);
    RefreshLayout();
}

void UShipSetupWidget::RefreshLayout()
{
    LastLayout=NavigationSelector?NavigationSelector->GetSelectedIndex():1;
    const bool Authored=LastLayout==1;
    const FLinearColor Selected(.03f,.42f,.58f),Other(.06f,.11f,.18f);
    if(UseMyMapButton)UseMyMapButton->SetBackgroundColor(Authored?Selected:Other);
    if(GenerateFromSeedButton)GenerateFromSeedButton->SetBackgroundColor(Authored?Other:Selected);
    // The seed is only relevant to generated layouts, so keep the second row hidden
    // until "Generate from seed" is active.
    if(SeedRow)SeedRow->SetVisibility(Authored?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    if(LayoutDescription)LayoutDescription->SetText(FText::FromString(Authored?
        TEXT("MY MAP: place ships and assign A/B in the Editor before Play. Keep those positions; AI goes to A, then B, then repeats. Seed does not move authored ships."):
        TEXT("GENERATED: the seed chooses checked starting positions and routes. Use the same positive seed and fleet to repeat a layout; 0 requests a new seed.")));
}

void UShipSetupWidget::RefreshSlots()
{
    if(!BlueCountSelector || !RedCountSelector || !PlayerTeamSelector || SlotRows.Num()!=6)return;
    LastBlue=BlueCountSelector->GetSelectedIndex()+1;LastRed=RedCountSelector->GetSelectedIndex()+1;
    LastPlayerTeam=PlayerTeamSelector->GetSelectedIndex();
    LastControl=ControlSelector?ControlSelector->GetSelectedIndex():0;
    if(PlayerTeamColumn)PlayerTeamColumn->SetVisibility(LastControl==0?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
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
    if(NavigationSelector && LastLayout!=NavigationSelector->GetSelectedIndex())RefreshLayout();
}

bool UShipSetupWidget::ApplySelection()
{
    auto* Settings=GetGameInstance()->GetSubsystem<UNavalMatchSettings>();if(!Settings || HullSelectors.Num()!=6)return false;
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
        Settings->bEnableSectorTactics=Settings->bUseSectorBattlefield;
        Settings->bEnableRedAssault=Settings->bUseSectorBattlefield;
        Settings->bUseIslandPatrolCircuit=Settings->bUseSectorBattlefield;
        if(Error)Error->SetText(FText::GetEmpty());
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
