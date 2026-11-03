#include "ShipDetailsUI.h"

#include "../CustomPlayerController.h"
#include "../ShipCombatComponent.h"
#include "../ShipMovementComponent.h"
#include "../NavalBattleUI.h"
#include "../NavalBattlefieldSubsystem.h"
#include "Engine/GameInstance.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "CesiumGeoreference.h"
#include "EngineUtils.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
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
    const FLinearColor LabelColor(0.55f, 0.70f, 0.80f, 1.0f);
    const FLinearColor ValueColor(0.93f, 0.97f, 1.0f, 1.0f);
    const FLinearColor SummaryValueColor(0.72f, 0.76f, 0.80f, 1.0f);
    const FLinearColor BlueTeamColor(0.28f, 0.60f, 1.0f, 1.0f);
    const FLinearColor RedTeamColor(1.0f, 0.36f, 0.32f, 1.0f);
    const FLinearColor VictoryColor(0.35f, 0.95f, 0.55f, 1.0f);
    const FLinearColor DefeatColor(1.0f, 0.40f, 0.35f, 1.0f);

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
            .FillColumn(0, 1.0f)
            .FillColumn(1, 0.0f)
            .FillColumn(2, 0.0f)
            .FillColumn(3, 0.0f);

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
                    + SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)
                    [ SAssignNew(MagazineText,STextBlock).Font(ValueFont).ColorAndOpacity(AccentColor) ]
                ]
            ]
        ]
        + SOverlay::Slot()
        [
            SNew(SOverlay)
            .Visibility_UObject(this,&UShipDetailsUI::GetMatchResultVisibility)
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
                            .Text_UObject(this,&UShipDetailsUI::GetMatchResultText)
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
                                .OnClicked_UObject(this,&UShipDetailsUI::ReturnToSetup)
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
                                .OnClicked_UObject(this,&UShipDetailsUI::RestartMatch)
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
                                .OnClicked_UObject(this,&UShipDetailsUI::QuitMatch)
                                [
                                    SNew(STextBlock)
                                    .Font(SummaryButtonFont)
                                    .Text(FText::FromString(TEXT("QUIT")))
                                ]
                            ]
                        ]
                    ]
                ]
            ]
        ];
}

EVisibility UShipDetailsUI::GetMatchResultVisibility() const
{
    const auto* Battle=GetWorld()?GetWorld()->GetSubsystem<UNavalBattleSubsystem>():nullptr;
    return Battle && !Battle->Outcome.IsEmpty()?EVisibility::Visible:EVisibility::Collapsed;
}

FText UShipDetailsUI::GetMatchResultText() const
{
    const auto* Battle=GetWorld()?GetWorld()->GetSubsystem<UNavalBattleSubsystem>():nullptr;
    return Battle?FText::FromString(Battle->Outcome):FText::GetEmpty();
}

FLinearColor UShipDetailsUI::GetOutcomeColor() const
{
    const auto* Battle=GetWorld()?GetWorld()->GetSubsystem<UNavalBattleSubsystem>():nullptr;
    if(!Battle)return AccentColor;
    if(Battle->Outcome==TEXT("VICTORY"))return VictoryColor;
    if(Battle->Outcome==TEXT("DEFEAT"))return DefeatColor;
    if(Battle->Outcome==TEXT("BLUE WINS"))return BlueTeamColor;
    if(Battle->Outcome==TEXT("RED WINS"))return RedTeamColor;
    return AccentColor;
}

FReply UShipDetailsUI::ReturnToSetup()
{
    SetVisibility(ESlateVisibility::Collapsed);
    if(auto* Controller=Cast<ACustomPlayerController>(GetOwningPlayer()))Controller->ReturnToMatchSetup();
    return FReply::Handled();
}

FReply UShipDetailsUI::RestartMatch()
{
    if(auto* Controller=Cast<ACustomPlayerController>(GetOwningPlayer()))Controller->RestartMatch();
    return FReply::Handled();
}

