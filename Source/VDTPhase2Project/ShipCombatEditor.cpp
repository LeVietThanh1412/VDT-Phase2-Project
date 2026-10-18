#include "ShipCombatComponent.h"
#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
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
