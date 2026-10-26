#include "ShipCombatComponent.h"
#include "ShipProjectileDamageComponent.h"
#include "NavalBattleUI.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_GenericToText.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_VariableGet.h"
#include "K2Node_Self.h"
#include "K2Node_IfThenElse.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "GameFramework/Pawn.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
namespace
{
UEdGraphNode* Node(UEdGraph* G,const TCHAR* Name){for(UEdGraphNode* N:G->Nodes)if(N->GetFName()==FName(Name))return N;return nullptr;}
UK2Node_CallFunction* Call(UEdGraph* G,UClass* Class,FName Function,int32 X,int32 Y)
{
    auto* N=NewObject<UK2Node_CallFunction>(G);G->AddNode(N,false,false);N->SetFromFunction(Class->FindFunctionByName(Function));
    N->CreateNewGuid();N->AllocateDefaultPins();N->NodePosX=X;N->NodePosY=Y;return N;
}

UK2Node_VariableGet* Get(UEdGraph* G,FName Property,int32 X,int32 Y)
{
    auto* N=NewObject<UK2Node_VariableGet>(G);G->AddNode(N,false,false);N->VariableReference.SetSelfMember(Property);
    N->CreateNewGuid();N->AllocateDefaultPins();N->NodePosX=X;N->NodePosY=Y;return N;
}
void Component(UBlueprint* BP,UClass* Class,FName Name)
{
    for(auto* N:BP->SimpleConstructionScript->GetAllNodes())if(N->ComponentClass==Class)return;
    auto* N=BP->SimpleConstructionScript->CreateNode(Class,Name);BP->SimpleConstructionScript->AddNode(N);
}
bool Compile(UBlueprint* BP)
{FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);return Log.NumErrors==0 && BP->Status!=BS_Error;}
bool Save(UBlueprint* BP)
{
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    return UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
}
FString PatchMinimapTeamIcons()
{
    UBlueprint* Marker=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Game/UI/Minimap/Widgets/WBP_ShipMarker.WBP_ShipMarker"));
    if(!Marker)return TEXT("ERROR: WBP_ShipMarker missing");
    if(Marker->ParentClass==UShipMarkerWidget::StaticClass())return TEXT("OK: minimap team marker icons already installed");
    Marker->Modify();
    Marker->ParentClass=UShipMarkerWidget::StaticClass();
    FBlueprintEditorUtils::RefreshAllNodes(Marker);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Marker);
    if(!Compile(Marker) || !Save(Marker))return TEXT("ERROR: WBP_ShipMarker team icon reparent compile/save");
    return TEXT("OK: minimap team marker icons installed");
}
FString PatchMinimapShipNames()
{
    UBlueprint* Marker=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Game/UI/Minimap/Widgets/WBP_ShipMarker.WBP_ShipMarker"));
    if(!Marker)return TEXT("ERROR: WBP_ShipMarker missing");
    UEdGraph* MG=FBlueprintEditorUtils::FindEventGraph(Marker);
    if(!MG)return TEXT("ERROR: WBP_ShipMarker event graph missing");
    bool NameInstalled=false;
    for(UEdGraphNode* N:MG->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))
        if(C->FunctionReference.GetMemberName()==GET_FUNCTION_NAME_CHECKED(UShipCombatLibrary,GetShipNameForObject))NameInstalled=true;
    if(NameInstalled)
    {
        UK2Node_CallFunction* NameNode=nullptr;
        for(UEdGraphNode* N:MG->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))
            if(C->FunctionReference.GetMemberName()==GET_FUNCTION_NAME_CHECKED(UShipCombatLibrary,GetShipNameForObject)){NameNode=C;break;}
        UEdGraphPin* NameInput=NameNode?NameNode->FindPin(TEXT("Object")):nullptr;
        if(NameInput && NameInput->LinkedTo.Num()>0)return TEXT("OK: minimap names already installed");
        UK2Node_VariableGet* ShipVar=nullptr;
        for(UEdGraphNode* N:MG->Nodes)if(auto* V=Cast<UK2Node_VariableGet>(N))
            if(V->VariableReference.GetMemberName()==TEXT("ShipActor")){ShipVar=V;break;}
        if(!NameInput || !ShipVar)return TEXT("ERROR: minimap name repair prerequisites missing");
        Marker->Modify();MG->Modify();
        if(!GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(ShipVar->GetValuePin(),NameInput))
            return TEXT("ERROR: minimap name repair connection failed");
        if(!Compile(Marker) || !Save(Marker))return TEXT("ERROR: WBP_ShipMarker repair compile/save");
        return TEXT("OK: minimap names repaired");
    }
    UK2Node_CallFunction* SetText=nullptr;
    for(UEdGraphNode* N:MG->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))
        if(C->FunctionReference.GetMemberName()==TEXT("SetText"))
        {
            for(UEdGraphPin* P:C->Pins)if(P->Direction==EGPD_Input && P->LinkedTo.Num()==1)
                if(auto* V=Cast<UK2Node_VariableGet>(P->LinkedTo[0]->GetOwningNode()))
                    if(V->VariableReference.GetMemberName()==TEXT("Text_ShipName")){SetText=C;break;}
            if(SetText)break;
        }
    if(!SetText)return TEXT("ERROR: WBP_ShipMarker SetText(Text_ShipName) not found");
    UEdGraphPin* TextInput=nullptr;
    for(UEdGraphPin* P:SetText->Pins)if(P->Direction==EGPD_Input && P->LinkedTo.Num()==1)
        if(Cast<UK2Node_GenericToText>(P->LinkedTo[0]->GetOwningNode())){TextInput=P;break;}
    if(!TextInput)return TEXT("ERROR: WBP_ShipMarker string-to-text input not found");
    UEdGraphNode* Conv=TextInput->LinkedTo[0]->GetOwningNode();
    UEdGraphPin* ConvInput=nullptr;UK2Node_CallFunction* OldName=nullptr;
    for(UEdGraphPin* P:Conv->Pins)if(P->Direction==EGPD_Input && P->LinkedTo.Num()==1)
        if(auto* C=Cast<UK2Node_CallFunction>(P->LinkedTo[0]->GetOwningNode())){ConvInput=P;OldName=C;break;}
    if(!ConvInput || !OldName)return TEXT("ERROR: WBP_ShipMarker name source not found");
    UEdGraphPin* OldInput=nullptr;
    for(UEdGraphPin* P:OldName->Pins)if(P->Direction==EGPD_Input && P->LinkedTo.Num()==1){OldInput=P;break;}
    if(!OldInput)return TEXT("ERROR: WBP_ShipMarker ShipActor input not found");
    UEdGraphPin* Source=OldInput->LinkedTo[0];
    UFunction* NameFunction=UShipCombatLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UShipCombatLibrary,GetShipNameForObject));
    if(!NameFunction)return TEXT("ERROR: GetShipNameForObject not reflected; restart the editor and retry");
    Marker->Modify();MG->Modify();
    auto* NewName=Call(MG,UShipCombatLibrary::StaticClass(),GET_FUNCTION_NAME_CHECKED(UShipCombatLibrary,GetShipNameForObject),OldName->NodePosX,OldName->NodePosY);
    if(!NewName || !NewName->GetReturnValuePin())return TEXT("ERROR: minimap name function node failed");
    UEdGraphPin* NewInput=NewName->FindPin(TEXT("Object"));
    if(!NewInput)return TEXT("ERROR: minimap name Object pin missing");
    FBlueprintEditorUtils::RemoveNode(Marker,OldName,true);
    const UEdGraphSchema_K2* Schema=GetDefault<UEdGraphSchema_K2>();
    const bool OKSource=Schema->TryCreateConnection(Source,NewInput);
    ConvInput->BreakAllPinLinks();
    const bool OKReturn=Schema->TryCreateConnection(NewName->GetReturnValuePin(),ConvInput);
    if(!OKSource || !OKReturn)return TEXT("ERROR: minimap name wiring failed; WBP_ShipMarker NOT saved");
    if(!Compile(Marker) || !Save(Marker))return TEXT("ERROR: WBP_ShipMarker compile/save");
    return TEXT("OK: minimap marker names use ship identity");
}
}
#endif
FString UShipCombatLibrary::InstallCombatIntegration()
{
#if WITH_EDITOR
    UBlueprint* Ship=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Ship/BluePrint/BP_Ship.BP_Ship"));
    UBlueprint* Enemy=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy"));
    UBlueprint* Anim=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Ship/BluePrint/ABP_Ship.ABP_Ship"));
    if(!Ship || !Enemy || !Anim)return TEXT("ERROR: original Blueprint assets missing");
    UEdGraph* G=FBlueprintEditorUtils::FindEventGraph(Ship);UEdGraph* A=FBlueprintEditorUtils::FindEventGraph(Anim);
    if(!G || !A || !Node(G,TEXT("K2Node_SpawnActorFromClass_0")) || !Node(G,TEXT("K2Node_SpawnActorFromClass_3")) || !Node(A,TEXT("K2Node_CallFunction_4")))return TEXT("ERROR: inspected graph precondition failed");
    for(UEdGraphNode* N:G->Nodes)if(auto* Event=Cast<UK2Node_CustomEvent>(N))if(Event->CustomFunctionName==TEXT("ExecuteWeaponShot"))return TEXT("OK: integration already installed; no edits");
    Ship->Modify();Enemy->Modify();Anim->Modify();G->Modify();A->Modify();
    Component(Ship,UShipWeaponComponent::StaticClass(),TEXT("ShipWeapon"));
    Component(Enemy,UShipCombatComponent::StaticClass(),TEXT("EnemyCombat"));
    if(!Compile(Ship) || !Compile(Enemy))return TEXT("ERROR: component Blueprint compile failed; NOT saved");
    const UEdGraphSchema_K2* Schema=GetDefault<UEdGraphSchema_K2>();bool OK=true;
    auto Wire=[&](UEdGraphPin* P,UEdGraphPin* Q){if(!P || !Q){OK=false;return;}OK &= Schema->TryCreateConnection(P,Q);};
    auto* Weapon=Get(G,TEXT("ShipWeapon"),2700,3500);
    auto* Player=Call(G,UShipWeaponComponent::StaticClass(),GET_FUNCTION_NAME_CHECKED(UShipWeaponComponent,FirePlayer),2900,3400);
    auto* Gate=Node(G,TEXT("K2Node_IfThenElse_8"));auto* Branch=Node(G,TEXT("K2Node_IfThenElse_9"));
    Gate->FindPin(TEXT("then"))->BreakAllPinLinks();
    Wire(Gate->FindPin(TEXT("then")),Player->GetExecPin());Wire(Weapon->GetValuePin(),Player->FindPin(TEXT("self")));
    auto* Event=NewObject<UK2Node_CustomEvent>(G);G->AddNode(Event,false,false);Event->CustomFunctionName=TEXT("ExecuteWeaponShot");Event->CreateNewGuid();Event->AllocateDefaultPins();Event->NodePosX=2800;Event->NodePosY=3900;
    FEdGraphPinType VectorType;VectorType.PinCategory=UEdGraphSchema_K2::PC_Struct;VectorType.PinSubCategoryObject=TBaseStructure<FVector>::Get();
    FEdGraphPinType BoolType;BoolType.PinCategory=UEdGraphSchema_K2::PC_Boolean;
    Event->CreateUserDefinedPin(TEXT("LaunchVelocity"),VectorType,EGPD_Output);
    Event->CreateUserDefinedPin(TEXT("bBallistic"),BoolType,EGPD_Output);
    Wire(Event->FindPin(TEXT("then")),Branch->FindPin(TEXT("execute")));
    Branch->FindPin(TEXT("Condition"))->BreakAllPinLinks();Wire(Event->FindPin(TEXT("bBallistic")),Branch->FindPin(TEXT("Condition")));
    auto* Ballistic=Node(G,TEXT("K2Node_SpawnActorFromClass_3"));auto* Direct=Node(G,TEXT("K2Node_SpawnActorFromClass_0"));
    Branch->FindPin(TEXT("then"))->BreakAllPinLinks();Ballistic->FindPin(TEXT("execute"))->BreakAllPinLinks();
    Wire(Branch->FindPin(TEXT("then")),Ballistic->FindPin(TEXT("execute")));
    Ballistic->FindPin(TEXT("Speed"))->BreakAllPinLinks();Wire(Event->FindPin(TEXT("LaunchVelocity")),Ballistic->FindPin(TEXT("Speed")));
    auto* Self=NewObject<UK2Node_Self>(G);G->AddNode(Self,false,false);Self->CreateNewGuid();Self->AllocateDefaultPins();Self->NodePosX=3050;Self->NodePosY=4200;
    for(auto* Spawn:{Direct,Ballistic})
    {
        auto* Then=Spawn->FindPin(TEXT("then"));const auto Downstream=Then->LinkedTo;Then->BreakAllPinLinks();
        auto* Finish=Call(G,UShipWeaponComponent::StaticClass(),GET_FUNCTION_NAME_CHECKED(UShipWeaponComponent,FinishShot),Spawn->NodePosX+400,Spawn->NodePosY);
        Wire(Weapon->GetValuePin(),Finish->FindPin(TEXT("self")));Wire(Spawn->FindPin(TEXT("ReturnValue")),Finish->FindPin(TEXT("Projectile")));
        Wire(Then,Finish->GetExecPin());for(auto* P:Downstream)Wire(Finish->GetThenPin(),P);
        Wire(Self->FindPin(TEXT("self")),Spawn->FindPin(TEXT("Owner")));Wire(Self->FindPin(TEXT("self")),Spawn->FindPin(TEXT("Instigator")));
        Spawn->FindPin(TEXT("CollisionHandlingOverride"))->DefaultValue=TEXT("AlwaysSpawn");
    }
    Event->NodeComment=TEXT("Shared weapon entry: original projectile classes, muzzle transforms, Niagara, sound, recoil and CanShoot timeline retained. Player and AI call ShipWeapon.");Event->bCommentBubbleVisible=true;
    // Keep manual aiming byte-for-byte upstream; select AI values only before the
    // existing GunPitch/GunYaw setters. No new bone controller nodes are created.
    auto* Delta=Node(A,TEXT("K2Node_CallFunction_4"));auto* Owner=Node(A,TEXT("K2Node_VariableGet_3"));
    auto* Select=Call(A,UShipCombatLibrary::StaticClass(),GET_FUNCTION_NAME_CHECKED(UShipCombatLibrary,SelectGunAim),Delta->NodePosX+300,Delta->NodePosY+300);
    Schema->SplitPin(Select->FindPin(TEXT("ManualAim")),false);Schema->SplitPin(Select->GetReturnValuePin(),false);
    Wire(Owner->FindPin(TEXT("BP Ship")),Select->FindPin(TEXT("Ship")));
    for(const TCHAR* Axis:{TEXT("Pitch"),TEXT("Yaw"),TEXT("Roll")})
    {
        auto* Old=Delta->FindPin(FName(*(FString(TEXT("ReturnValue_"))+Axis)));
        if(!Old)continue;const auto Destinations=Old->LinkedTo;Old->BreakAllPinLinks();
        Wire(Old,Select->FindPin(FName(*(FString(TEXT("ManualAim_"))+Axis))));
        for(auto* P:Destinations)Wire(Select->FindPin(FName(*(FString(TEXT("ReturnValue_"))+Axis))),P);
    }
    if(!OK)return TEXT("ERROR: shared firing/animation wiring failed; assets NOT saved");
    if(!Compile(Ship) || !Compile(Enemy) || !Compile(Anim))return TEXT("ERROR: shared integration compile failed; assets NOT saved");
    if(!Save(Ship) || !Save(Enemy) || !Save(Anim))return TEXT("ERROR: save failed");
    return TEXT("OK: shared original BP spawn/FX/recoil graph installed; manual aim preserved; AI angle selection installed; all three Blueprints compiled and saved");
#else
    return TEXT("ERROR: editor operation");
#endif
}

