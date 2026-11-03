#include "MatchSummaryWidget.h"

#include "../CustomPlayerController.h"
#include "../NavalBattleUI.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    const FLinearColor PanelFillColor(0.015f, 0.030f, 0.055f, 0.94f);
    const FLinearColor PanelOutlineColor(0.16f, 0.56f, 0.76f, 0.55f);
    const FLinearColor AccentColor(0.35f, 0.85f, 1.0f, 1.0f);
    const FLinearColor SummaryValueColor(0.72f, 0.76f, 0.80f, 1.0f);
    const FLinearColor BlueTeamColor(0.28f, 0.60f, 1.0f, 1.0f);
    const FLinearColor RedTeamColor(1.0f, 0.36f, 0.32f, 1.0f);
    const FLinearColor VictoryColor(0.35f, 0.95f, 0.55f, 1.0f);
    const FLinearColor DefeatColor(1.0f, 0.40f, 0.35f, 1.0f);
}

TSharedRef<SWidget> UMatchSummaryWidget::RebuildWidget()
{
    static const FSlateRoundedBoxBrush PanelBrush(PanelFillColor, 16.0f, PanelOutlineColor, 1.5f);
    static const FSlateRoundedBoxBrush SeparatorBrush(FLinearColor(0.20f, 0.55f, 0.70f, 0.45f), 0.5f);

    const FSlateFontInfo SummaryTitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 30.0f);
    const FSlateFontInfo SummaryOutcomeFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16.0f);
    const FSlateFontInfo SummaryHeaderFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11.0f);
    const FSlateFontInfo SummaryRowFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13.0f);
    const FSlateFontInfo SummaryTeamFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 17.0f);
    const FSlateFontInfo SummaryButtonFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 12.0f);

    const auto* Battle = GetWorld() ? GetWorld()->GetSubsystem<UNavalBattleSubsystem>() : nullptr;

    auto MakeTeamColumn = [&](int32 TeamIndex, const FText& TeamTitle, const FLinearColor& TeamColor)
    {
        TSharedRef<SGridPanel> Grid = SNew(SGridPanel)
            .FillColumn(0, 0.0f)
            .FillColumn(1, 1.0f)
            .FillColumn(2, 1.0f)
            .FillColumn(3, 1.0f);

        int32 Row = 0;
        auto AddCell = [&](int32 Column, int32 InRow, const FText& Text, const FSlateFontInfo& Font,
            const FLinearColor& Color, EHorizontalAlignment Align)
        {
            Grid->AddSlot(Column, InRow)
            .Padding(FMargin(Column == 0 ? 0.0f : 6.0f, 4.0f, Column == 3 ? 0.0f : 6.0f, 4.0f))
            .HAlign(Align)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Font(Font)
                .ColorAndOpacity(FSlateColor(Color))
                .Text(Text)
            ];
        };

        AddCell(0, Row, FText::FromString(TEXT("SHIP")), SummaryHeaderFont, FLinearColor::White, HAlign_Left);
        AddCell(1, Row, FText::FromString(TEXT("DAMAGE DEALT")), SummaryHeaderFont, FLinearColor::White, HAlign_Right);
        AddCell(2, Row, FText::FromString(TEXT("DAMAGE TAKEN")), SummaryHeaderFont, FLinearColor::White, HAlign_Right);
        AddCell(3, Row, FText::FromString(TEXT("ELIMINATIONS")), SummaryHeaderFont, FLinearColor::White, HAlign_Right);

        int32 ShipCount = 0;
        if (Battle)
        {
            for (const FNavalShipScore& Score : Battle->GetScoreEntries())
            {
                if (Score.Team != TeamIndex)
                {
                    continue;
                }

                ++Row;
                ++ShipCount;
                const FString ShipName = Score.ShipName.IsEmpty() ? TEXT("Ship") : Score.ShipName;
                AddCell(0, Row, FText::FromString(ShipName), SummaryRowFont, SummaryValueColor, HAlign_Left);
                AddCell(1, Row, FText::FromString(FString::Printf(TEXT("%.0f"), Score.DamageDealt)),
                    SummaryRowFont, SummaryValueColor, HAlign_Right);
                AddCell(2, Row, FText::FromString(FString::Printf(TEXT("%.0f"), Score.DamageTaken)),
                    SummaryRowFont, SummaryValueColor, HAlign_Right);
                AddCell(3, Row, FText::FromString(FString::FromInt(Score.Eliminations)),
                    SummaryRowFont, SummaryValueColor, HAlign_Right);
            }
        }

        if (ShipCount == 0)
        {
            ++Row;
            AddCell(0, Row, FText::FromString(TEXT("No ships")), SummaryRowFont, SummaryValueColor, HAlign_Left);
        }

        return SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(0.0f, 0.0f, 0.0f, 6.0f)
            [
                SNew(STextBlock)
                .Font(SummaryTeamFont)
                .ColorAndOpacity(FSlateColor(TeamColor))
                .Text(TeamTitle)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                Grid
            ];
    };

    return SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .Padding(FMargin(0.0f))
            .BorderBackgroundColor(FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.62f)))
        ]
        + SOverlay::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
            .WidthOverride(1280.0f)
            [
                SNew(SBorder)
                .Padding(FMargin(30.0f, 24.0f))
                .BorderImage(&PanelBrush)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [
                        SNew(STextBlock)
                        .Font(SummaryTitleFont)
                        .ColorAndOpacity(FSlateColor(AccentColor))
                        .Text(FText::FromString(TEXT("MATCH SUMMARY")))
                    ]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 2, 0, 0)
                    [
                        SNew(STextBlock)
                        .Font(SummaryOutcomeFont)
                        .ColorAndOpacity_Lambda([this]() { return FSlateColor(GetOutcomeColor()); })
                        .Text_UObject(this, &UMatchSummaryWidget::GetMatchResultText)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 10)
                    [
                        SNew(SBox)
                        .HeightOverride(1.0f)
                        [
                            SNew(SImage)
                            .Image(&SeparatorBrush)
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .Padding(0.0f, 0.0f, 10.0f, 0.0f)
                        [
                            MakeTeamColumn(0, FText::FromString(TEXT("BLUE TEAM")), BlueTeamColor)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Fill)
                        [
                            SNew(SBox)
                            .WidthOverride(1.0f)
                            [
                                SNew(SImage)
                                .Image(&SeparatorBrush)
                            ]
                        ]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .Padding(10.0f, 0.0f, 0.0f, 0.0f)
                        [
                            MakeTeamColumn(1, FText::FromString(TEXT("RED TEAM")), RedTeamColor)
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 22, 0, 0)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
                        [
                            SNew(SButton)
                            .ContentPadding(FMargin(18.0f, 10.0f))
                            .ButtonColorAndOpacity(FLinearColor(0.24f, 0.46f, 0.66f))
                            .OnClicked_UObject(this, &UMatchSummaryWidget::ReturnToSetup)
                            [
                                SNew(STextBlock)
                                .Font(SummaryButtonFont)
                                .Text(FText::FromString(TEXT("RETURN TO MAP SETTING")))
                            ]
                        ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
                        [
                            SNew(SButton)
                            .ContentPadding(FMargin(18.0f, 10.0f))
                            .ButtonColorAndOpacity(FLinearColor(0.24f, 0.46f, 0.66f))
                            .OnClicked_UObject(this, &UMatchSummaryWidget::RestartMatch)
                            [
                                SNew(STextBlock)
                                .Font(SummaryButtonFont)
                                .Text(FText::FromString(TEXT("RESTART")))
                            ]
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SButton)
                            .ContentPadding(FMargin(18.0f, 10.0f))
                            .ButtonColorAndOpacity(FLinearColor(0.24f, 0.46f, 0.66f))
                            .OnClicked_UObject(this, &UMatchSummaryWidget::QuitMatch)
                            [
                                SNew(STextBlock)
                                .Font(SummaryButtonFont)
                                .Text(FText::FromString(TEXT("QUIT")))
                            ]
                        ]
                    ]
                ]
            ]
        ];
}

