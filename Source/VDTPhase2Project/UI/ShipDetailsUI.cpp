#include "ShipDetailsUI.h"

#include "../CustomPlayerController.h"
#include "CesiumGeoreference.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    bool TryGetNumericProperty(const UObject* Object, std::initializer_list<FName> PropertyNames, double& OutValue)
    {
        if (!IsValid(Object))
        {
            return false;
        }

        for (const FName PropertyName : PropertyNames)
        {
            FNumericProperty* NumericProperty = FindFProperty<FNumericProperty>(Object->GetClass(), PropertyName);
            if (!NumericProperty)
            {
                continue;
            }

            const void* ValueAddress = NumericProperty->ContainerPtrToValuePtr<void>(Object);
            OutValue = NumericProperty->IsFloatingPoint()
                ? NumericProperty->GetFloatingPointPropertyValue(ValueAddress)
                : static_cast<double>(NumericProperty->GetSignedIntPropertyValue(ValueAddress));
            return true;
        }

        return false;
    }

    const TCHAR* GetCardinalDirection(float HeadingDegrees)
    {
        static constexpr const TCHAR* Directions[] = { TEXT("N"), TEXT("E"), TEXT("S"), TEXT("W") };
        const int32 DirectionIndex = FMath::RoundToInt(FRotator::NormalizeAxis(HeadingDegrees) / 90.0f) & 3;
        return Directions[DirectionIndex];
    }
}

void UShipDetailsUI::SetSelectedShip(APawn* InSelectedShip)
{
    SelectedShip = InSelectedShip;
}

TSharedRef<SWidget> UShipDetailsUI::RebuildWidget()
{
    const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 20);
    const FSlateFontInfo DetailFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16);

    return SNew(SOverlay)
        + SOverlay::Slot()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Bottom)
        .Padding(FMargin(32.0f))
        [
            SNew(SBorder)
            .Padding(FMargin(18.0f))
            .BorderBackgroundColor(FLinearColor(0.015f, 0.035f, 0.065f, 0.90f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, 5.0f)
                [
                    SAssignNew(ShipNameText, STextBlock)
                    .Font(TitleFont)
                    .ColorAndOpacity(FLinearColor(0.30f, 0.85f, 1.0f))
                    .Text(FText::FromString(TEXT("SHIP DETAILS")))
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SAssignNew(LongitudeText, STextBlock).Font(DetailFont)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SAssignNew(LatitudeText, STextBlock).Font(DetailFont)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SAssignNew(SpeedText, STextBlock).Font(DetailFont)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SAssignNew(HealthText, STextBlock).Font(DetailFont)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SAssignNew(DirectionText, STextBlock).Font(DetailFont)
                ]
            ]
        ];
}

void UShipDetailsUI::NativeConstruct()
{
    Super::NativeConstruct();
    UpdateDetails();
}

void UShipDetailsUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UpdateDetails();
}

APawn* UShipDetailsUI::ResolveSelectedShip() const
{
    if (const ACustomPlayerController* Controller = Cast<ACustomPlayerController>(GetOwningPlayer()))
    {
        return Controller->GetSelectedShip();
    }

    return SelectedShip.Get();
}

void UShipDetailsUI::UpdateDetails()
{
    if (!ShipNameText.IsValid())
    {
        return;
    }

    APawn* Ship = ResolveSelectedShip();
    if (!IsValid(Ship))
    {
        ShipNameText->SetText(FText::FromString(TEXT("SHIP DETAILS")));
        LongitudeText->SetText(FText::FromString(TEXT("Longitude: --")));
        LatitudeText->SetText(FText::FromString(TEXT("Latitude: --")));
        SpeedText->SetText(FText::FromString(TEXT("Speed: --")));
        HealthText->SetText(FText::FromString(TEXT("Health: --")));
        DirectionText->SetText(FText::FromString(TEXT("Direction: --")));
        return;
    }

    ShipNameText->SetText(FText::FromString(FString::Printf(TEXT("SHIP DETAILS: %s"), *Ship->GetActorNameOrLabel())));

    FVector GeographicCoordinates = Ship->GetActorLocation();
    if (UWorld* World = Ship->GetWorld())
    {
        for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
        {
            GeographicCoordinates = It->TransformUnrealPositionToLongitudeLatitudeHeight(Ship->GetActorLocation());
            break;
        }
    }

    const double SpeedKnots = Ship->GetVelocity().Size() * 0.0194384449;
    double HealthValue = 0.0;
    const bool bHasHealth = TryGetNumericProperty(
        Ship,
        { TEXT("Health"), TEXT("CurrentHealth"), TEXT("HullHealth"), TEXT("HP"), TEXT("HitPoints") },
        HealthValue);

    const float HeadingDegrees = FRotator::NormalizeAxis(Ship->GetActorRotation().Yaw);
    LongitudeText->SetText(FText::FromString(FString::Printf(TEXT("Longitude: %.6f"), GeographicCoordinates.X)));
    LatitudeText->SetText(FText::FromString(FString::Printf(TEXT("Latitude: %.6f"), GeographicCoordinates.Y)));
    SpeedText->SetText(FText::FromString(FString::Printf(TEXT("Speed: %.1f knots"), SpeedKnots)));
    HealthText->SetText(bHasHealth
        ? FText::FromString(FString::Printf(TEXT("Health: %.0f"), HealthValue))
        : FText::FromString(TEXT("Health: --")));
    DirectionText->SetText(FText::FromString(FString::Printf(
        TEXT("Direction: %.0f deg %s"),
        HeadingDegrees,
        GetCardinalDirection(HeadingDegrees))));
}
