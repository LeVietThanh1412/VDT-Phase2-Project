#include "ShipEnemyNavigationComponent.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "GameFramework/Pawn.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace
{
constexpr const TCHAR* EnemyPath=TEXT("/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy");
bool IsPermittedBlueprint(const FString& P)
{return P==EnemyPath || P.StartsWith(TEXT("/Game/ShipEnemy/NavigationTests/"));}
UK2Node_CallFunction* FindCall(UEdGraph* G,FName Name)
{for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))if(C->FunctionReference.GetMemberName()==Name)return C;return nullptr;}
FString Check(UBlueprint* BP)
{
    if(!BP || !BP->ParentClass || BP->ParentClass->GetPathName()!=TEXT("/Game/Ship/BluePrint/BP_Ship.BP_Ship_C"))return TEXT("ERROR: wrong enemy parent class");
    for(FName F:{FName("ApplyAIForward"),FName("ApplyAIRudder"),FName("ScanOneDirection"),FName("ScanSurroundings"),FName("UpdateMoveToDestination"),FName("UpdateObstacleAvoidance")})
        if(!BP->GeneratedClass || !BP->GeneratedClass->FindFunctionByName(F))return FString::Printf(TEXT("ERROR: missing preserved function %s"),*F.ToString());
    USCS_Node* Component=nullptr;
    for(auto* N:BP->SimpleConstructionScript->GetAllNodes())if(N->ComponentClass==UShipEnemyNavigationComponent::StaticClass())Component=N;
    if(!Component)return TEXT("ERROR: navigation component absent");
    UEdGraph* G=FBlueprintEditorUtils::FindEventGraph(BP);
    auto* C=G?FindCall(G,GET_FUNCTION_NAME_CHECKED(UShipEnemyNavigationComponent,UpdateNavigation)):nullptr;
    if(!C || C->GetExecPin()->LinkedTo.Num()!=1 || C->GetThenPin()->LinkedTo.Num()!=1)return TEXT("ERROR: missing connected synchronous navigation gate");
    const auto* Branch=Cast<UK2Node_IfThenElse>(C->GetThenPin()->LinkedTo[0]->GetOwningNode());
    if(!Branch || !Branch->GetThenPin()->LinkedTo.Num() || !Branch->GetElsePin()->LinkedTo.Num())return TEXT("ERROR: incomplete legacy/new branch");
    auto* Force=FindCall(G,FName("ApplyAIForward"));auto* Legacy=FindCall(G,FName("UpdateMoveToDestination"));auto* Rudder=FindCall(G,FName("ApplyAIRudder"));
    if(!Force || !Legacy || !Rudder || Branch->GetThenPin()->LinkedTo[0]!=Force->GetExecPin() || Branch->GetElsePin()->LinkedTo[0]!=Legacy->GetExecPin() ||
       !Force->GetThenPin()->LinkedTo.Contains(Rudder->GetExecPin()))return TEXT("ERROR: force sequencing or legacy branch changed");
    int32 FCount=0,RCount=0;for(UEdGraphNode* N:G->Nodes)if(auto* Call=Cast<UK2Node_CallFunction>(N))
    {if(Call->FunctionReference.GetMemberName()==FName("ApplyAIForward"))++FCount;if(Call->FunctionReference.GetMemberName()==FName("ApplyAIRudder"))++RCount;}
    if(FCount!=1 || RCount!=1 || BP->Status==BS_Error)return TEXT("ERROR: duplicate force calls or Blueprint compile failure");
    return TEXT("OK: component attached; parent and six legacy functions preserved; exclusive command branch; one forward/rudder call pair; Blueprint compiled");
}
}
#endif

