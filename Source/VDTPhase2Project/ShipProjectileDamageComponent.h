#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShipProjectileDamageComponent.generated.h"

/** Metadata only: the existing BP projectile movement, hit event and VFX stay intact. */
UCLASS(ClassGroup=(Combat),meta=(BlueprintSpawnableComponent))
class VDTPHASE2PROJECT_API UShipProjectileDamageComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Damage",meta=(ClampMin="0")) float ProjectileDamage=25;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Damage",meta=(Units="cm")) float SplashRadiusCm=1000;
    UPROPERTY(BlueprintReadOnly,Category="Damage") int32 SourceTeam=2;
    UPROPERTY(BlueprintReadOnly,Category="Damage") TWeakObjectPtr<AActor> SourceShip;
    UPROPERTY(BlueprintReadOnly,Category="Damage") bool bImpacted=false;
    void InitializeSource(AActor* Source);
    bool ClaimVictim(AActor* Victim);
protected:
    virtual void BeginPlay() override;
private:
    TSet<TWeakObjectPtr<AActor>> DamagedActors;
};
