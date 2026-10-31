#pragma once

class UWorld;
class UWaterBodyComponent;
struct FCollisionQueryParams;

/** Internal query policy shared by survey, navigation and combat. No cache:
 * streamed world membership remains live, as in the original implementations. */
namespace NavalWaterQueries
{
    void IgnoreWaterActors(UWorld* World,FCollisionQueryParams& Query);
    UWaterBodyComponent* FindOcean(UWorld* World);
}
