#include "ShipDetailsUI.h"

#include "../CustomPlayerController.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "CesiumGeoreference.h"
#include "EngineUtils.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/Pawn.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
#include "Widgets/Images/SImage.h"
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
    const FLinearColor LabelColor(0.55f, 0.70f, 0.80f, 1.0f);
    const FLinearColor ValueColor(0.93f, 0.97f, 1.0f, 1.0f);

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

    /** Maps an azimuth (0 deg = North, clockwise, [0, 360)) to an 8-point compass direction. */
    const TCHAR* GetCardinalDirection(float AzimuthDegrees)
    {
        static constexpr const TCHAR* Directions[] = {
            TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"),
            TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW")
        };
        const int32 DirectionIndex = FMath::RoundToInt(AzimuthDegrees / 45.0f) % 8;
        return Directions[DirectionIndex];
    }

    /** Turns engine actor names such as "BP_Ship_C_0" into "ShipC0" (only the "BP" prefix is dropped). */
    FString MakeShipDisplayName(const FString& RawName)
    {
        TArray<FString> Tokens;
        RawName.ParseIntoArray(Tokens, TEXT("_"), true);

        Tokens.RemoveAll([](const FString& Token)
        {
            return Token.Equals(TEXT("BP"), ESearchCase::IgnoreCase);
        });

        return FString::Join(Tokens, TEXT(""));
    }
}

void UShipDetailsUI::SetSelectedShip(APawn* InSelectedShip)
{
    SelectedShip = InSelectedShip;
}

