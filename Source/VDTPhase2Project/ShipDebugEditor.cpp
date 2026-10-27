#include "ShipCombatComponent.h"
#if WITH_EDITOR
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

FString UShipCombatLibrary::DisableSensorDebugDefaults()
{
#if WITH_EDITOR
    if(GEditor && GEditor->PlayWorld)return TEXT("ERROR: stop PIE before editing debug defaults");
    int32 Changed=0,LinkedDebugPins=0;
    for(const TCHAR* Path:{TEXT("/Game/Ship/BluePrint/BP_Ship.BP_Ship"),TEXT("/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy")})
    {
        auto* BP=LoadObject<UBlueprint>(nullptr,Path);if(!BP)return TEXT("ERROR: missing ship Blueprint");
        BP->Modify();TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);int32 AssetChanges=0;
        for(auto* Graph:Graphs)for(UEdGraphNode* Node:Graph->Nodes)if(auto* Call=Cast<UK2Node_CallFunction>(Node))
        {
            if(auto* Pin=Call->FindPin(TEXT("DrawDebugType")))
            {
                if(!Pin->LinkedTo.IsEmpty())++LinkedDebugPins;
                else if(Pin->DefaultValue!=TEXT("None")){Pin->DefaultValue=TEXT("None");++AssetChanges;}
            }
            if(Call->FunctionReference.GetMemberName()==TEXT("PrintString"))
                for(const TCHAR* Name:{TEXT("bPrintToScreen"),TEXT("bPrintToLog")})
                    if(auto* Pin=Call->FindPin(Name);Pin && Pin->LinkedTo.IsEmpty() && Pin->DefaultValue!=TEXT("false"))
                    {Pin->DefaultValue=TEXT("false");++AssetChanges;}
        }
        if(AssetChanges)
        {
            FBlueprintEditorUtils::MarkBlueprintAsModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
            if(BP->Status==BS_Error)return TEXT("ERROR: compile failed; assets not saved");
        }
        Changed+=AssetChanges;
    }
    // Caller owns backup and save. Trace execution/data/force wiring is intact.
    return FString::Printf(TEXT("OK: changed=%d linked_debug_pins_preserved=%d; compile passed; caller must save"),Changed,LinkedDebugPins);
#else
    return TEXT("ERROR: editor operation");
#endif
}
