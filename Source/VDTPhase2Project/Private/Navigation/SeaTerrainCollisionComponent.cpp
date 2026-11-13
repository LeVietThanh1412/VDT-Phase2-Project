#include "Navigation/SeaTerrainCollisionComponent.h"
#include "Utilities/NavalCesiumCompatibility.h"
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "CesiumLoadedTile.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"
#include "Navigation/SeaNavigationMap.h"
#include <CesiumGltf/AccessorView.h>
#include <CesiumGltf/Model.h>
#include <CesiumGltfContent/SkirtMeshMetadata.h>

namespace {
struct FCoastalVertex {
  FVector Local;
  FVector2D Metric;
};
// Clip in the SAME affine ECEF/ENU frame as A*. Interpolate original terrain
// vertices: retained coastal pieces lie exactly on the source triangle plane.
TArray<FCoastalVertex> ClipTerrain(const TArray<FCoastalVertex> &Input,
                                   const FVector2D &Lo, const FVector2D &Hi) {
  TArray<FCoastalVertex> Polygon = Input;
  for (int32 Plane = 0; Plane < 4 && Polygon.Num() >= 3; ++Plane) {
    const int32 Axis = Plane / 2;
    const bool Lower = Plane % 2 == 0;
    const double Bound = Lower ? Lo[Axis] : Hi[Axis];
    TArray<FCoastalVertex> Next;
    auto Distance = [&](const FCoastalVertex &V) {
      return Lower ? V.Metric[Axis] - Bound : Bound - V.Metric[Axis];
    };
    for (int32 I = 0; I < Polygon.Num(); ++I) {
      const auto &A = Polygon[I];
      const auto &B = Polygon[(I + 1) % Polygon.Num()];
      const double DA = Distance(A), DB = Distance(B);
      const bool InsideA = DA >= 0, InsideB = DB >= 0;
      if (InsideA)
        Next.Add(A);
      if (InsideA != InsideB) {
        const double T = DA / (DA - DB);
        Next.Add({FMath::Lerp(A.Local, B.Local, T),
                  FMath::Lerp(A.Metric, B.Metric, T)});
      }
    }
    Polygon = MoveTemp(Next);
  }
  return Polygon;
}
} // namespace

