#include "ShipProjectileDamageComponent.h"
#include "ShipCombatComponent.h"
void UShipProjectileDamageComponent::BeginPlay(){Super::BeginPlay();InitializeSource(GetOwner()->GetOwner());}
void UShipProjectileDamageComponent::InitializeSource(AActor* Source)
{
    if(Source && Source->FindComponentByClass<UShipWeaponComponent>())
    {SourceShip=Source;SourceTeam=Source->FindComponentByClass<UShipWeaponComponent>()->Team();}
}
bool UShipProjectileDamageComponent::ClaimVictim(AActor* Victim)
{
    if(DamagedActors.Contains(Victim))return false;DamagedActors.Add(Victim);return true;
}
