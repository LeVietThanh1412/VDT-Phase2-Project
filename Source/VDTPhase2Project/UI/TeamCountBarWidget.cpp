#include "TeamCountBarWidget.h"

#include "../NavalBattleUI.h"
#include "Engine/World.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    /** Backdrop for the score bar: blue fades out to the left, red fades out to the right. */
    class STeamCountBar : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(STeamCountBar) {}
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs) {}

        virtual FVector2D ComputeDesiredSize(float) const override
        {
            return FVector2D(560.0f, 64.0f);
        }

        virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
            FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
        {
            const FVector2D Size = AllottedGeometry.GetLocalSize();
            const float HalfWidth = static_cast<float>(Size.X) * 0.5f;
            const float Height = static_cast<float>(Size.Y);
            const FLinearColor Blue(0.16f, 0.48f, 1.0f, 1.0f);
            const FLinearColor Red(1.0f, 0.22f, 0.18f, 1.0f);

            // Slate names the axis by its gradient direction: Orient_Vertical runs
            // left-to-right along X, which is what a horizontal fade needs.
            TArray<FSlateGradientStop> Stops;
            Stops.Reserve(2);
            Stops.Emplace(FVector2f(0.0f, 0.0f), FLinearColor(Blue.R, Blue.G, Blue.B, 0.0f));
            Stops.Emplace(FVector2f(HalfWidth, 0.0f), FLinearColor(Blue.R, Blue.G, Blue.B, 0.85f));
            FSlateDrawElement::MakeGradient(OutDrawElements, LayerId,
                AllottedGeometry.ToPaintGeometry(FVector2f(HalfWidth, Height), FSlateLayoutTransform(FVector2f(0.0f, 0.0f))),
                Stops, Orient_Vertical, ESlateDrawEffect::None);

            Stops.Reset();
            Stops.Emplace(FVector2f(0.0f, 0.0f), FLinearColor(Red.R, Red.G, Red.B, 0.85f));
            Stops.Emplace(FVector2f(HalfWidth, 0.0f), FLinearColor(Red.R, Red.G, Red.B, 0.0f));
            FSlateDrawElement::MakeGradient(OutDrawElements, LayerId,
                AllottedGeometry.ToPaintGeometry(FVector2f(HalfWidth, Height), FSlateLayoutTransform(FVector2f(HalfWidth, 0.0f))),
                Stops, Orient_Vertical, ESlateDrawEffect::None);
            return LayerId;
        }
    };
}

TSharedRef<SWidget> UTeamCountBarWidget::RebuildWidget()
{
    const FSlateFontInfo CountFont = FCoreStyle::GetDefaultFontStyle(
        TEXT("Bold"), 30.0f, FFontOutlineSettings(1, FLinearColor(0.01f, 0.05f, 0.10f, 0.9f)));
    const FLinearColor TeamBlueColor(0.55f, 0.82f, 1.0f, 1.0f);
    const FLinearColor TeamRedColor(1.0f, 0.58f, 0.52f, 1.0f);

    return SNew(SOverlay)
        + SOverlay::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Top)
        .Padding(FMargin(0.0f, 22.0f, 0.0f, 0.0f))
        [
            SNew(SBox)
            .WidthOverride(560.0f)
            .HeightOverride(64.0f)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    SNew(STeamCountBar)
                ]
                + SOverlay::Slot()
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Center)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .HAlign(HAlign_Right)
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, 26.0f, 0.0f))
                    [
                        SNew(STextBlock)
                        .Font(CountFont)
                        .ColorAndOpacity(FSlateColor(TeamBlueColor))
                        .ShadowOffset(FVector2D(0.0f, 1.0f))
                        .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f))
                        .Text_UObject(this, &UTeamCountBarWidget::GetBlueCountText)
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .HAlign(HAlign_Left)
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(26.0f, 0.0f, 0.0f, 0.0f))
                    [
                        SNew(STextBlock)
                        .Font(CountFont)
                        .ColorAndOpacity(FSlateColor(TeamRedColor))
                        .ShadowOffset(FVector2D(0.0f, 1.0f))
                        .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f))
                        .Text_UObject(this, &UTeamCountBarWidget::GetRedCountText)
                    ]
                ]
            ]
        ];
}

FText UTeamCountBarWidget::GetBlueCountText() const
{
    const auto* Battle = GetWorld() ? GetWorld()->GetSubsystem<UNavalBattleSubsystem>() : nullptr;
    return FText::FromString(Battle ? FString::FromInt(Battle->BlueAlive) : TEXT("--"));
}

FText UTeamCountBarWidget::GetRedCountText() const
{
    const auto* Battle = GetWorld() ? GetWorld()->GetSubsystem<UNavalBattleSubsystem>() : nullptr;
    return FText::FromString(Battle ? FString::FromInt(Battle->RedAlive) : TEXT("--"));
}
