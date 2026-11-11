#include "Subsystems/NavalSectorSubsystem.h"
#include "Utilities/NavalCesiumCompatibility.h"
#include "CesiumGeoreference.h"
#include "Engine/World.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Navigation/SeaNavigationMap.h"

bool UNavalSectorSubsystem::DoesSupportWorldType(EWorldType::Type Type) const {
  return Type == EWorldType::Game || Type == EWorldType::PIE;
}
void UNavalSectorSubsystem::OnWorldBeginPlay(UWorld &World) {
  Super::OnWorldBeginPlay(World);
  LoadSectorGrid();
}

bool UNavalSectorSubsystem::LoadSectorGrid() {
  bGridReady = false;
  Sectors.Reset();
  Representatives.Reset();
  GridError.Empty();
  auto Fail = [&](const TCHAR *Error) {
    GridError = Error;
    return false;
  };
  Chart =
      FSeaNavigationMap::Load(FPaths::ProjectContentDir() /
                                  TEXT("SeaNavigation/halong_navigation.json"),
                              GridError);
  if (!Chart)
    return false;
  FString Text;
  TSharedPtr<FJsonObject> J;
  if (!FFileHelper::LoadFileToString(
          Text, *(FPaths::ProjectContentDir() /
                  TEXT("SeaNavigation/quangninh_sectors.json"))) ||
      Text.Len() > 4 * 1024 * 1024 ||
      !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) ||
      !J)
    return Fail(TEXT("Sector data missing, invalid or oversized"));
  if (J->GetStringField(TEXT("format")) != TEXT("VDT_NAVAL_SECTORS") ||
      J->GetIntegerField(TEXT("version")) != 1)
    return Fail(TEXT("Unsupported sector data format"));
  auto Vector2 = [](const TArray<TSharedPtr<FJsonValue>> &A) {
    return A.Num() == 2 ? FVector2D(A[0]->AsNumber(), A[1]->AsNumber())
                        : FVector2D(DBL_MAX);
  };
  CenterEN = Vector2(J->GetArrayField(TEXT("center_en_m")));
  WidthM = J->GetNumberField(TEXT("width_m"));
  HeightM = J->GetNumberField(TEXT("height_m"));
  SectorSizeM = J->GetNumberField(TEXT("sector_size_m"));
  YawDegrees = J->GetNumberField(TEXT("yaw_deg"));
  Columns = J->GetIntegerField(TEXT("columns"));
  Rows = J->GetIntegerField(TEXT("rows"));
  MainConnectedRegion = J->GetIntegerField(TEXT("main_connected_region"));
  if (!CenterEN.Equals(FVector2D(14500, 3500), .01) || WidthM != 2500 ||
      HeightM != 2500 || !FMath::IsFinite(YawDegrees) || YawDegrees != 0 ||
      !FMath::IsFinite(SectorSizeM) || SectorSizeM < 25 || SectorSizeM > 250 ||
      Columns != FMath::CeilToInt(WidthM / SectorSizeM) ||
      Rows != FMath::CeilToInt(HeightM / SectorSizeM))
    return Fail(
        TEXT("Sector coordinates differ from authoritative candidate 1"));
  TArray<uint8> ChartBytes;
  FSHAHash Hash;
  if (!FFileHelper::LoadFileToArray(
          ChartBytes, *(FPaths::ProjectContentDir() /
                        TEXT("SeaNavigation/halong_navigation.json"))))
    return Fail(TEXT("Navigation chart unavailable"));
  FSHA1::HashBuffer(ChartBytes.GetData(), ChartBytes.Num(), Hash.Hash);
  if (!Hash.ToString().Equals(J->GetStringField(TEXT("navigation_sha1")),
                              ESearchCase::IgnoreCase))
    return Fail(TEXT(
        "Sectors are stale relative to the navigation chart; regenerate them"));
  const auto &Items = J->GetArrayField(TEXT("sectors"));
  if (Items.Num() != Columns * Rows)
    return Fail(TEXT("Sector count does not match grid dimensions"));
  for (int32 Index = 0; Index < Items.Num(); ++Index) {
    const auto O = Items[Index]->AsObject();
    if (!O)
      return Fail(TEXT("Invalid sector row"));
    FNavalSector S;
    S.Id = FName(O->GetStringField(TEXT("id")));
    S.Row = O->GetIntegerField(TEXT("row"));
    S.Column = O->GetIntegerField(TEXT("column"));
    S.CenterEN = Vector2(O->GetArrayField(TEXT("center_en_m")));
    S.AnchorEN = Vector2(O->GetArrayField(TEXT("anchor_en_m")));
    const auto &LL = O->GetArrayField(TEXT("longitude_latitude"));
    if (LL.Num() != 2)
      return Fail(TEXT("Invalid sector longitude/latitude"));
    S.LongitudeLatitudeHeight =
        FVector(LL[0]->AsNumber(), LL[1]->AsNumber(), 0);
    S.TeamRegion = FName(O->GetStringField(TEXT("team_region")));
    S.WaterAreaFraction = O->GetNumberField(TEXT("water_area_fraction"));
    S.ChartSampleFraction = O->GetNumberField(TEXT("chart_sample_fraction"));
    S.ShoreClearanceM = O->GetNumberField(TEXT("shore_clearance_m"));
    S.ConnectedRegion = O->GetIntegerField(TEXT("connected_region"));
    S.bNavigable = O->GetBoolField(TEXT("navigable"));
    S.bSuitableSmall = O->GetBoolField(TEXT("suitable_small"));
    S.bSuitableMedium = O->GetBoolField(TEXT("suitable_medium"));
    S.bSuitableLarge = O->GetBoolField(TEXT("suitable_large"));
    S.bSpawnEligible = O->GetBoolField(TEXT("spawn_eligible"));
    S.bPatrolEligible = O->GetBoolField(TEXT("patrol_eligible"));
    for (const auto &V : O->GetArrayField(TEXT("nearby_islands")))
      S.NearbyIslands.Add(V->AsNumber());
    for (const auto &V : O->GetArrayField(TEXT("zones")))
      S.Zones.Add(FName(V->AsString()));
    for (const auto &V : O->GetArrayField(TEXT("samples"))) {
      const auto Sample = V->AsObject();
      if (!Sample)
        return Fail(TEXT("Invalid sector sample"));
      const auto P = Vector2(Sample->GetArrayField(TEXT("en_m")));
      if (!Chart->IsWater(P))
        return Fail(TEXT("Sector sample disagrees with navigation chart"));
      S.SamplesEN.Add(P);
    }
    if (S.Row != Index / Columns || S.Column != Index % Columns ||
        !IsInsideField(S.AnchorEN) ||
        (S.bNavigable &&
         (!Chart->IsWater(S.AnchorEN) || S.SamplesEN.IsEmpty())) ||
        !FMath::IsFinite(S.WaterAreaFraction) || S.WaterAreaFraction < -.0001 ||
        S.WaterAreaFraction > 1.0001)
      return Fail(
          TEXT("Invalid sector position, ordering, water fraction or anchor"));
    Sectors.Add(MoveTemp(S));
  }
  for (const auto &Pair :
       J->GetObjectField(TEXT("zone_representatives"))->Values) {
    const FName Id(Pair.Value->AsString());
    const int32 Index = Sectors.IndexOfByPredicate(
        [&](const FNavalSector &S) { return S.Id == Id; });
    if (Index == INDEX_NONE ||
        Sectors[Index].ConnectedRegion != MainConnectedRegion ||
        !Sectors[Index].bPatrolEligible)
      return Fail(
          TEXT("Required zone has no connected navigable representative"));
    Representatives.Add(FName(Pair.Key), Index);
  }
  bGridReady = true;
  return true;
}
bool UNavalSectorSubsystem::IsInsideField(FVector2D P, float Inset) const {
  const FVector2D D = P - CenterEN;
  return !P.ContainsNaN() && Inset >= 0 &&
         FMath::Abs(D.X) <= WidthM * .5 - Inset &&
         FMath::Abs(D.Y) <= HeightM * .5 - Inset;
}
int32 UNavalSectorSubsystem::SectorAtChartPosition(FVector2D P) const {
  if (!bGridReady || !IsInsideField(P))
    return INDEX_NONE;
  const int32 X = FMath::FloorToInt((P.X - CenterEN.X + WidthM * .5) /
                                    SectorSizeM),
              Y = FMath::FloorToInt((CenterEN.Y + HeightM * .5 - P.Y) /
                                    SectorSizeM);
  return X >= 0 && X < Columns && Y >= 0 && Y < Rows ? Y * Columns + X
                                                     : INDEX_NONE;
}
FVector2D UNavalSectorSubsystem::WorldToChart(FVector P) const {
  const auto *Geo = ACesiumGeoreference::GetDefaultGeoreference(this);
  return Geo && Chart
             ? Chart->ToMetric(
                   Geo->TransformUnrealPositionToEarthCenteredEarthFixed(
                       P + FVector(GetWorld()->OriginLocation)))
             : FVector2D(DBL_MAX);
}
FVector UNavalSectorSubsystem::ChartToWorld(FVector2D P) const {
  const auto *Geo = ACesiumGeoreference::GetDefaultGeoreference(this);
  return Geo && Chart ? Geo->TransformEarthCenteredEarthFixedPositionToUnreal(
                            Chart->ToECEF(P)) -
                            FVector(GetWorld()->OriginLocation)
                      : FVector::ZeroVector;
}
TArray<FVector> UNavalSectorSubsystem::FindSectorRoute(int32 A, int32 B) {
  LastRouteError.Empty();
  TArray<FVector> Out;
  if (!bGridReady || !Sectors.IsValidIndex(A) || !Sectors.IsValidIndex(B)) {
    LastRouteError = TEXT("Invalid sector query");
    return Out;
  }
  const auto Route =
      Chart->FindPath(Sectors[A].AnchorEN, Sectors[B].AnchorEN, 75000, 20);
  if (!Route.bSuccess) {
    LastRouteError = Route.Failure;
    return Out;
  }
  for (int32 I = 0; I < Route.Points.Num(); ++I) {
    if (!IsInsideField(Route.Points[I]) ||
        (I > 0 && !Chart->SegmentClear(Route.Points[I - 1], Route.Points[I]))) {
      LastRouteError =
          TEXT("Native route leaves selected field or crosses excluded water");
      return {};
    }
    Out.Add(FVector(Route.Points[I], 0));
  }
  return Out;
}
FString UNavalSectorSubsystem::ValidateRequiredConnections() {
  auto J = MakeShared<FJsonObject>();
  TArray<TSharedPtr<FJsonValue>> Paths;
  bool Good = bGridReady;
  const FName A[] = {TEXT("RED_STAGING"), TEXT("RED_APPROACH"),
                     TEXT("CENTER_CROSSING"), TEXT("BLUE_DEFENSE_CORE"),
                     TEXT("BLUE_WEST_ISLAND_PATROL")};
  const FName B[] = {TEXT("RED_APPROACH"), TEXT("CENTER_CROSSING"),
                     TEXT("BLUE_DEFENSE_CORE"), TEXT("BLUE_WEST_ISLAND_PATROL"),
                     TEXT("BLUE_NORTH_WATCH")};
  for (int32 I = 0; I < 5; ++I) {
    const auto *Start = Representatives.Find(A[I]);
    const auto *Goal = Representatives.Find(B[I]);
    const auto Points =
        Start && Goal ? FindSectorRoute(*Start, *Goal) : TArray<FVector>();
    auto P = MakeShared<FJsonObject>();
    P->SetStringField(TEXT("from"), A[I].ToString());
    P->SetStringField(TEXT("to"), B[I].ToString());
    P->SetBoolField(TEXT("passed"), Points.Num() >= 2);
    P->SetNumberField(TEXT("waypoints"), Points.Num());
    P->SetStringField(TEXT("error"), LastRouteError);
    Good &= Points.Num() >= 2;
    Paths.Add(MakeShared<FJsonValueObject>(P));
  }
  J->SetBoolField(TEXT("passed"), Good);
  J->SetArrayField(TEXT("connections"), Paths);
  FString Out;
  FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&Out));
  return Out;
}
