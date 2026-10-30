#include "ShipProjectileDamageComponent.h"
#include "ShipCombatComponent.h"
void UShipProjectileDamageComponent::BeginPlay(){Super::BeginPlay();InitializeSource(GetOwner()->GetOwner());}
void UShipProjectileDamageComponent::InitializeSource(AActor* Source)
{
    if(bSourceCaptured)return;
    if(Source && Source->FindComponentByClass<UShipWeaponComponent>())
    {const auto* Weapon=Source->FindComponentByClass<UShipWeaponComponent>();SourceShip=Source;SourceTeam=Weapon->Team();ProjectileDamage=Weapon->BaseProjectileDamage;bSourceCaptured=true;}
}
bool UShipProjectileDamageComponent::ClaimVictim(AActor* Victim)
{
    if(DamagedActors.Contains(Victim))return false;DamagedActors.Add(Victim);return true;
}
