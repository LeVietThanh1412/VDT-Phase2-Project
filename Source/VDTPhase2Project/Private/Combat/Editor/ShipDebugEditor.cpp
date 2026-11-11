#include "Utilities/NavalWaterQueries.h"
#include "Combat/ShipCombatComponent.h"
#include "WaterBodyComponent.h"
#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

bool UShipCombatLibrary::SampleOceanWaves(UObject *WorldContextObject,
                                          FVector Location, FVector &Surface,
                                          FVector &Normal) {
  Surface = Location;
  Normal = FVector::UpVector;
#if WITH_EDITOR
  UWorld *World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
  if (!World)
    return false;
  if (auto *Water = NavalWaterQueries::FindOcean(World)) {
    const auto Result = Water->QueryWaterInfoClosestToWorldLocation(
        Location, EWaterBodyQueryFlags::ComputeLocation |
                      EWaterBodyQueryFlags::ComputeNormal |
                      EWaterBodyQueryFlags::ComputeDepth |
                      EWaterBodyQueryFlags::IncludeWaves);
    Surface = Result.GetWaterSurfaceLocation();
    Normal = Result.GetWaterSurfaceNormal();
    return !Result.IsInExclusionVolume() && Water->HasWaves();
  }
#endif
  return false;
}

FString UShipCombatLibrary::InspectImpactDebug(bool bDisable) {
#if WITH_EDITOR
  if (GEditor && GEditor->PlayWorld)
    return TEXT("ERROR: stop PIE before inspecting Blueprint graphs");
  FString Report;
  for (const TCHAR *Path :
       {TEXT("/Game/Ship/BluePrint/BP_ShipProjectile.BP_ShipProjectile"),
        TEXT("/Game/Ship/BluePrint/"
             "BP_ShipProjectileWithSpeed.BP_ShipProjectileWithSpeed")}) {
    auto *BP = LoadObject<UBlueprint>(nullptr, Path);
    if (!BP)
      return TEXT("ERROR: missing projectile Blueprint");
    TArray<UEdGraph *> Graphs;
    BP->GetAllGraphs(Graphs);
    int32 Changes = 0;
    for (auto *Graph : Graphs)
      for (UEdGraphNode *Node : Graph->Nodes)
        if (auto *Call = Cast<UK2Node_CallFunction>(Node)) {
          if (Call->FunctionReference.GetMemberName() !=
              TEXT("DrawDebugSphere"))
            continue;
          auto *In = Call->FindPin(UEdGraphSchema_K2::PN_Execute);
          auto *Out = Call->FindPin(UEdGraphSchema_K2::PN_Then);
          if (!In || !Out)
            return TEXT("ERROR: unexpected debug sphere pins; no save");
          Report += FString::Printf(
              TEXT("%s graph=%s node=%s inputs=%d outputs=%d\n"), Path,
              *Graph->GetName(), *Call->GetName(), In->LinkedTo.Num(),
              Out->LinkedTo.Num());
          if (!bDisable || In->LinkedTo.IsEmpty())
            continue;
          BP->Modify();
          Call->Modify();
          const auto Sources = In->LinkedTo;
          const auto Destinations = Out->LinkedTo;
          In->BreakAllPinLinks();
          Out->BreakAllPinLinks();
          for (auto *Source : Sources)
            for (auto *Destination : Destinations)
              Source->MakeLinkTo(Destination);
          ++Changes;
        }
    if (Changes) {
      FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
      FKismetEditorUtilities::CompileBlueprint(BP);
      if (BP->Status == BS_Error)
        return TEXT("ERROR: compile failed; no save");
    }
    Report +=
        FString::Printf(TEXT("changed=%d; caller owns backup/save\n"), Changes);
  }
  return Report;
#else
  return TEXT("ERROR: editor operation");
#endif
}

FString UShipCombatLibrary::DisableSensorDebugDefaults() {
#if WITH_EDITOR
  if (GEditor && GEditor->PlayWorld)
    return TEXT("ERROR: stop PIE before editing debug defaults");
  int32 Changed = 0, LinkedDebugPins = 0;
  for (const TCHAR *Path :
       {TEXT("/Game/Ship/BluePrint/BP_Ship.BP_Ship"),
        TEXT("/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy")}) {
    auto *BP = LoadObject<UBlueprint>(nullptr, Path);
    if (!BP)
      return TEXT("ERROR: missing ship Blueprint");
    BP->Modify();
    TArray<UEdGraph *> Graphs;
    BP->GetAllGraphs(Graphs);
    int32 AssetChanges = 0;
    for (auto *Graph : Graphs)
      for (UEdGraphNode *Node : Graph->Nodes)
        if (auto *Call = Cast<UK2Node_CallFunction>(Node)) {
          if (auto *Pin = Call->FindPin(TEXT("DrawDebugType"))) {
            if (!Pin->LinkedTo.IsEmpty())
              ++LinkedDebugPins;
            else if (Pin->DefaultValue != TEXT("None")) {
              Pin->DefaultValue = TEXT("None");
              ++AssetChanges;
            }
          }
          if (Call->FunctionReference.GetMemberName() == TEXT("PrintString"))
            for (const TCHAR *Name :
                 {TEXT("bPrintToScreen"), TEXT("bPrintToLog")})
              if (auto *Pin = Call->FindPin(Name);
                  Pin && Pin->LinkedTo.IsEmpty() &&
                  Pin->DefaultValue != TEXT("false")) {
                Pin->DefaultValue = TEXT("false");
                ++AssetChanges;
              }
        }
    if (AssetChanges) {
      FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
      FKismetEditorUtilities::CompileBlueprint(BP);
      if (BP->Status == BS_Error)
        return TEXT("ERROR: compile failed; assets not saved");
    }
    Changed += AssetChanges;
  }
  // Caller owns backup and save. Trace execution/data/force wiring is intact.
  return FString::Printf(TEXT("OK: changed=%d linked_debug_pins_preserved=%d; "
                              "compile passed; caller must save"),
                         Changed, LinkedDebugPins);
#else
  return TEXT("ERROR: editor operation");
#endif
}
