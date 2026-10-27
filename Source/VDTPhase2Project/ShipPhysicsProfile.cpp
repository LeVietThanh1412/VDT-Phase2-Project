#include "ShipPhysicsProfile.h"
void UShipPhysicsProfile::SetDefaults(EShipHullClass Type)
{
    HullClass=Type;const bool Large=Type==EShipHullClass::Large;
    MassKg=Large?240:150;InertiaScale=Large?FVector(1.8f,1.8f,2.f):FVector(1);
    CenterOfMassOffset=Large?FVector(0,0,-80):FVector::ZeroVector;
    LinearDamping=Large?.55f:.5f;AngularDamping=Large?.65f:.5f;
    PropulsionScale=Large?1.1f:1;RudderScale=Large?.8f:1;
    ForwardMaxSpeed=26;ReverseMaxSpeed=7;CruiseSpeed=20;
    // Damping must grow with the 1.6x mass. The former 1.2x value reduced
    // damping per kg and let the Large hull bounce clear of wave contact.
    DisplacementMultiplier=Large?1.6f:1;VerticalDampingMultiplier=Large?1.8f:1;PontoonRadiusMultiplier=1;
    MaxHealth=100;BaseProjectileDamage=10;
    if(Type==EShipHullClass::Small)
    {
        MassKg=90;PropulsionScale=.7f;RudderScale=.5f;InertiaScale=FVector(1);
        CenterOfMassOffset=FVector(0,0,-150);LinearDamping=.5f;AngularDamping=.65f;
        ForwardMaxSpeed=29;CruiseSpeed=24;DisplacementMultiplier=.6f;VerticalDampingMultiplier=.8f;
        PontoonRadiusMultiplier=1;PontoonLocations={FVector(2100,550,-100),FVector(2100,-550,-100),FVector(-2100,550,-100),FVector(-2100,-550,-100)};
        MaxHealth=38;BaseProjectileDamage=6;RudderLocationScale=1;CameraDistanceScale=.5f;WreckEffectScale=.425f;
        StaticVisualTransform=FTransform(FRotator(0,-90,0),FVector(0,0,-400),FVector(2.75));
        // Reference yaw pivot verified against the actual DDG102 skeleton.
        WeaponDriverTransform=FTransform(FQuat::Identity,FVector(2000,0,420)-FVector(4907.657,-5.639,773.625)*.4,FVector(.4));
    }
}
