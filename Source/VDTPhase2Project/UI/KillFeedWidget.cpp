#include "KillFeedWidget.h"

#include "../ShipCombatComponent.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    constexpr double NotificationLifetime = 5.0;
    constexpr double NotificationFadeSeconds = 0.9;
    constexpr int32 MaxNotifications = 5;
    constexpr float NotificationWidth = 430.0f;
    constexpr double BindIntervalSeconds = 0.5;
    const FLinearColor NeutralTextColor(0.10f, 0.10f, 0.10f, 1.0f);

    FLinearColor TeamColor(int32 Team)
    {
        if (Team == 0)
        {
            return FLinearColor(0.05f, 0.33f, 0.85f, 1.0f);
        }
        if (Team == 1)
        {
            return FLinearColor(0.82f, 0.08f, 0.06f, 1.0f);
        }
        return FLinearColor(0.16f, 0.16f, 0.16f, 1.0f);
    }

    /** One kill-feed line: solid white on the left fading to transparent on the right. */
    class SKillFeedRow : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SKillFeedRow)
            : _AttackerColor(FLinearColor::White)
            , _VictimColor(FLinearColor::White)
            , _bHasAttacker(false)
            , _CreatedAt(0.0)
            , _Lifetime(NotificationLifetime)
            , _FadeSeconds(NotificationFadeSeconds)
        {}
            SLATE_ARGUMENT(FText, AttackerName)
            SLATE_ARGUMENT(FLinearColor, AttackerColor)
            SLATE_ARGUMENT(FText, VictimName)
            SLATE_ARGUMENT(FLinearColor, VictimColor)
            SLATE_ARGUMENT(bool, bHasAttacker)
            SLATE_ARGUMENT(double, CreatedAt)
            SLATE_ARGUMENT(double, Lifetime)
            SLATE_ARGUMENT(double, FadeSeconds)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            AttackerName = InArgs._AttackerName;
            AttackerColor = InArgs._AttackerColor;
            VictimName = InArgs._VictimName;
            VictimColor = InArgs._VictimColor;
            bHasAttacker = InArgs._bHasAttacker;
            CreatedAt = InArgs._CreatedAt;
            Lifetime = InArgs._Lifetime;
            FadeSeconds = InArgs._FadeSeconds;

            const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16.0f);
            TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox);
            if (bHasAttacker)
            {
                Line->AddSlot().AutoWidth()
                [
                    SNew(STextBlock)
                    .Font(Font)
                    .ColorAndOpacity_Lambda([this]() { return FSlateColor(GetFadedColor(AttackerColor)); })
                    .Text(AttackerName)
                ];
                Line->AddSlot().AutoWidth()
                [
                    SNew(STextBlock)
                    .Font(Font)
                    .ColorAndOpacity_Lambda([this]() { return FSlateColor(GetFadedColor(NeutralTextColor)); })
                    .Text(FText::FromString(TEXT(" has eliminated ")))
                ];
            }
            Line->AddSlot().AutoWidth()
            [
                SNew(STextBlock)
                .Font(Font)
                .ColorAndOpacity_Lambda([this]() { return FSlateColor(GetFadedColor(VictimColor)); })
                .Text(VictimName)
            ];
            if (!bHasAttacker)
            {
                Line->AddSlot().AutoWidth()
                [
                    SNew(STextBlock)
                    .Font(Font)
                    .ColorAndOpacity_Lambda([this]() { return FSlateColor(GetFadedColor(NeutralTextColor)); })
                    .Text(FText::FromString(TEXT(" has been eliminated")))
                ];
            }

            ChildSlot
            [
                SNew(SBox)
                .WidthOverride(NotificationWidth)
                .Padding(FMargin(16.0f, 7.0f, 20.0f, 7.0f))
                [
                    Line
                ]
            ];
        }

        virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
            FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
        {
            const float Alpha = GetFadeAlpha();
            const FVector2D Size = AllottedGeometry.GetLocalSize();
            TArray<FSlateGradientStop> Stops;
            Stops.Reserve(2);
            Stops.Emplace(FVector2f(0.0f, 0.0f), FLinearColor(1.0f, 1.0f, 1.0f, 0.92f * Alpha));
            Stops.Emplace(FVector2f(static_cast<float>(Size.X), 0.0f), FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
            FSlateDrawElement::MakeGradient(OutDrawElements, LayerId,
                AllottedGeometry.ToPaintGeometry(FVector2f(Size.X, Size.Y), FSlateLayoutTransform(FVector2f(0.0f, 0.0f))),
                Stops, Orient_Vertical, ESlateDrawEffect::None);

            return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
        }

        float GetFadeAlpha() const
        {
            const double Now = FSlateApplication::Get().GetCurrentTime();
            const double Remaining = CreatedAt + Lifetime - Now;
            if (Remaining <= 0.0)
            {
                return 0.0f;
            }
            if (FadeSeconds <= 0.0)
            {
                return 1.0f;
            }
            return static_cast<float>(FMath::Clamp(Remaining / FadeSeconds, 0.0, 1.0));
        }

        FLinearColor GetFadedColor(const FLinearColor& Base) const
        {
            const float Alpha = GetFadeAlpha();
            return FLinearColor(Base.R, Base.G, Base.B, Base.A * Alpha);
        }

    private:
        FText AttackerName;
        FLinearColor AttackerColor = FLinearColor::White;
        FText VictimName;
        FLinearColor VictimColor = FLinearColor::White;
        bool bHasAttacker = false;
        double CreatedAt = 0.0;
        double Lifetime = NotificationLifetime;
        double FadeSeconds = NotificationFadeSeconds;
    };
}

