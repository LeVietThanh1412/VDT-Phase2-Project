#include "ShipEnemyProjectile.h"
#include "ShipEnemyNavigationComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DamageType.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AShipEnemyProjectile::AShipEnemyProjectile()
{
    PrimaryActorTick.bCanEverTick=false;InitialLifeSpan=20;
    Collision=CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));SetRootComponent(Collision);
    Collision->InitSphereRadius(12.5f);Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));Visual->SetupAttachment(Collision);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);Visual->SetRelativeScale3D(FVector(.25));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if(Sphere.Succeeded())Visual->SetStaticMesh(Sphere.Object);
    Movement=CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    Movement->UpdatedComponent=Collision;Movement->InitialSpeed=0;Movement->MaxSpeed=50000;
    Movement->ProjectileGravityScale=1;Movement->bRotationFollowsVelocity=true;Movement->bForceSubStepping=true;
    Movement->OnProjectileStop.AddDynamic(this,&AShipEnemyProjectile::OnStopped);
}
void AShipEnemyProjectile::BeginPlay()
{
    // Configure before movement starts. Both sweep collision and radial damage ignore the owner.
    if(GetOwner())Collision->IgnoreActorWhenMoving(GetOwner(),true);
    if(GetInstigator())Collision->IgnoreActorWhenMoving(GetInstigator(),true);
    for(TActorIterator<APawn> I(GetWorld());I;++I)
        if(I->FindComponentByClass<UShipEnemyNavigationComponent>())Collision->IgnoreActorWhenMoving(*I,true);
    Movement->Velocity=LaunchVelocity;Super::BeginPlay();
}
void AShipEnemyProjectile::OnStopped(const FHitResult& Hit)
{
    TArray<AActor*> Ignore;Ignore.Add(this);if(GetOwner())Ignore.Add(GetOwner());
    for(TActorIterator<APawn> I(GetWorld());I;++I)
        if(I->FindComponentByClass<UShipEnemyNavigationComponent>())Ignore.Add(*I);
    // Same inspected legacy model: 10,000 base radial damage, 1,000 cm radius, falloff, Visibility prevention.
    UGameplayStatics::ApplyRadialDamage(this,Damage,Hit.ImpactPoint,DamageRadiusCm,UDamageType::StaticClass(),Ignore,this,GetInstigatorController(),false,ECC_Visibility);
    Destroy();
}
