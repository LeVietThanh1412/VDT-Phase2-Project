#pragma once
#include "CoreMinimal.h"

struct FSeaRouteResult
{
    bool bSuccess = false;
    FString Failure;
    TArray<FVector2D> Points;
    int32 Expansions = 0;
    double Milliseconds = 0;
};

/** Immutable offline coastline chart. ENU meters, never Unreal world centimeters. */
struct VDTPHASE2PROJECT_API FSeaNavigationMap
{
    static TSharedPtr<const FSeaNavigationMap> Load(const FString& Filename, FString& Error);
    int32 Width = 0, Height = 0;
    double CellSize = 100, MinEast = 0, MinNorth = 0, SafetyMargin = 0, MaxVesselRadius = 0;
    FVector OriginLLH = FVector::ZeroVector, OriginECEF = FVector::ZeroVector;
    FVector East = FVector(1,0,0), North = FVector(0,1,0);
    TArray<uint8> Cells, Clearance;
    TArray<int32> ConnectedRegions;
    bool IsWater(int32 X, int32 Y) const;
    bool IsWater(const FVector2D& P) const;
    bool SameWaterRegion(const FVector2D& A, const FVector2D& B) const;
    bool SegmentClear(const FVector2D& A, const FVector2D& B) const;
    int32 CellAt(const FVector2D& P) const;
    FVector2D Center(int32 Id) const;
    FVector2D ToMetric(const FVector& ECEF) const;
    FVector ToECEF(const FVector2D& P) const;
    FSeaRouteResult FindPath(const FVector2D& Start, const FVector2D& Goal,
                            int32 MaxExpansions = 75000, double MaxMilliseconds = 20,
                            const TArray<FVector2D>& TemporaryExclusions = {}, double ExclusionRadiusM = 0) const;
};