TSharedRef<SWidget> UShipDetailsUI::RebuildWidget()
{
    static const FSlateRoundedBoxBrush PanelBrush(PanelFillColor, 16.0f, PanelOutlineColor, 1.5f);
    static const FSlateRoundedBoxBrush SeparatorBrush(FLinearColor(0.20f, 0.55f, 0.70f, 0.45f), 0.5f);

    // Bold typeface plus a thin 1px outline keeps the ship name visually heavy.
    const FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle(
        TEXT("Bold"),
        22.0f,
        FFontOutlineSettings(1, FLinearColor(0.02f, 0.16f, 0.24f, 1.0f)));
    const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14.0f);
    const FSlateFontInfo ValueFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14.0f);

    return SNew(SOverlay)
        + SOverlay::Slot()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Bottom)
        .Padding(FMargin(32.0f))
        [
            SNew(SBox)
            .MinDesiredWidth(320.0f)
            [
                SNew(SBorder)
                .Padding(FMargin(22.0f, 18.0f))
                .BorderImage(&PanelBrush)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(ShipNameText, STextBlock)
                        .Font(NameFont)
                        .ColorAndOpacity(AccentColor)
                        .Text(FText::FromString(TEXT("--")))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 10.0f, 0.0f, 8.0f)
                    [
                        SNew(SBox)
                        .HeightOverride(1.0f)
                        [
                            SNew(SImage)
                            .Image(&SeparatorBrush)
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SGridPanel)
                        .FillColumn(0, 0.0f)
                        .FillColumn(1, 1.0f)
                        + SGridPanel::Slot(0, 0)
                        .Padding(0.0f, 2.0f, 18.0f, 2.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Font(LabelFont)
                            .ColorAndOpacity(LabelColor)
                            .Text(FText::FromString(TEXT("Longitude")))
                        ]
                        + SGridPanel::Slot(1, 0)
                        .Padding(0.0f, 2.0f)
                        .HAlign(HAlign_Right)
                        .VAlign(VAlign_Center)
                        [
                            SAssignNew(LongitudeText, STextBlock)
                            .Font(ValueFont)
                            .ColorAndOpacity(ValueColor)
                            .Text(FText::FromString(TEXT("--")))
                        ]
                        + SGridPanel::Slot(0, 1)
                        .Padding(0.0f, 2.0f, 18.0f, 2.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Font(LabelFont)
                            .ColorAndOpacity(LabelColor)
                            .Text(FText::FromString(TEXT("Latitude")))
                        ]
                        + SGridPanel::Slot(1, 1)
                        .Padding(0.0f, 2.0f)
                        .HAlign(HAlign_Right)
                        .VAlign(VAlign_Center)
                        [
                            SAssignNew(LatitudeText, STextBlock)
                            .Font(ValueFont)
                            .ColorAndOpacity(ValueColor)
                            .Text(FText::FromString(TEXT("--")))
                        ]
                        + SGridPanel::Slot(0, 2)
                        .Padding(0.0f, 2.0f, 18.0f, 2.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Font(LabelFont)
                            .ColorAndOpacity(LabelColor)
                            .Text(FText::FromString(TEXT("Speed")))
                        ]
                        + SGridPanel::Slot(1, 2)
                        .Padding(0.0f, 2.0f)
                        .HAlign(HAlign_Right)
                        .VAlign(VAlign_Center)
                        [
                            SAssignNew(SpeedText, STextBlock)
                            .Font(ValueFont)
                            .ColorAndOpacity(ValueColor)
                            .Text(FText::FromString(TEXT("--")))
                        ]
                        + SGridPanel::Slot(0, 3)
                        .Padding(0.0f, 2.0f, 18.0f, 2.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Font(LabelFont)
                            .ColorAndOpacity(LabelColor)
                            .Text(FText::FromString(TEXT("Health")))
                        ]
                        + SGridPanel::Slot(1, 3)
                        .Padding(0.0f, 2.0f)
                        .HAlign(HAlign_Right)
                        .VAlign(VAlign_Center)
                        [
                            SAssignNew(HealthText, STextBlock)
                            .Font(ValueFont)
                            .ColorAndOpacity(ValueColor)
                            .Text(FText::FromString(TEXT("--")))
                        ]
                        + SGridPanel::Slot(0, 4)
                        .Padding(0.0f, 2.0f, 18.0f, 2.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Font(LabelFont)
                            .ColorAndOpacity(LabelColor)
                            .Text(FText::FromString(TEXT("Direction")))
                        ]
                        + SGridPanel::Slot(1, 4)
                        .Padding(0.0f, 2.0f)
                        .HAlign(HAlign_Right)
                        .VAlign(VAlign_Center)
                        [
                            SAssignNew(DirectionText, STextBlock)
                            .Font(ValueFont)
                            .ColorAndOpacity(ValueColor)
                            .Text(FText::FromString(TEXT("--")))
                        ]
                    ]
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
        ShipNameText->SetText(FText::FromString(TEXT("--")));
        LongitudeText->SetText(FText::FromString(TEXT("--")));
        LatitudeText->SetText(FText::FromString(TEXT("--")));
        SpeedText->SetText(FText::FromString(TEXT("--")));
        HealthText->SetText(FText::FromString(TEXT("--")));
        DirectionText->SetText(FText::FromString(TEXT("--")));
        return;
    }

    // GetName() (not GetActorNameOrLabel()) keeps the editor label from overriding the
    // engine name, so "BP_Ship_C_0" always resolves to "ShipC0" in both editor and packaged builds.
    ShipNameText->SetText(FText::FromString(MakeShipDisplayName(Ship->GetName())));

    FVector GeographicCoordinates = Ship->GetActorLocation();
    ACesiumGeoreference* Georeference = nullptr;
    if (UWorld* World = Ship->GetWorld())
    {
        for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
        {
            Georeference = *It;
            GeographicCoordinates = Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(Ship->GetActorLocation());
            break;
        }
    }

    // Horizontal (surface) speed only; vertical bobbing along the waves is ignored.
    const double SpeedKnots = Ship->GetVelocity().Size2D() * 0.0194384449;
    double HealthValue = 0.0;
    const bool bHasHealth = TryGetNumericProperty(
        Ship,
        { TEXT("Health"), TEXT("CurrentHealth"), TEXT("HullHealth"), TEXT("HP"), TEXT("HitPoints") },
        HealthValue);

    // Azimuth relative to the map's true directions. Cesium maps the world as East-South-Up,
    // so the georeference's +X is East and its -Y is North. Fall back to the Unreal convention
    // (yaw 0 = +X) when no georeference exists.
    const FVector ShipForward = Ship->GetActorForwardVector();
    double AzimuthDegrees = 0.0;
    if (Georeference)
    {
        const FVector EastDirection = Georeference->GetActorForwardVector();
        const FVector NorthDirection = -Georeference->GetActorRightVector();
        AzimuthDegrees = FMath::RadiansToDegrees(FMath::Atan2(
            ShipForward.Dot(EastDirection),
            ShipForward.Dot(NorthDirection)));
    }
    else
    {
        AzimuthDegrees = FRotator::NormalizeAxis(Ship->GetActorRotation().Yaw);
    }

    if (AzimuthDegrees < 0.0)
    {
        AzimuthDegrees += 360.0;
    }

    const int32 AzimuthRounded = FMath::RoundToInt32(AzimuthDegrees) % 360;
    LongitudeText->SetText(FText::FromString(FString::Printf(TEXT("%.6f"), GeographicCoordinates.X)));
    LatitudeText->SetText(FText::FromString(FString::Printf(TEXT("%.6f"), GeographicCoordinates.Y)));
    SpeedText->SetText(FText::FromString(FString::Printf(TEXT("%.1f knots"), SpeedKnots)));
    HealthText->SetText(bHasHealth
        ? FText::FromString(FString::Printf(TEXT("%.0f"), HealthValue))
        : FText::FromString(TEXT("--")));
    DirectionText->SetText(FText::FromString(FString::Printf(
        TEXT("%d\u00B0 (%s)"),
        AzimuthRounded,
        GetCardinalDirection(static_cast<float>(AzimuthDegrees)))));
}