TSharedRef<SWidget> UKillFeedWidget::RebuildWidget()
{
    return SNew(SOverlay)
        + SOverlay::Slot()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Top)
        .Padding(FMargin(32.0f, 24.0f, 0.0f, 0.0f))
        [
            SAssignNew(FeedBox, SVerticalBox)
        ];
}

void UKillFeedWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BindShips();
}

void UKillFeedWidget::NativeDestruct()
{
    for (const TWeakObjectPtr<UShipWeaponComponent>& Entry : BoundShips)
    {
        if (UShipWeaponComponent* Ship = Entry.Get())
        {
            Ship->OnShipDestroyed.RemoveDynamic(this, &UKillFeedWidget::HandleShipDestroyed);
        }
    }
    BoundShips.Empty();
    Entries.Empty();
    FeedBox.Reset();
    Super::NativeDestruct();
}

void UKillFeedWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    const double Now = FSlateApplication::Get().GetCurrentTime();
    if (Now >= NextBindTime)
    {
        BindShips();
        NextBindTime = Now + BindIntervalSeconds;
    }

    for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
    {
        if (Now < Entries[Index].ExpiresAt)
        {
            continue;
        }
        if (FeedBox.IsValid() && Entries[Index].Row.IsValid())
        {
            FeedBox->RemoveSlot(Entries[Index].Row.ToSharedRef());
        }
        Entries.RemoveAt(Index);
    }
}

void UKillFeedWidget::BindShips()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (int32 Index = BoundShips.Num() - 1; Index >= 0; --Index)
    {
        if (!BoundShips[Index].IsValid())
        {
            BoundShips.RemoveAt(Index);
        }
    }

    for (TActorIterator<APawn> It(World); It; ++It)
    {
        UShipWeaponComponent* Ship = It->FindComponentByClass<UShipWeaponComponent>();
        if (!Ship || Ship->bIsDestroyed || BoundShips.Contains(Ship))
        {
            continue;
        }
        Ship->OnShipDestroyed.AddUniqueDynamic(this, &UKillFeedWidget::HandleShipDestroyed);
        BoundShips.Add(Ship);
    }
}

void UKillFeedWidget::HandleShipDestroyed(UShipWeaponComponent* Ship, AActor* Attacker)
{
    if (!Ship)
    {
        return;
    }

    const UShipWeaponComponent* AttackerWeapon = Attacker ? Attacker->FindComponentByClass<UShipWeaponComponent>() : nullptr;
    const FString VictimName = Ship->GetShipName();
    const FString AttackerName = AttackerWeapon ? AttackerWeapon->GetShipName() : (Attacker ? Attacker->GetActorNameOrLabel() : FString());
    const bool bHasAttacker = !AttackerName.IsEmpty() && Attacker && Attacker != Ship->GetOwner();

    const FLinearColor AttackerColor = TeamColor(AttackerWeapon ? AttackerWeapon->Team() : Ship->Team());
    const FLinearColor VictimColor = TeamColor(Ship->Team());

    AddNotification(FText::FromString(AttackerName), AttackerColor,
        FText::FromString(VictimName), VictimColor, bHasAttacker);
}

void UKillFeedWidget::AddNotification(const FText& AttackerName, const FLinearColor& AttackerColor,
    const FText& VictimName, const FLinearColor& VictimColor, bool bHasAttacker)
{
    if (!FeedBox.IsValid())
    {
        return;
    }

    const double Now = FSlateApplication::Get().GetCurrentTime();
    TSharedRef<SWidget> Row = SNew(SKillFeedRow)
        .AttackerName(AttackerName)
        .AttackerColor(AttackerColor)
        .VictimName(VictimName)
        .VictimColor(VictimColor)
        .bHasAttacker(bHasAttacker)
        .CreatedAt(Now)
        .Lifetime(NotificationLifetime)
        .FadeSeconds(NotificationFadeSeconds);

    FeedBox->InsertSlot(0)[Row];
    Entries.Insert(FEntry{Row, Now + NotificationLifetime}, 0);

    while (Entries.Num() > MaxNotifications)
    {
        if (Entries.Last().Row.IsValid())
        {
            FeedBox->RemoveSlot(Entries.Last().Row.ToSharedRef());
        }
        Entries.Pop();
    }
}
