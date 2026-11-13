#include "UI/ButtonTutorialsUI.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

namespace {
const FLinearColor TutorialTextColor(1.f, 1.f, 1.f, 1.f);
const FLinearColor TutorialShadowColor(0.f, 0.f, 0.f, 0.9f);

UTextBlock *MakeTutorialText(UWidgetTree *Tree, const FString &Value,
                             const FSlateFontInfo &Font) {
  auto *Text = Tree->ConstructWidget<UTextBlock>();
  Text->SetText(FText::FromString(Value));
  Text->SetFont(Font);
  Text->SetColorAndOpacity(TutorialTextColor);
  Text->SetShadowColorAndOpacity(TutorialShadowColor);
  Text->SetShadowOffset(FVector2D(1.f, 1.f));
  return Text;
}
} // namespace

TSharedRef<SWidget> UButtonTutorialsUI::RebuildWidget() {
  if (!WidgetTree)
    WidgetTree = NewObject<UWidgetTree>(this);

  auto *Root = WidgetTree->ConstructWidget<UOverlay>();
  Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
  WidgetTree->RootWidget = Root;

  auto *Stack = WidgetTree->ConstructWidget<UVerticalBox>();
  Stack->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
  auto *StackSlot = Root->AddChildToOverlay(Stack);
  StackSlot->SetHorizontalAlignment(HAlign_Right);
  StackSlot->SetVerticalAlignment(VAlign_Bottom);
  StackSlot->SetPadding(FMargin(24.f));

  TutorialPanel = WidgetTree->ConstructWidget<UBorder>();
  TutorialPanel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
  TutorialPanel->SetPadding(FMargin(8.f, 6.f));
  TutorialPanel->SetVisibility(ESlateVisibility::Collapsed);
  auto *PanelSlot = Stack->AddChildToVerticalBox(TutorialPanel);
  PanelSlot->SetHorizontalAlignment(HAlign_Right);
  PanelSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

  auto *Guide = WidgetTree->ConstructWidget<UVerticalBox>();
  Guide->SetVisibility(ESlateVisibility::HitTestInvisible);
  TutorialPanel->AddChild(Guide);

  const FSlateFontInfo HeaderFont =
      FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11);
  const FSlateFontInfo KeyFont =
      FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 12);
  const FSlateFontInfo ActionFont =
      FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 12);

  auto *Header = MakeTutorialText(WidgetTree, TEXT("CONTROLS"), HeaderFont);
  Guide->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

  const TCHAR *Entries[][2] = {{TEXT("P"), TEXT("Match settings")},
                               {TEXT("I"), TEXT("Ship details")},
                               {TEXT("M"), TEXT("Minimap Interaction")},
                               {TEXT("F"), TEXT("Bird's eye camera")},
                               {TEXT("Q"), TEXT("Switch team's camera")},
                               {TEXT("Tab"), TEXT("Next teammate")}};
  for (const auto &Entry : Entries) {
    auto *Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    Row->SetVisibility(ESlateVisibility::HitTestInvisible);
    Guide->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 1.f));

    auto *KeyBox = WidgetTree->ConstructWidget<USizeBox>();
    KeyBox->SetWidthOverride(34.f);
    auto *Key = MakeTutorialText(WidgetTree, Entry[0], KeyFont);
    KeyBox->AddChild(Key);
    auto *KeySlot = Row->AddChildToHorizontalBox(KeyBox);
    KeySlot->SetVerticalAlignment(VAlign_Center);
    KeySlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

    auto *Action = MakeTutorialText(WidgetTree, Entry[1], ActionFont);
    auto *ActionSlot = Row->AddChildToHorizontalBox(Action);
    ActionSlot->SetVerticalAlignment(VAlign_Center);
  }

  auto *ButtonBox = WidgetTree->ConstructWidget<USizeBox>();
  ButtonBox->SetWidthOverride(34.f);
  ButtonBox->SetHeightOverride(34.f);
  auto *ButtonSlot = Stack->AddChildToVerticalBox(ButtonBox);
  ButtonSlot->SetHorizontalAlignment(HAlign_Right);

  HelpButton = WidgetTree->ConstructWidget<UButton>();
  HelpButton->SetBackgroundColor(FLinearColor(0.02f, 0.07f, 0.12f, 0.72f));
  HelpButton->SetToolTipText(FText::FromString(TEXT("Controls (?)")));
  HelpButton->OnClicked.AddDynamic(this,
                                   &UButtonTutorialsUI::HandleHelpClicked);
  ButtonBox->AddChild(HelpButton);

  auto *Question = MakeTutorialText(
      WidgetTree, TEXT("?"), FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18));
  Question->SetJustification(ETextJustify::Center);
  HelpButton->AddChild(Question);

  SetVisibility(ESlateVisibility::SelfHitTestInvisible);
  return Super::RebuildWidget();
}

void UButtonTutorialsUI::ToggleTutorial() {
  bTutorialVisible = !bTutorialVisible;
  if (TutorialPanel)
    TutorialPanel->SetVisibility(bTutorialVisible
                                     ? ESlateVisibility::HitTestInvisible
                                     : ESlateVisibility::Collapsed);
}

void UButtonTutorialsUI::HandleHelpClicked() { ToggleTutorial(); }