FString UShipEnemyNavigationComponent::InstallInEnemyBlueprint(bool bSave)
{return InstallNavigationBlueprint(TEXT("/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy"),bSave);}
FString UShipEnemyNavigationComponent::ValidateInstalledBlueprint()
{
#if WITH_EDITOR
    return Check(LoadObject<UBlueprint>(nullptr,EnemyPath));
#else
    return TEXT("ERROR: editor-only operation");
#endif
}
FString UShipEnemyNavigationComponent::CompileBattleBlueprints()
{
#if WITH_EDITOR
    FString Result=TEXT("OK: affected battle Blueprints compiled\n");
    for(const TCHAR* Path:{TEXT("/Game/Ship/BluePrint/BP_Ship.BP_Ship"),EnemyPath,
        TEXT("/Game/Ship/BluePrint/BP_ShipProjectile.BP_ShipProjectile"),TEXT("/Game/Ship/BluePrint/BP_ShipProjectileWithSpeed.BP_ShipProjectileWithSpeed"),
        TEXT("/Game/NavalBattle/UI/WBP_ShipHUD.WBP_ShipHUD"),TEXT("/Game/NavalBattle/UI/WBP_ShipHealthBar.WBP_ShipHealthBar"),TEXT("/Game/Ship/BluePrint/GMBP.GMBP")})
    {
        UBlueprint* BP=LoadObject<UBlueprint>(nullptr,Path);
        if(!BP)return FString::Printf(TEXT("ERROR: missing Blueprint %s"),Path);
        FKismetEditorUtilities::CompileBlueprint(BP);
        if(!BP->GeneratedClass || (BP->Status!=BS_UpToDate && BP->Status!=BS_UpToDateWithWarnings))
            return FString::Printf(TEXT("ERROR: compile failed for %s; inspect compiler log"),Path);
        Result+=FString::Printf(TEXT("%s: %s\n"),Path,BP->Status==BS_UpToDateWithWarnings?TEXT("compiled with warnings"):TEXT("compiled"));
    }
    return Result;
#else
    return TEXT("ERROR: editor-only operation");
#endif
}
FString UShipEnemyNavigationComponent::InspectMovementBlueprints()
{
#if WITH_EDITOR
    FString Result;
    for(const TCHAR* Path:{TEXT("/Game/Ship/BluePrint/BP_Ship.BP_Ship"),EnemyPath})
    {
        UBlueprint* BP=LoadObject<UBlueprint>(nullptr,Path);if(!BP)continue;
        TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
        for(UEdGraph* Graph:Graphs)
        {
            bool Physics=false;
            for(UEdGraphNode* Node:Graph->Nodes)if(auto* Call=Cast<UK2Node_CallFunction>(Node))
                if(Call->FunctionReference.GetMemberName().ToString().Contains(TEXT("Force")))Physics=true;
            if(!Physics)continue;
            Result+=FString::Printf(TEXT("\nASSET %s GRAPH %s\n"),Path,*Graph->GetName());
            for(UEdGraphNode* Node:Graph->Nodes)
            {
                Result+=FString::Printf(TEXT("NODE %s %s\n"),*Node->GetName(),*Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
                for(const UEdGraphPin* Pin:Node->Pins)
                {
                    Result+=FString::Printf(TEXT("  %s %s default=%s"),Pin->Direction==EGPD_Input?TEXT("IN"):TEXT("OUT"),*Pin->PinName.ToString(),*Pin->DefaultValue);
                    for(const auto* Link:Pin->LinkedTo)Result+=FString::Printf(TEXT(" -> %s.%s"),*Link->GetOwningNode()->GetName(),*Link->PinName.ToString());
                    Result+=TEXT("\n");
                }
            }
        }
    }
    return Result;
#else
    return TEXT("Editor only");
#endif
}
FString UShipEnemyNavigationComponent::InstallNavigationBlueprint(const FString& Path,bool bSave)
{
#if WITH_EDITOR
    if(!IsPermittedBlueprint(Path))return TEXT("ERROR: only active enemy or disposable navigation-test Blueprints may be edited");
    UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*Path);
    if(!BP || !BP->SimpleConstructionScript || !BP->ParentClass || BP->ParentClass->GetPathName()!=TEXT("/Game/Ship/BluePrint/BP_Ship.BP_Ship_C"))return TEXT("ERROR: enemy Blueprint/parent precondition failed");
    UEdGraph* G=FBlueprintEditorUtils::FindEventGraph(BP);
    if(!G)return TEXT("ERROR: no event graph");
    if(FindCall(G,GET_FUNCTION_NAME_CHECKED(UShipEnemyNavigationComponent,UpdateNavigation)))return Check(BP);
    auto* Legacy=FindCall(G,FName("UpdateMoveToDestination"));auto* Force=FindCall(G,FName("ApplyAIForward"));
    UK2Node_Event* Tick=nullptr;for(UEdGraphNode* N:G->Nodes)if(auto* E=Cast<UK2Node_Event>(N))if(E->EventReference.GetMemberName()==FName("ReceiveTick"))Tick=E;
    if(!Legacy || !Force || !Tick || Legacy->GetExecPin()->LinkedTo.Num()!=1 || !Tick->FindPin(TEXT("DeltaSeconds")))return TEXT("ERROR: unrecognized legacy Tick; no mutation performed");
    UEdGraphPin* Upstream=Legacy->GetExecPin()->LinkedTo[0];
    auto* Schema=CastChecked<UEdGraphSchema_K2>(G->GetSchema());
    BP->Modify();G->Modify();
    USimpleConstructionScript* SCS=BP->SimpleConstructionScript.Get();SCS->Modify();
    auto* Node=SCS->CreateNode(UShipEnemyNavigationComponent::StaticClass(),TEXT("EnemyNavigation"));
    if(!Node)return TEXT("ERROR: component template creation failed");
    SCS->AddNode(Node);
    auto* Settings=CastChecked<UShipEnemyNavigationComponent>(Node->ComponentTemplate);
    // Verified existing ship/TargetPoint coordinates; no map actors or spawn settings changed.
    Settings->PatrolPointA=FVector(107.29702857184641,20.975025161990075,0);
    // Original TargetPoint is inside the 200m exclusion zone. Leave it untouched;
    // use the nearest connected candidate validated against the actual OSM grid.
    Settings->PatrolPointB=FVector(107.29882028905828,20.984919743714805,0);
    Settings->PlayerShipClass=LoadClass<APawn>(nullptr,TEXT("/Game/Ship/BluePrint/BP_Ship.BP_Ship_C"));
    Settings->bNewNavigationEnabled=false; // Preserve gameplay until user explicitly enables the new system.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    if(BP->Status==BS_Error)return TEXT("ERROR: component-only Blueprint compile failed");
    auto* Get=NewObject<UK2Node_VariableGet>(G);G->AddNode(Get,false,false);
    Get->VariableReference.SetSelfMember(Node->GetVariableName());Get->CreateNewGuid();Get->AllocateDefaultPins();Get->NodePosX=Legacy->NodePosX-600;Get->NodePosY=Legacy->NodePosY+240;
    auto* Call=NewObject<UK2Node_CallFunction>(G);G->AddNode(Call,false,false);
    Call->SetFromFunction(StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UShipEnemyNavigationComponent,UpdateNavigation)));
    Call->CreateNewGuid();Call->AllocateDefaultPins();Call->NodePosX=Legacy->NodePosX-600;Call->NodePosY=Legacy->NodePosY;
    auto* Branch=NewObject<UK2Node_IfThenElse>(G);G->AddNode(Branch,false,false);Branch->CreateNewGuid();Branch->AllocateDefaultPins();Branch->NodePosX=Legacy->NodePosX-260;Branch->NodePosY=Legacy->NodePosY;
    Call->NodeComment=TEXT("Synchronous command arbitration. True bypasses legacy writers; physics functions below remain unchanged.");Call->bCommentBubbleVisible=true;
    bool OK=Schema->TryCreateConnection(Get->GetValuePin(),Call->FindPinChecked(UEdGraphSchema_K2::PN_Self));
    OK&=Schema->TryCreateConnection(Tick->FindPinChecked(TEXT("DeltaSeconds")),Call->FindPinChecked(TEXT("DeltaSeconds")));
    OK&=Schema->TryCreateConnection(Call->GetReturnValuePin(),Branch->GetConditionPin());
    OK&=Schema->TryCreateConnection(Call->GetThenPin(),Branch->GetExecPin());
    if(!OK)return TEXT("ERROR: new-node data wiring failed; asset NOT saved");
    Upstream->BreakLinkTo(Legacy->GetExecPin());
    OK&=Schema->TryCreateConnection(Upstream,Call->GetExecPin());
    OK&=Schema->TryCreateConnection(Branch->GetElsePin(),Legacy->GetExecPin());
    OK&=Schema->TryCreateConnection(Branch->GetThenPin(),Force->GetExecPin());
    if(!OK)return TEXT("ERROR: arbitration execution wiring failed; asset NOT saved");
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
    if(Log.NumErrors || BP->Status==BS_Error)return TEXT("ERROR: integrated enemy compile failed; asset NOT saved");
    const FString Result=Check(BP);if(!Result.StartsWith(TEXT("OK:")))return Result;
    if(bSave)
    {
        const FString Filename=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
        if(!UPackage::SavePackage(BP->GetOutermost(),BP,*Filename,Args))return TEXT("ERROR: enemy package save failed");
    }
    return Result+(bSave?TEXT("; SAVED"):TEXT("; UNSAVED probe"));
#else
    return TEXT("ERROR: editor-only operation");
#endif
}