USeaTerrainCollisionComponent::USeaTerrainCollisionComponent() {
  PrimaryComponentTick.bCanEverTick = true;
  PrimaryComponentTick.TickGroup = TG_PrePhysics;
  bTickInEditor = false;
}
void USeaTerrainCollisionComponent::BeginPlay() {
  Super::BeginPlay();
  if (!bFilterSeaLevelDEM) {
    SetComponentTickEnabled(false);
    return;
  }
  auto *Terrain = Cast<ACesium3DTileset>(GetOwner());
  if (!Terrain) {
    LastError = TEXT("Attach to the QuangNinh Cesium tileset");
    SetComponentTickEnabled(false);
    return;
  }
  Georeference = Terrain->ResolveGeoreference();
  Chart =
      FSeaNavigationMap::Load(FPaths::ProjectContentDir() /
                                  TEXT("SeaNavigation/halong_navigation.json"),
                              LastError);
  if (!Chart || !Georeference.IsValid()) {
    SetComponentTickEnabled(false);
    return;
  }
  // An exclusion buffer is for vessel planning, not a physical sea floor.
  // Use raw coast cells for DEM clipping; actual land and submerged nonzero
  // terrain still retain collision under the existing selected-LOD policy.
  Chart = Chart->UnbufferedCoastView();
  // Cesium selects tiles and toggles native collision in its actor tick.
  // Filter after that selection, BEFORE Chaos simulates this frame.
  AddTickPrerequisiteActor(Terrain);
}
bool USeaTerrainCollisionComponent::IsChartWater(const FVector2D &A,
                                                 const FVector2D &B,
                                                 const FVector2D &C) const {
  if (!Chart->IsWater(A) || !Chart->IsWater(B) || !Chart->IsWater(C))
    return false;
  // Conservative full bounding-cell test. Retain ambiguous coast/low-LOD
  // triangles rather than accidentally opening a hole through an island.
  const FVector2D Lo = A.ComponentMin(B).ComponentMin(C),
                  Hi = A.ComponentMax(B).ComponentMax(C);
  const int32 X0 = FMath::FloorToInt((Lo.X - Chart->MinEast) / Chart->CellSize);
  const int32 Y0 =
      FMath::FloorToInt((Lo.Y - Chart->MinNorth) / Chart->CellSize);
  const int32 X1 = FMath::FloorToInt((Hi.X - Chart->MinEast) / Chart->CellSize);
  const int32 Y1 =
      FMath::FloorToInt((Hi.Y - Chart->MinNorth) / Chart->CellSize);
  for (int32 Y = Y0; Y <= Y1; ++Y)
    for (int32 X = X0; X <= X1; ++X)
      if (!Chart->IsWater(X, Y))
        return false;
  return true;
}
bool USeaTerrainCollisionComponent::BuildTile(UStaticMeshComponent *Source,
                                              FTile &Out) {
  const double BuildStarted = FPlatformTime::Seconds();
  UStaticMesh *Mesh = Source->GetStaticMesh();
  auto *Body = Mesh ? Mesh->GetBodySetup() : nullptr;
  if (!Body || Body->TriMeshGeometries.IsEmpty())
    return false;
  Out.Source = Source;
  Out.Original = Source->GetCollisionEnabled();
  Out.RootRelative = Source->GetComponentTransform().GetRelativeTransform(
      GetOwner()->GetRootComponent()->GetComponentTransform());
  for (const auto &Physics : Body->TriMeshGeometries)
    if (Physics)
      Out.GeometryHash = HashCombine(Out.GeometryHash, Physics->GetTypeHash());
  // A tile may stream out and return. Reuse identical collision instead of
  // growing duplicate geometry every time the player turns the camera.
  if (const auto *Cached = Tiles.FindByPredicate([&](const FTile &T) {
        return T.GeometryHash == Out.GeometryHash &&
               T.RootRelative.Equals(Out.RootRelative, .001) &&
               (T.Land == 0 || T.Collision.IsValid() ||
                T.WaterDetailCollision.IsValid());
      })) {
    Out.Land = Cached->Land;
    Out.Water = Cached->Water;
    Out.Bathymetry = Cached->Bathymetry;
    Out.SeaSkirts = Cached->SeaSkirts;
    Out.bSkirtMetadata = Cached->bSkirtMetadata;
    Out.Collision = Cached->Collision;
    Out.WaterDetailCollision = Cached->WaterDetailCollision;
    return true;
  }
  TArray<FVector> Vertices;
  TArray<int32> Indices;
  TArray<int32> WaterDetailIndices;
  const FTransform Transform = Source->GetComponentTransform();
  // Cesium expands skirts OUTWARD as well as downward. Longitude/latitude
  // coincidence is not a valid skirt test (captured offsets: 0.23-0.49 m).
  // Use the public loaded-primitive interface and generated-vertex metadata.
  // Match positions rather than assuming Chaos preserves glTF vertex indices.
  TSet<FVector3f> GeneratedSkirtPositions;
  if (auto *Primitive = Cast<ICesiumLoadedTilePrimitive>(Source)) {
    const auto *Gltf = Primitive->GetMeshPrimitive();
    const auto *Model = Primitive->GetLoadedTile().GetGltfModel();
    const auto Metadata =
        Gltf ? CesiumGltfContent::SkirtMeshMetadata::parseFromGltfExtras(
                   Gltf->extras)
             : std::nullopt;
    if (Metadata && Model) {
      const auto Attribute = Gltf->attributes.find("POSITION");
      if (Attribute != Gltf->attributes.end()) {
        const CesiumGltf::AccessorView<FVector3f> Positions(*Model,
                                                            Attribute->second);
        if (Positions.status() == CesiumGltf::AccessorViewStatus::Valid) {
          Out.bSkirtMetadata = true;
          const FVector Scale =
              Primitive->GetLoadedTile()
                  .GetGltfToUnrealLocalVertexPositionScaleFactor();
          const uint64 Begin = Metadata->noSkirtVerticesBegin,
                       End = Begin + Metadata->noSkirtVerticesCount;
          for (int64 I = 0; I < Positions.size(); ++I)
            if (uint64(I) < Begin || uint64(I) >= End)
              GeneratedSkirtPositions.Add(
                  FVector3f(FVector(Positions[I]) * Scale));
        }
      }
    }
  }
  // Read the retained CPU physics mesh, not GPU render buffers (Cesium may
  // discard those). Never mutate shared BodySetup or the plugin's meshes.
  for (const auto &Physics : Body->TriMeshGeometries) {
    if (!Physics)
      continue;
    const auto &Particles = Physics->Particles();
    const auto &Elements = Physics->Elements();
    const int32 Base = Vertices.Num();
    TArray<FVector2D> Metric;
    TArray<bool> IsGeneratedSkirt;
    TArray<double> Height;
    for (int32 I = 0; I < static_cast<int32>(Particles.Size()); ++I) {
      const FVector Local(Particles.GetX(I));
      Vertices.Add(Local);
      const FVector World = Transform.TransformPosition(Local) +
                            FVector(GetWorld()->OriginLocation);
      Metric.Add(Chart->ToMetric(
          Georeference->TransformUnrealPositionToEarthCenteredEarthFixed(
              World)));
      const FVector LLH =
          Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(World);
      Height.Add(LLH.Z);
      IsGeneratedSkirt.Add(GeneratedSkirtPositions.Contains(FVector3f(Local)));
    }
    auto Append = [&](const auto &Triangles) {
      for (const auto &T : Triangles) {
        const int32 A = T[0], B = T[1], C = T[2];
        auto KeepOriginal = [&]() {
          Indices.Append({Base + A, Base + C, Base + B});
          ++Out.Land;
        };
        const double Low = FMath::Min3(Height[A], Height[B], Height[C]);
        const double High = FMath::Max3(Height[A], Height[B], Height[C]);
        const bool SeaSurface =
            Low >= -SeaLevelToleranceM && High <= SeaLevelToleranceM;
        const bool SeaSkirt =
            High <= SeaLevelToleranceM && High >= -SeaLevelToleranceM &&
            Low < -SeaLevelToleranceM &&
            (IsGeneratedSkirt[A] || IsGeneratedSkirt[B] || IsGeneratedSkirt[C]);
        if (!SeaSurface && !SeaSkirt) {
          if (High < -SeaLevelToleranceM)
            ++Out.Bathymetry;
          // Coast support stays persistent. Inside certified chart
          // water, nonzero terrain belongs to its selected LOD: an
          // obsolete coarse interpolation is not bathymetry evidence.
          if (IsChartWater(Metric[A], Metric[B], Metric[C])) {
            WaterDetailIndices.Append({Base + A, Base + C, Base + B});
            ++Out.Land;
            continue;
          }
        }
        if ((SeaSurface || SeaSkirt) &&
            IsChartWater(Metric[A], Metric[B], Metric[C])) {
          if (SeaSkirt)
            ++Out.SeaSkirts;
          ++Out.Water;
          continue;
        }
        const FVector2D Lo =
            Metric[A].ComponentMin(Metric[B]).ComponentMin(Metric[C]);
        const FVector2D Hi =
            Metric[A].ComponentMax(Metric[B]).ComponentMax(Metric[C]);
        const int32 X0 = FMath::Clamp(
            FMath::FloorToInt((Lo.X - Chart->MinEast) / Chart->CellSize), 0,
            Chart->Width - 1);
        const int32 Y0 = FMath::Clamp(
            FMath::FloorToInt((Lo.Y - Chart->MinNorth) / Chart->CellSize), 0,
            Chart->Height - 1);
        const int32 X1 = FMath::Clamp(
            FMath::FloorToInt((Hi.X - Chart->MinEast) / Chart->CellSize), 0,
            Chart->Width - 1);
        const int32 Y1 = FMath::Clamp(
            FMath::FloorToInt((Hi.Y - Chart->MinNorth) / Chart->CellSize), 0,
            Chart->Height - 1);
        bool AnyWater = false;
        for (int32 Y = Y0; Y <= Y1 && !AnyWater; ++Y)
          for (int32 X = X0; X <= X1; ++X)
            if (Chart->IsWater(X, Y)) {
              AnyWater = true;
              break;
            }
        const FVector2D ChartLo(Chart->MinEast, Chart->MinNorth),
            ChartHi = ChartLo +
                      FVector2D(Chart->Width, Chart->Height) * Chart->CellSize;
        if (!AnyWater || Hi.X <= ChartLo.X || Hi.Y <= ChartLo.Y ||
            Lo.X >= ChartHi.X || Lo.Y >= ChartHi.Y) {
          KeepOriginal();
          continue;
        }
        const TArray<FCoastalVertex> Original = {
            {Vertices[Base + A], Metric[A]},
            {Vertices[Base + B], Metric[B]},
            {Vertices[Base + C], Metric[C]}};
        auto KeepClipped = [&](const FVector2D &Min, const FVector2D &Max,
                               bool WaterDetail = false) {
          if (Max.X <= Min.X || Max.Y <= Min.Y)
            return;
          const auto P = ClipTerrain(Original, Min, Max);
          if (P.Num() < 3)
            return;
          for (int32 J = 1; J < P.Num() - 1; ++J) {
            if (FVector::CrossProduct(P[J].Local - P[0].Local,
                                      P[J + 1].Local - P[0].Local)
                    .SizeSquared() < .001)
              continue;
            const int32 Start = Vertices.Num();
            Vertices.Append({P[0].Local, P[J].Local, P[J + 1].Local});
            // Chaos input is already wound for collision. Procedural
            // mesh cooking flips normals, so reverse it once here.
            if (WaterDetail)
              WaterDetailIndices.Append({Start, Start + 2, Start + 1});
            else
              Indices.Append({Start, Start + 2, Start + 1});
            ++Out.Land;
          }
        };
        // Merge identical row runs before clipping. This retains the
        // exact same union of coastal cells without cooking a separate
        // triangle fan for every 25 m cell in a coarse terrain triangle.
        struct FCoastRect {
          int32 X0, X1, Y0, Y1;
          bool Water;
        };
        TArray<FCoastRect> Rectangles;
        TArray<int32> Previous, Current;
        for (int32 Y = Y0; Y <= Y1; ++Y) {
          Current.Reset();
          for (int32 X = X0; X <= X1;) {
            const bool Water = Chart->IsWater(X, Y);
            const int32 Begin = X++;
            while (X <= X1 && Chart->IsWater(X, Y) == Water)
              ++X;
            if (Water && (SeaSurface || SeaSkirt))
              continue;
            int32 Match = INDEX_NONE;
            for (int32 Id : Previous)
              if (Rectangles[Id].X0 == Begin && Rectangles[Id].X1 == X &&
                  Rectangles[Id].Water == Water) {
                Match = Id;
                break;
              }
            if (Match == INDEX_NONE)
              Match = Rectangles.Add({Begin, X, Y, Y + 1, Water});
            else
              Rectangles[Match].Y1 = Y + 1;
            Current.Add(Match);
          }
          Swap(Previous, Current);
        }
        for (const auto &R : Rectangles)
          KeepClipped(ChartLo + FVector2D(R.X0, R.Y0) * Chart->CellSize,
                      ChartLo + FVector2D(R.X1, R.Y1) * Chart->CellSize,
                      R.Water);
        // Never remove geometry outside the validated chart extent.
        KeepClipped(Lo, FVector2D(FMath::Min(Hi.X, ChartLo.X), Hi.Y));
        KeepClipped(FVector2D(FMath::Max(Lo.X, ChartHi.X), Lo.Y), Hi);
        KeepClipped(FVector2D(FMath::Max(Lo.X, ChartLo.X), Lo.Y),
                    FVector2D(FMath::Min(Hi.X, ChartHi.X),
                              FMath::Min(Hi.Y, ChartLo.Y)));
        KeepClipped(
            FVector2D(FMath::Max(Lo.X, ChartLo.X), FMath::Max(Lo.Y, ChartHi.Y)),
            FVector2D(FMath::Min(Hi.X, ChartHi.X), Hi.Y));
        if (SeaSurface || SeaSkirt)
          ++Out.Water;
        if (SeaSkirt)
          ++Out.SeaSkirts;
      }
    };
    if (Elements.RequiresLargeIndices())
      Append(Elements.GetLargeIndexBuffer());
    else
      Append(Elements.GetSmallIndexBuffer());
  }
  auto CreateProxy = [&](const TArray<int32> &Triangles) {
    if (Triangles.IsEmpty())
      return static_cast<UProceduralMeshComponent *>(nullptr);
    auto *Proxy = NewObject<UProceduralMeshComponent>(GetOwner(), NAME_None,
                                                      RF_Transient);
    Proxy->SetMobility(Source->Mobility);
    // Keep physical land after its render tile is hidden/unloaded. Colliding
    // LOD surfaces are conservative: removing a higher tile underneath a
    // body then reintroducing it above that body makes one-sided Chaos
    // triangles fail. The fixed root-relative cache avoids that hole.
    Proxy->SetupAttachment(GetOwner()->GetRootComponent());
    Proxy->SetRelativeTransform(Out.RootRelative);
    Proxy->bUseComplexAsSimpleCollision = true;
    Proxy->bUseAsyncCooking = false;
    Proxy->SetCollisionObjectType(Source->GetCollisionObjectType());
    Proxy->SetCollisionResponseToChannels(
        Source->GetCollisionResponseToChannels());
    Proxy->SetCollisionEnabled(Out.Original);
    Proxy->SetCanEverAffectNavigation(false);
    Proxy->SetCastShadow(false);
    Proxy->RegisterComponent();
    Proxy->CreateMeshSection(0, Vertices, Triangles, {}, {}, {}, {}, true);
    Proxy->SetMeshSectionVisible(0, false);
    Proxy->SetHiddenInGame(true);
    Proxy->SetVisibility(false);
    return Proxy;
  };
  Out.Collision = CreateProxy(Indices);
  Out.WaterDetailCollision = CreateProxy(WaterDetailIndices);
  const double Milliseconds = (FPlatformTime::Seconds() - BuildStarted) * 1000;
  if (Milliseconds > 250)
    UE_LOG(LogTemp, Display,
           TEXT("SeaTerrain collision build %.1f ms: land=%d water=%d "
                "vertices=%d source=%s"),
           Milliseconds, Out.Land, Out.Water, Vertices.Num(),
           *Source->GetName());
  return true;
}
bool USeaTerrainCollisionComponent::HasCollisionCoverageAt(
    const FVector &World) const {
  for (const auto &Tile : Tiles) {
    const auto *Source = Tile.Source.Get();
    if (!Source || !Source->IsVisible() || !Source->IsRegistered() ||
        (Tile.Land > 0 && !Tile.Collision.IsValid() &&
         !Tile.WaterDetailCollision.IsValid()))
      continue;
    const auto Box = Source->Bounds.GetBox();
    if (Box.IsInsideOrOn(FVector(World.X, World.Y, Box.GetCenter().Z)))
      return true;
  }
  return false;
}
void USeaTerrainCollisionComponent::TickComponent(
    float Delta, ELevelTick Type, FActorComponentTickFunction *Function) {
  Super::TickComponent(Delta, Type, Function);
  // Editor validation/reconstruction can dispatch a tick before BeginPlay.
  // Runtime-only filtering must never touch editor preview terrain.
  if (!GetWorld() || !GetWorld()->IsGameWorld() || !Chart.IsValid() ||
      !Georeference.IsValid())
    return;
  TArray<UStaticMeshComponent *> Components;
  GetOwner()->GetComponents(Components);
  for (auto *Source : Components) {
    FTile *Tile = Tiles.FindByPredicate(
        [Source](const FTile &T) { return T.Source.Get() == Source; });
    // Process selected tiles only; the physics mesh may not yet exist while
    // Cesium finishes asynchronous resource creation. Retry on next frame.
    if (!Tile &&
        Source->GetCollisionEnabled() != ECollisionEnabled::NoCollision) {
      FTile Added;
      if (BuildTile(Source, Added)) {
        // A reloaded tile already reuses its collision proxy in
        // BuildTile. Rebind its expired source slot too: appending a
        // weak-source record on every eviction grows metadata forever
        // even though the actual collision geometry stays constant.
        Tile = Tiles.FindByPredicate([&](const FTile &Cached) {
          return !Cached.Source.IsValid() &&
                 Cached.GeometryHash == Added.GeometryHash &&
                 Cached.RootRelative.Equals(Added.RootRelative, .001) &&
                 Cached.Collision == Added.Collision &&
                 Cached.WaterDetailCollision == Added.WaterDetailCollision;
        });
        if (Tile)
          *Tile = Added;
        else {
          Tiles.Add(Added);
          Tile = &Tiles.Last();
        }
      }
    }
    if (!Tile)
      continue;
    Source->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  }
  FilteredTiles = Tiles.Num();
  TMap<UProceduralMeshComponent *, bool> DetailVisibility;
  for (const auto &Tile : Tiles)
    if (auto *Detail = Tile.WaterDetailCollision.Get()) {
      bool &Active = DetailVisibility.FindOrAdd(Detail);
      Active |= Tile.Source.IsValid() && Tile.Source->IsVisible();
    }
  for (const auto &Entry : DetailVisibility)
    Entry.Key->SetCollisionEnabled(Entry.Value
                                       ? ECollisionEnabled::QueryAndPhysics
                                       : ECollisionEnabled::NoCollision);
  RetainedLandTriangles = RemovedWaterTriangles = RetainedSubmergedTriangles =
      RemovedSeaSkirtTriangles = SkirtMetadataTiles = 0;
  TSet<const UProceduralMeshComponent *> Counted;
  for (const auto &Tile : Tiles) {
    const auto *Proxy = Tile.Collision.IsValid()
                            ? Tile.Collision.Get()
                            : Tile.WaterDetailCollision.Get();
    if (Proxy && Counted.Contains(Proxy))
      continue;
    if (Proxy)
      Counted.Add(Proxy);
    RetainedLandTriangles += Tile.Land;
    RemovedWaterTriangles += Tile.Water;
    RetainedSubmergedTriangles += Tile.Bathymetry;
    RemovedSeaSkirtTriangles += Tile.SeaSkirts;
    SkirtMetadataTiles += Tile.bSkirtMetadata ? 1 : 0;
  }
}
void USeaTerrainCollisionComponent::EndPlay(const EEndPlayReason::Type Reason) {
  for (const auto &Tile : Tiles) {
    if (auto *Source = Tile.Source.Get())
      Source->SetCollisionEnabled(Tile.Original);
    if (auto *Proxy = Tile.Collision.Get())
      Proxy->DestroyComponent();
    if (auto *Proxy = Tile.WaterDetailCollision.Get())
      Proxy->DestroyComponent();
  }
  Tiles.Reset();
  Super::EndPlay(Reason);
}