FText UMatchSummaryWidget::GetMatchResultText() const
{
    const auto* Battle = GetWorld() ? GetWorld()->GetSubsystem<UNavalBattleSubsystem>() : nullptr;
    return Battle ? FText::FromString(Battle->Outcome) : FText::GetEmpty();
}

FLinearColor UMatchSummaryWidget::GetOutcomeColor() const
{
    const auto* Battle = GetWorld() ? GetWorld()->GetSubsystem<UNavalBattleSubsystem>() : nullptr;
    if (!Battle)
    {
        return AccentColor;
    }
    if (Battle->Outcome == TEXT("VICTORY"))
    {
        return VictoryColor;
    }
    if (Battle->Outcome == TEXT("DEFEAT"))
    {
        return DefeatColor;
    }
    if (Battle->Outcome == TEXT("BLUE WINS"))
    {
        return BlueTeamColor;
    }
    if (Battle->Outcome == TEXT("RED WINS"))
    {
        return RedTeamColor;
    }
    return AccentColor;
}

FReply UMatchSummaryWidget::ReturnToSetup()
{
    if (auto* Controller = Cast<ACustomPlayerController>(GetOwningPlayer()))
    {
        Controller->ReturnToMatchSetup();
    }
    return FReply::Handled();
}

FReply UMatchSummaryWidget::RestartMatch()
{
    if (auto* Controller = Cast<ACustomPlayerController>(GetOwningPlayer()))
    {
        Controller->RestartMatch();
    }
    return FReply::Handled();
}

FReply UMatchSummaryWidget::QuitMatch()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
    return FReply::Handled();
}
