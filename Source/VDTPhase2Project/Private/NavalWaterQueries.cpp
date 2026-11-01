#include "NavalWaterQueries.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "WaterBodyOceanActor.h"

void NavalWaterQueries::IgnoreWaterActors(UWorld* World,FCollisionQueryParams& Query)
{
    for(TActorIterator<AActor> It(World);It;++It)
    {
        const FString Name=It->GetClass()->GetName();
        if(Name.StartsWith(TEXT("WaterBody")) || Name==TEXT("WaterZone"))Query.AddIgnoredActor(*It);
    }
}

UWaterBodyComponent* NavalWaterQueries::FindOcean(UWorld* World)
{
    for(TActorIterator<AWaterBodyOcean> It(World);It;++It)return It->GetWaterBodyComponent();
    return nullptr;
}