FString UShipCombatLibrary::InstallBattlePresentation()
{
#if WITH_EDITOR
    const FString MarkerResult=PatchMinimapTeamIcons();
    if(MarkerResult.StartsWith(TEXT("ERROR")))return MarkerResult;
    const FString MinimapResult=PatchMinimapShipNames();
    if(MinimapResult.StartsWith(TEXT("ERROR")))return MinimapResult;
    if(auto* Enemy=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy")))
    {
        Enemy->Modify();TArray<UEdGraph*> Graphs;Enemy->GetAllGraphs(Graphs);
        for(auto* G:Graphs)for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))
        {
            if(C->FunctionReference.GetMemberName()==TEXT("PrintString"))
                for(auto Name:{TEXT("bPrintToScreen"),TEXT("bPrintToLog")})if(auto* P=C->FindPin(Name);P && P->LinkedTo.IsEmpty())P->DefaultValue=TEXT("false");
            if(auto* P=C->FindPin(TEXT("DrawDebugType"));P && P->LinkedTo.IsEmpty())P->DefaultValue=TEXT("None");
        }
        if(!Compile(Enemy) || !Save(Enemy))return TEXT("ERROR: enemy debug defaults compile/save");
    }
    for(const TCHAR* Path:{TEXT("/Game/Ship/BluePrint/BP_ShipProjectile.BP_ShipProjectile"),TEXT("/Game/Ship/BluePrint/BP_ShipProjectileWithSpeed.BP_ShipProjectileWithSpeed")})
    {
        auto* BP=LoadObject<UBlueprint>(nullptr,Path);if(!BP)return TEXT("ERROR: projectile missing");
        BP->Modify();Component(BP,UShipProjectileDamageComponent::StaticClass(),TEXT("TeamDamage"));
        auto* G=FBlueprintEditorUtils::FindEventGraph(BP);auto* Old=Node(G,TEXT("K2Node_CallFunction_1"));
        bool Installed=false;
        UK2Node_CallFunction* OldImpact=nullptr;UEdGraph* ImpactGraph=nullptr;
        {
            TArray<UEdGraph*> AllGraphs;BP->GetAllGraphs(AllGraphs);
            for(UEdGraph* AG:AllGraphs)for(UEdGraphNode* N:AG->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))
                if(C->FunctionReference.GetMemberName()==GET_FUNCTION_NAME_CHECKED(UShipCombatLibrary,ApplyProjectileImpact))
                {OldImpact=C;ImpactGraph=AG;break;}
        }
        if(OldImpact)
        {
            // Rebuild the old pre-static impact node from scratch; a stale
            // Target pin on the existing node can survive SetFromFunction.
            auto* Impact=Call(ImpactGraph,UShipCombatLibrary::StaticClass(),GET_FUNCTION_NAME_CHECKED(UShipCombatLibrary,ApplyProjectileImpact),OldImpact->NodePosX,OldImpact->NodePosY);
            auto* Schema=GetDefault<UEdGraphSchema_K2>();bool OK=true;
            auto Wire=[&](UEdGraphPin* A,UEdGraphPin* B){if(!A || !B){OK=false;return;}OK &= Schema->TryCreateConnection(A,B);};
            auto* OldExec=OldImpact->GetExecPin();auto* NewExec=Impact->GetExecPin();const auto ExecInputs=OldExec->LinkedTo;OldExec->BreakAllPinLinks();for(auto* P:ExecInputs)Wire(P,NewExec);
            auto* OldThen=OldImpact->GetThenPin();auto* NewThen=Impact->GetThenPin();const auto ExecOutputs=OldThen->LinkedTo;OldThen->BreakAllPinLinks();for(auto* P:ExecOutputs)Wire(NewThen,P);
            for(const TCHAR* PinName:{TEXT("Projectile"),TEXT("Impact")})
            {
                auto* OldPin=OldImpact->FindPin(PinName);auto* NewPin=Impact->FindPin(PinName);
                if(OldPin && NewPin){const auto Links=OldPin->LinkedTo;OldPin->BreakAllPinLinks();for(auto* P:Links)Wire(P,NewPin);}
            }
            FBlueprintEditorUtils::RemoveNode(BP,OldImpact,true);
            if(!OK)return TEXT("ERROR: projectile impact rewire");
            Installed=true;
        }
        if(!Installed)
        {
            if(!Old)return TEXT("ERROR: expected original radial damage node missing");
            auto* Impact=Call(G,UShipCombatLibrary::StaticClass(),GET_FUNCTION_NAME_CHECKED(UShipCombatLibrary,ApplyProjectileImpact),Old->NodePosX,Old->NodePosY+240);
            auto* Schema=GetDefault<UEdGraphSchema_K2>();bool OK=true;
            auto Wire=[&](UEdGraphPin* A,UEdGraphPin* B){if(!A || !B){OK=false;return;}OK &= Schema->TryCreateConnection(A,B);};
            auto* Input=Old->FindPin(TEXT("execute"));auto* Output=Old->FindPin(TEXT("then"));const auto Inputs=Input->LinkedTo;const auto Outputs=Output->LinkedTo;
            Input->BreakAllPinLinks();Output->BreakAllPinLinks();for(auto* P:Inputs)Wire(P,Impact->GetExecPin());for(auto* P:Outputs)Wire(Impact->GetThenPin(),P);
            auto* Self=Node(G,TEXT("K2Node_Self_0"));auto* Hit=Node(G,TEXT("K2Node_ComponentBoundEvent_0"));
            Wire(Self?Self->FindPin(TEXT("self")):nullptr,Impact->FindPin(TEXT("Projectile")));
            Wire(Hit?Hit->FindPin(TEXT("ImpactResult")):nullptr,Impact->FindPin(TEXT("Impact")));
            // Unused old radial node is removed to prevent accidental unfiltered AOE.
            FBlueprintEditorUtils::RemoveNode(BP,Old,true);
            if(!OK)return TEXT("ERROR: projectile impact wiring");
        }
        if(!Compile(BP) || !Save(BP))return TEXT("ERROR: projectile compile/save");
    }
    struct FWidgetDefinition {const TCHAR* Name;UClass* Parent;};
    for(auto D:{FWidgetDefinition{TEXT("WBP_ShipHealthBar"),UShipHealthBarWidget::StaticClass()}})
    {
        FString PackageName=FString(TEXT("/Game/NavalBattle/UI/"))+D.Name;
        auto* BP=LoadObject<UBlueprint>(nullptr,*(PackageName+TEXT(".")+D.Name));
        if(!BP)BP=FKismetEditorUtilities::CreateBlueprint(D.Parent,CreatePackage(*PackageName),FName(D.Name),BPTYPE_Normal,UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass(),TEXT("NavalBattle"));
        if(!BP || !Compile(BP) || !Save(BP))return TEXT("ERROR: widget compile/save");
    }
    return TEXT("OK: both original projectiles use cached team damage; WBP_ShipHealthBar compiled and saved; ")+MarkerResult+TEXT("; ")+MinimapResult;
#else
    return TEXT("ERROR: editor required");
#endif
}