FReply UShipDetailsUI::QuitMatch()
{
    UKismetSystemLibrary::QuitGame(this,GetOwningPlayer(),EQuitPreference::Quit,false);
    return FReply::Handled();
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

    // Match state remains visible even after the selected ship/wreck is gone.
    if(const auto* Battle=GetWorld()?GetWorld()->GetSubsystem<UNavalBattleSubsystem>():nullptr)
    {
        BattleText=FString::Printf(TEXT("BLUE %d  |  RED %d"),Battle->BlueAlive,Battle->RedAlive);
        if(const auto* Settings=GetWorld()->GetGameInstance()->GetSubsystem<UNavalMatchSettings>();Settings && Settings->bFullAIDemo)
            BattleText=TEXT("FULL AI  |  ")+BattleText;
        if(!Battle->Outcome.IsEmpty())BattleText+=TEXT(" | ")+Battle->Outcome;
        else if(const auto* Field=GetWorld()->GetSubsystem<UNavalBattlefieldSubsystem>();Field && Field->IsEnabled() && !Field->bReady)
            BattleText+=Field->InitializationError.IsEmpty()?TEXT(" | PREPARING BATTLEFIELD"):TEXT(" | SETUP FAILED - press P");
        else if(Battle->BattleMode==ENavalBattleMode::TurnBased)
            BattleText+=FString::Printf(TEXT(" | %s %.0fs"),*Battle->ActiveTurnLabel,Battle->TurnSecondsRemaining());
    }
    APawn* Ship = ResolveSelectedShip();DisplayedShip=Ship;
    if(IsValid(Ship))if(const auto* Move=Ship->FindComponentByClass<UShipMovementComponent>();Move && Move->bShorelineBlocked)
    {BattleText+=TEXT(" | SHALLOW WATER AHEAD - turn or reverse");}
    if (!IsValid(Ship))
    {
        ShipNameText->SetText(FText::FromString(TEXT("--")));
        ShipNameText->SetColorAndOpacity(FSlateColor(AccentColor));
        AmmoText=IdentityText=TEXT("--");MagazineText->SetText(FText::FromString(AmmoText));
        LongitudeText->SetText(FText::FromString(TEXT("--")));
        LatitudeText->SetText(FText::FromString(TEXT("--")));
        SpeedText->SetText(FText::FromString(TEXT("--")));
        HealthText->SetText(FText::FromString(TEXT("--")));
        DirectionText->SetText(FText::FromString(TEXT("--")));
        return;
    }

    if(const auto* Weapon=Ship->FindComponentByClass<UShipWeaponComponent>())
    {
        const int32 Seconds=FMath::CeilToInt(Weapon->ReloadRemaining());
        AmmoText=Weapon->bIsDestroyed?TEXT("WEAPON OFFLINE"):Weapon->bReloading?
            FString::Printf(TEXT("AMMO: %d/%d  |  RELOADING: %02d:%02d"),Weapon->RoundsRemaining,Weapon->MagazineCapacity,Seconds/60,Seconds%60):
            FString::Printf(TEXT("AMMO: %d/%d"),Weapon->RoundsRemaining,Weapon->MagazineCapacity);
        MagazineText->SetText(FText::FromString(AmmoText));
        IdentityText=Weapon->Team()==0?TEXT("BLUE"):Weapon->Team()==1?TEXT("RED"):TEXT("NEUTRAL");
        for(const auto& Tag:Ship->Tags)
        {
            const FString Value=Tag.ToString();
            if(Value.StartsWith(TEXT("AI.Role.")))IdentityText+=TEXT(" | ")+Value.Mid(8);
            if(Value.StartsWith(TEXT("AI.State.")))IdentityText+=TEXT(" | ")+Value.Mid(9);
        }
    }
    // Show the same designer-authored identity as the minimap instead of the
    // long engine object name (for example, "BP_ShipEnemy_C_UAID_...").
    FString DisplayName;
    if (const UShipWeaponComponent* Weapon = Ship->FindComponentByClass<UShipWeaponComponent>())
    {
        DisplayName = Weapon->GetShipName();
    }
    else if (const UShipIdentityComponent* Identity = Ship->FindComponentByClass<UShipIdentityComponent>())
    {
        DisplayName = Identity->GetDisplayName();
    }
    else
    {
        DisplayName = MakeShipDisplayName(Ship->GetName());
    }
    FLinearColor NameColor = AccentColor;
    if (const UShipWeaponComponent* Weapon = Ship->FindComponentByClass<UShipWeaponComponent>())
    {
        if (Weapon->Team() == 0)
        {
            NameColor = FLinearColor(0.32f, 0.66f, 1.0f, 1.0f);
        }
        else if (Weapon->Team() == 1)
        {
            NameColor = FLinearColor(1.0f, 0.32f, 0.28f, 1.0f);
        }
    }
    ShipNameText->SetColorAndOpacity(FSlateColor(NameColor));
    ShipNameText->SetText(FText::FromString(DisplayName));

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

    // Hull health lives on the shared weapon component, which is the final damage
    // authority for both the player ship and AI ships. Keep the actor-property
    // fallback for blueprints that have not installed that component yet.
    double HealthValue = 0.0;
    double MaximumHealthValue = 0.0;
    bool bHasHealth = false;
    if (const UShipWeaponComponent* Weapon = Ship->FindComponentByClass<UShipWeaponComponent>())
    {
        HealthValue = Weapon->CurrentHealth;
        MaximumHealthValue = Weapon->MaxHealth;
        bHasHealth = true;
    }
    else
    {
        bHasHealth = TryGetNumericProperty(
            Ship,
            { TEXT("Health"), TEXT("CurrentHealth"), TEXT("HullHealth"), TEXT("HP"), TEXT("HitPoints") },
            HealthValue);
        TryGetNumericProperty(
            Ship,
            { TEXT("MaxHealth"), TEXT("MaximumHealth"), TEXT("MaxHP"), TEXT("MaxHitPoints") },
            MaximumHealthValue);
    }

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
    if (bHasHealth)
    {
        HealthText->SetText(FText::FromString(MaximumHealthValue > 0.0
            ? FString::Printf(TEXT("%.0f / %.0f"), HealthValue, MaximumHealthValue)
            : FString::Printf(TEXT("%.0f"), HealthValue)));

        const double HealthFraction = MaximumHealthValue > 0.0 ? HealthValue / MaximumHealthValue : 1.0;
        const FLinearColor HealthColor = HealthFraction <= 0.3
            ? FLinearColor(1.0f, 0.36f, 0.30f, 1.0f)
            : HealthFraction <= 0.6
                ? FLinearColor(1.0f, 0.78f, 0.30f, 1.0f)
                : ValueColor;
        HealthText->SetColorAndOpacity(FSlateColor(HealthColor));
    }
    else
    {
        HealthText->SetText(FText::FromString(TEXT("--")));
        HealthText->SetColorAndOpacity(FSlateColor(ValueColor));
    }
    DirectionText->SetText(FText::FromString(FString::Printf(
        TEXT("%d\u00B0 (%s)"),
        AzimuthRounded,
        GetCardinalDirection(static_cast<float>(AzimuthDegrees)))));
}
