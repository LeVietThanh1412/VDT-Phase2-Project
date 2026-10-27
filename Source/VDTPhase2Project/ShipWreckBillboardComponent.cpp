#include "ShipWreckBillboardComponent.h"
#include "ShipCombatComponent.h"
#include "ShipMovementComponent.h"
#include "CesiumGlobeAnchorComponent.h"
#include "CesiumGeoreference.h"
#include "Materials/MaterialInstanceDynamic.h"

UShipWreckBillboardComponent::UShipWreckBillboardComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.bStartWithTickEnabled=false;
    PrimaryComponentTick.TickInterval=.1f;
    PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
    bTickInEditor=false;
    SetAbsolute(false,true,true);
    SetMobility(EComponentMobility::Movable);
    SetCanEverAffectNavigation(false);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCastShadow(false);
    ComponentTags.AddUnique(TEXT("NavalWreckFX"));
    SetVisibility(false);
    SetHiddenInGame(true);
}

void UShipWreckBillboardComponent::BeginPlay()
{
    Super::BeginPlay();
    HideEffect();
    Weapon=GetOwner()->FindComponentByClass<UShipWeaponComponent>();
    if(auto* W=Weapon.Get())
    {
        W->OnShipDestroyed.AddUniqueDynamic(this,&UShipWreckBillboardComponent::HandleDestroyed);
        if(W->bIsDestroyed)ActivateForWreck();
    }
}

void UShipWreckBillboardComponent::HandleDestroyed(UShipWeaponComponent*,AActor*)
{
    ActivateForWreck();
}

void UShipWreckBillboardComponent::ActivateForWreck()
{
    const auto* W=Weapon.Get();
    if(!W || !W->bIsDestroyed || W->DestructionStage==EShipDestructionStage::Submerged || W->DestructionStage==EShipDestructionStage::Cleanup)return;
    if(bWreckVisualActive)return;
    const auto* Movement=GetOwner()->FindComponentByClass<UShipMovementComponent>();
    const float Scale=Movement && Movement->Profile?Movement->Profile->WreckEffectScale:1.f;
    for(int32 Index=0;Index<Elements.Num();++Index)
    {
        if(!Elements[Index].Material)continue;
        auto* Dynamic=Cast<UMaterialInstanceDynamic>(Elements[Index].Material);
        if(!Dynamic)
        {
            Dynamic=UMaterialInstanceDynamic::Create(Elements[Index].Material,this);
            SetMaterial(Index,Dynamic);
        }
        if(Dynamic)Dynamic->SetScalarParameterValue(TEXT("Phase"),Phase);
        Elements[Index].BaseSizeX*=Scale;Elements[Index].BaseSizeY*=Scale;
    }
    bWreckVisualActive=true;
    UpdatePlacement();
    SetHiddenInGame(false);
    SetVisibility(true);
    SetComponentTickEnabled(true);
}

void UShipWreckBillboardComponent::UpdatePlacement()
{
    const auto* Hull=UShipCombatComponent::PhysicalHull(GetOwner());
    if(!Hull || DeckOffsetCm.ContainsNaN())return;
    const FVector Origin=Hull->GetComponentLocation();
    FVector Up=FVector::UpVector;
    if(auto* Anchor=GetOwner()->FindComponentByClass<UCesiumGlobeAnchorComponent>())
        if(auto* Geo=Anchor->ResolveGeoreference())
        {
            const FVector LLH=Geo->TransformUnrealPositionToLongitudeLatitudeHeight(Origin);
            Up=(Geo->TransformLongitudeLatitudeHeightPositionToUnreal(LLH+FVector(0,0,1))-Origin).GetSafeNormal();
        }
    FVector Forward=FVector::VectorPlaneProject(Hull->GetForwardVector(),Up).GetSafeNormal();
    if(Forward.IsNearlyZero())Forward=FVector::VectorPlaneProject(Hull->GetRightVector(),Up).GetSafeNormal();
    const FVector Right=FVector::CrossProduct(Up,Forward).GetSafeNormal();
    // Geographic up deliberately survives a capsized hull's roll. Storing the
    // resulting world position as an attached relative position also follows
    // the wreck immediately when its Cesium anchor changes coordinate frame.
    const auto* Movement=GetOwner()->FindComponentByClass<UShipMovementComponent>();
    const float Scale=Movement && Movement->Profile?Movement->Profile->WreckEffectScale:1.f;
    SetWorldLocation(Origin+(Forward*DeckOffsetCm.X+Right*DeckOffsetCm.Y+Up*DeckOffsetCm.Z)*Scale,false,nullptr,ETeleportType::TeleportPhysics);
}

void UShipWreckBillboardComponent::HideEffect()
{
    bWreckVisualActive=false;
    SetVisibility(false);
    SetHiddenInGame(true);
    SetComponentTickEnabled(false);
}

void UShipWreckBillboardComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime,TickType,TickFunction);
    const auto* W=Weapon.Get();
    if(!W || W->DestructionStage==EShipDestructionStage::Submerged || W->DestructionStage==EShipDestructionStage::Cleanup)
    {
        HideEffect();
        return;
    }
    UpdatePlacement();
}

void UShipWreckBillboardComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if(auto* W=Weapon.Get())W->OnShipDestroyed.RemoveDynamic(this,&UShipWreckBillboardComponent::HandleDestroyed);
    HideEffect();
    Weapon.Reset();
    Super::EndPlay(Reason);
}
