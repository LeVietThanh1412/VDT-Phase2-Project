#include "ShipPhysicsProfile.h"
void UShipPhysicsProfile::SetDefaults(EShipHullClass Type)
{
    HullClass=Type;const bool Large=Type==EShipHullClass::Large;
    // UE scales equivalent body dimensions here, not individual inertia axes.
    // Keep horizontal dimensions at one: enlarging X/Y also makes yaw inert.
    // The original large hull remains heavier; passive drag now supplies stability.
    MassKg=Large?240:150;InertiaScale=FVector(1,1,6);
    CenterOfMassOffset=Large?FVector(0,0,-80):FVector::ZeroVector;
    // Water buoyancy drag damps upward Z only; body damping also attenuates downward heave.
    LinearDamping=Large?.9f:.5f;AngularDamping=Large?.65f:.5f;
    PropulsionScale=Large?1.1f:1;RudderScale=Large?.8f:1;
    ForwardMaxSpeed=26;ReverseMaxSpeed=7;CruiseSpeed=20;
    // Preserve the historical 1.8x Large fix, extending it after the fixed-field
    // 120s baseline exposed >4.2m/s p99 vertical motion and contact loss.
    // Scale upward buoyancy drag independently; mass/drive/buoyant lift stay intact.
    // Double the lighter hulls' support radius, compensating r^3 volume so
    // displacement stays the same while troughs no longer uncover all supports.
    DisplacementMultiplier=Large?1.6f:.125f;VerticalDampingMultiplier=Large?3.6f:1;PontoonRadiusMultiplier=Large?1:2;
    HeaveDampingPerSecond=20;LateralDampingPerSecond=4;RollDampingPerSecond=30;PitchDampingPerSecond=15;
    MaxHealth=100;BaseProjectileDamage=10;
    PontoonLocations.Reset();
    // Keep the four authored end supports; a middle pair samples the 100m gap
    // so a passing wave trough cannot leave the heavy hull wholly unsupported.
    if(Large)PontoonLocations={FVector(-5460,-820,-170),FVector(-5460,820,-170),FVector(4570,760,0),FVector(4570,-760,0),FVector(-445,-790,-85),FVector(-445,790,-85)};
    if(Type==EShipHullClass::Small)
    {
        MassKg=90;PropulsionScale=.7f;RudderScale=.5f;
        // The compact hull needs more pitch inertia; vertical scaling preserves yaw.
        InertiaScale=FVector(1,1,20);
        CenterOfMassOffset=FVector(0,0,-150);LinearDamping=.5f;AngularDamping=.65f;
        ForwardMaxSpeed=29;CruiseSpeed=24;DisplacementMultiplier=.075f;VerticalDampingMultiplier=.8f;
        PontoonLocations={FVector(2100,550,-100),FVector(2100,-550,-100),FVector(-2100,550,-100),FVector(-2100,-550,-100)};
        MaxHealth=38;BaseProjectileDamage=6;RudderLocationScale=1;CameraDistanceScale=.5f;WreckEffectScale=.425f;
        StaticVisualTransform=FTransform(FRotator(0,-90,0),FVector(0,0,-400),FVector(2.75));
        // Reference yaw pivot verified against the actual DDG102 skeleton.
        WeaponDriverTransform=FTransform(FQuat::Identity,FVector(2000,0,420)-FVector(4907.657,-5.639,773.625)*.4,FVector(.4));
    }
}
