#include "LoadingScreenWidget.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    const FLinearColor BackgroundColor(0.004f, 0.008f, 0.018f, 1.0f);
    const FLinearColor TitleColor(0.86f, 0.95f, 1.0f, 1.0f);
    const FLinearColor AccentColor(0.35f, 0.82f, 1.0f, 1.0f);
    const FLinearColor LoadingColor(0.42f, 0.55f, 0.68f, 1.0f);
}

TSharedRef<SWidget> ULoadingScreenWidget::RebuildWidget()
{
    const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 30.0f);
    const FSlateFontInfo LoadingFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 12.0f);

    return SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .Padding(FMargin(0.0f))
            .BorderBackgroundColor(FSlateColor(BackgroundColor))
        ]
        + SOverlay::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            [
                SNew(STextBlock)
                .Font(TitleFont)
                .ColorAndOpacity(FSlateColor(TitleColor))
                .Text(FText::FromString(TEXT("PREPARING BATTLEFIELD")))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(FMargin(0.0f, 14.0f, 0.0f, 22.0f))
            [
                SNew(SBox)
                .WidthOverride(56.0f)
                .HeightOverride(3.0f)
                [
                    SNew(SBorder)
                    .Padding(FMargin(0.0f))
                    .BorderBackgroundColor(FSlateColor(AccentColor))
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            [
                SNew(STextBlock)
                .Font(LoadingFont)
                .ColorAndOpacity(FSlateColor(LoadingColor))
                .Text_Lambda([this]() { return GetLoadingText(); })
            ]
        ];
}

FText ULoadingScreenWidget::GetLoadingText() const
{
    const double Now = FSlateApplication::Get().GetCurrentTime();
    const int32 Dots = 1 + (static_cast<int32>(Now * 2.0) % 3);
    FString Text(TEXT("LOADING"));
    for (int32 Index = 0; Index < Dots; ++Index)
    {
        Text.AppendChar(TEXT('.'));
    }
    return FText::FromString(Text);
}
