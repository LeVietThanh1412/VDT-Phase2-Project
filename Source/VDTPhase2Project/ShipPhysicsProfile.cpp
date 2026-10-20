#include "ShipPhysicsProfile.h"
void UShipPhysicsProfile::SetDefaults(EShipHullClass Type)
{
    HullClass=Type;const bool Large=Type==EShipHullClass::Large;
    MassKg=Large?240:150;InertiaScale=Large?FVector(1.8f,1.8f,2.f):FVector(1);
    CenterOfMassOffset=Large?FVector(0,0,-80):FVector::ZeroVector;
    LinearDamping=Large?.55f:.5f;AngularDamping=Large?.65f:.5f;
    PropulsionScale=Large?1.1f:1;RudderScale=Large?.8f:1;
    ForwardMaxSpeed=26;ReverseMaxSpeed=7;CruiseSpeed=20;
    DisplacementMultiplier=Large?1.6f:1;VerticalDampingMultiplier=Large?1.2f:1;PontoonRadiusMultiplier=1;
}
