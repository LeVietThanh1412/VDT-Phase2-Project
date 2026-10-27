#include "ShipMovementComponent.h"
#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#endif
FString UShipMovementComponent::InstallPropulsionFilter()
{
#if WITH_EDITOR
    FString Result=TEXT("OK: original player/AI AddForce filtered and saved\n");
    for(const TCHAR* Path:{TEXT("/Game/Ship/BluePrint/BP_Ship.BP_Ship"),TEXT("/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy")})
    {
        auto* BP=LoadObject<UBlueprint>(nullptr,Path);if(!BP)return TEXT("ERROR: ship Blueprint missing");
        TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);UK2Node_CallFunction* Force=nullptr;
        for(auto* G:Graphs)for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))
            if(C->FunctionReference.GetMemberName()==TEXT("AddForce") && (G->GetFName()==TEXT("EventGraph") || G->GetFName()==TEXT("ApplyAIForward")))
            {if(Force)return TEXT("ERROR: expected exactly one inspected forward AddForce per Blueprint");Force=C;}
        if(!Force || !Force->FindPin(TEXT("Force")) || Force->FindPin(TEXT("Force"))->LinkedTo.Num()!=1)return TEXT("ERROR: original force input precondition failed");
        BP->Modify();auto* G=Force->GetGraph();G->Modify();auto* Input=Force->FindPin(TEXT("Force"));
        auto* Existing=Cast<UK2Node_CallFunction>(Input->LinkedTo[0]->GetOwningNode());
        if(!Existing || Existing->FunctionReference.GetMemberName()!=GET_FUNCTION_NAME_CHECKED(UShipMovementComponent,FilterPropulsion))
        {
            if(BP->GetFName()==TEXT("BP_Ship"))
            {
                bool Found=false;for(auto* N:BP->SimpleConstructionScript->GetAllNodes())Found|=N->ComponentClass==StaticClass();
                if(!Found)BP->SimpleConstructionScript->AddNode(BP->SimpleConstructionScript->CreateNode(StaticClass(),TEXT("ShipMovement")));
            }
            auto* Original=Input->LinkedTo[0];auto* Filter=NewObject<UK2Node_CallFunction>(G);G->AddNode(Filter,false,false);
            Filter->SetFromFunction(StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UShipMovementComponent,FilterPropulsion)));
            Filter->CreateNewGuid();Filter->AllocateDefaultPins();Filter->NodePosX=Force->NodePosX-250;Filter->NodePosY=Force->NodePosY+180;
            auto* Self=NewObject<UK2Node_Self>(G);G->AddNode(Self,false,false);Self->CreateNewGuid();Self->AllocateDefaultPins();
            Self->NodePosX=Filter->NodePosX-180;Self->NodePosY=Filter->NodePosY+100;
            Input->BreakAllPinLinks();const auto* Schema=GetDefault<UEdGraphSchema_K2>();
            if(!Schema->TryCreateConnection(Original,Filter->FindPin(TEXT("OriginalForce"))) ||
               !Schema->TryCreateConnection(Self->FindPin(TEXT("self")),Filter->FindPin(TEXT("Ship"))) ||
               !Schema->TryCreateConnection(Filter->GetReturnValuePin(),Input))return TEXT("ERROR: force filter wiring failed; not saved");
        }
        int32 Rudders=0;
        for(auto* Graph:Graphs)
        {
            // Snapshot prevents appending nodes from invalidating this traversal.
            const auto Nodes=Graph->Nodes;
            for(UEdGraphNode* Node:Nodes)if(auto* Call=Cast<UK2Node_CallFunction>(Node))
            {
                if(Call->FunctionReference.GetMemberName()!=TEXT("AddForceAtLocationLocal") ||
                    (Graph->GetFName()!=TEXT("EventGraph") && Graph->GetFName()!=TEXT("ApplyAIRudder")))continue;
                auto* Pin=Call->FindPin(TEXT("Force"));if(!Pin || Pin->LinkedTo.Num()!=1)return TEXT("ERROR: original rudder input precondition failed");
                ++Rudders;auto* Old=Cast<UK2Node_CallFunction>(Pin->LinkedTo[0]->GetOwningNode());
                auto* Location=Call->FindPin(TEXT("Location"));
                if(!Location)return TEXT("ERROR: original rudder location missing");
                auto* LocationOld=Location->LinkedTo.Num()?Cast<UK2Node_CallFunction>(Location->LinkedTo[0]->GetOwningNode()):nullptr;
                if(!LocationOld || LocationOld->FunctionReference.GetMemberName()!=GET_FUNCTION_NAME_CHECKED(UShipMovementComponent,FilterRudderLocation))
                {
                    auto* LocationFilter=NewObject<UK2Node_CallFunction>(Graph);Graph->AddNode(LocationFilter,false,false);
                    LocationFilter->SetFromFunction(StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UShipMovementComponent,FilterRudderLocation)));
                    LocationFilter->CreateNewGuid();LocationFilter->AllocateDefaultPins();LocationFilter->NodePosX=Call->NodePosX-260;LocationFilter->NodePosY=Call->NodePosY+350;
                    auto* InputLocation=LocationFilter->FindPin(TEXT("OriginalLocation"));
                    const auto* LocationSchema=GetDefault<UEdGraphSchema_K2>();
                    if(Location->LinkedTo.Num())
                    {auto* Source=Location->LinkedTo[0];Location->BreakAllPinLinks();if(!LocationSchema->TryCreateConnection(Source,InputLocation))return TEXT("ERROR: rudder location source");}
                    else {InputLocation->DefaultValue=Location->DefaultValue;InputLocation->DefaultObject=Location->DefaultObject;}
                    auto* LocationSelf=NewObject<UK2Node_Self>(Graph);Graph->AddNode(LocationSelf,false,false);LocationSelf->CreateNewGuid();LocationSelf->AllocateDefaultPins();
                    if(!LocationSchema->TryCreateConnection(LocationSelf->FindPin(TEXT("self")),LocationFilter->FindPin(TEXT("Ship"))) ||
                       !LocationSchema->TryCreateConnection(LocationFilter->GetReturnValuePin(),Location))return TEXT("ERROR: rudder location filter");
                }
                if(Old && Old->FunctionReference.GetMemberName()==GET_FUNCTION_NAME_CHECKED(UShipMovementComponent,FilterRudder))continue;
                Graph->Modify();auto* Original=Pin->LinkedTo[0];auto* Filter=NewObject<UK2Node_CallFunction>(Graph);Graph->AddNode(Filter,false,false);
                Filter->SetFromFunction(StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UShipMovementComponent,FilterRudder)));
                Filter->CreateNewGuid();Filter->AllocateDefaultPins();Filter->NodePosX=Call->NodePosX-250;Filter->NodePosY=Call->NodePosY+180;
                auto* Self=NewObject<UK2Node_Self>(Graph);Graph->AddNode(Self,false,false);Self->CreateNewGuid();Self->AllocateDefaultPins();Self->NodePosX=Filter->NodePosX-180;Self->NodePosY=Filter->NodePosY+100;
                Pin->BreakAllPinLinks();const auto* Schema=GetDefault<UEdGraphSchema_K2>();
                if(!Schema->TryCreateConnection(Original,Filter->FindPin(TEXT("OriginalForce"))) || !Schema->TryCreateConnection(Self->FindPin(TEXT("self")),Filter->FindPin(TEXT("Ship"))) ||
                    !Schema->TryCreateConnection(Filter->GetReturnValuePin(),Pin))return TEXT("ERROR: rudder filter wiring; not saved");
            }
        }
        if(Rudders!=(BP->GetFName()==TEXT("BP_Ship")?2:1))return TEXT("ERROR: expected two original manual and one AI rudder nodes");
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
        if(Log.NumErrors || !BP->GeneratedClass || BP->Status==BS_Error)return TEXT("ERROR: propulsion Blueprint compile; not saved");
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
        if(!UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args))return TEXT("ERROR: propulsion Blueprint save failed");
        Result+=FString(Path)+TEXT(": compiled/saved\n");
    }
    return Result;
#else
    return TEXT("ERROR: editor required");
#endif
}
FString UShipMovementComponent::CreateDefaultProfiles()
{
#if WITH_EDITOR
    FString Result=TEXT("OK: default profiles available\n");
    for(auto Type:{EShipHullClass::Small,EShipHullClass::Large,EShipHullClass::Medium})
    {
        const FString Name=Type==EShipHullClass::Large?TEXT("DA_LargeShip"):Type==EShipHullClass::Medium?TEXT("DA_MediumShip"):TEXT("DA_SmallShip");
        const FString Package=TEXT("/Game/NavalBattle/Profiles/")+Name;
        auto* P=LoadObject<UShipPhysicsProfile>(nullptr,*(Package+TEXT(".")+Name));
        if(P){Result+=Name+TEXT(": existing profile preserved\n");continue;}
        P=NewObject<UShipPhysicsProfile>(CreatePackage(*Package),FName(*Name),RF_Public|RF_Standalone);P->SetDefaults(Type);
        FAssetRegistryModule::AssetCreated(P);P->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
        if(!UPackage::SavePackage(P->GetOutermost(),P,*FPackageName::LongPackageNameToFilename(Package,FPackageName::GetAssetPackageExtension()),Args))return TEXT("ERROR: profile save failed");
        Result+=Name+TEXT(": created/saved\n");
    }
    return Result;
#else
    return TEXT("ERROR: editor required");
#endif
}
