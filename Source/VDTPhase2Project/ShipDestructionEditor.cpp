#include "ShipCombatComponent.h"
#if WITH_EDITOR
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "NiagaraSpriteRendererProperties.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
namespace
{
constexpr const TCHAR* OriginalFX=TEXT("/Game/Ship/Assets/VigilanteContent/Vehicles/West_Ship_DDG102/FX/NS_West_Ship_DDG102_Damaged.NS_West_Ship_DDG102_Damaged");
constexpr const TCHAR* WreckPackage=TEXT("/Game/NavalBattle/FX/NS_ShipBurn");
constexpr const TCHAR* WreckPath=TEXT("/Game/NavalBattle/FX/NS_ShipBurn.NS_ShipBurn");
int32 FloatCount(const FNiagaraVariable& V)
{
    const auto T=V.GetType();
    return T==FNiagaraTypeDefinition::GetFloatDef() || T==FNiagaraTypeDefinition::GetVec2Def() || T==FNiagaraTypeDefinition::GetVec3Def() || T==FNiagaraTypeDefinition::GetVec4Def() || T==FNiagaraTypeDefinition::GetColorDef()?V.GetSizeInBytes()/sizeof(float):0;
}
}
#endif
FString UShipWeaponComponent::InspectWreckEffectParameters()
{
#if WITH_EDITOR
    auto* S=LoadObject<UNiagaraSystem>(nullptr,WreckPath);if(!S)S=LoadObject<UNiagaraSystem>(nullptr,OriginalFX);
    if(!S)return TEXT("ERROR: existing ship damage effect missing");
    TArray<TSharedPtr<FJsonValue>> Rows;
    for(const auto& H:S->GetEmitterHandles())if(auto* E=H.GetEmitterData())
    {
        TArray<UNiagaraScript*> Scripts;E->GetScripts(Scripts,false,false);
        for(auto* Script:Scripts)
        {
            TArray<FNiagaraVariable> Variables;Script->RapidIterationParameters.GetParameters(Variables);
            for(const auto& V:Variables)if(const int32 Count=FloatCount(V))
            {
                const float* Data=reinterpret_cast<const float*>(Script->RapidIterationParameters.GetParameterData(V));if(!Data)continue;
                auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("emitter"),H.GetName().ToString());Row->SetStringField(TEXT("script"),Script->GetName());Row->SetStringField(TEXT("parameter"),V.GetName().ToString());
                TArray<TSharedPtr<FJsonValue>> Values;for(int32 I=0;I<Count;++I)Values.Add(MakeShared<FJsonValueNumber>(Data[I]));Row->SetArrayField(TEXT("value"),Values);Rows.Add(MakeShared<FJsonValueObject>(Row));
            }
        }
    }
    FString Result;auto Writer=TJsonWriterFactory<>::Create(&Result);FJsonSerializer::Serialize(Rows,Writer);return Result;
#else
    return TEXT("ERROR: editor required");
#endif
}
FString UShipWeaponComponent::ConfigureWreckEffect(const FString& Json)
{
#if WITH_EDITOR
    TSharedPtr<FJsonObject> Overrides;auto Reader=TJsonReaderFactory<>::Create(Json);
    if(!FJsonSerializer::Deserialize(Reader,Overrides) || !Overrides.IsValid() || Overrides->Values.IsEmpty())return TEXT("ERROR: nonempty exact parameter/value JSON required");
    FString FireMaterialPath;Overrides->TryGetStringField(TEXT("__fireMaterial"),FireMaterialPath);Overrides->RemoveField(TEXT("__fireMaterial"));
    auto* FireMaterial=FireMaterialPath.IsEmpty()?nullptr:LoadObject<UMaterialInterface>(nullptr,*FireMaterialPath);
    if(!FireMaterialPath.IsEmpty() && !FireMaterial)return TEXT("ERROR: fire material missing");
    auto* S=LoadObject<UNiagaraSystem>(nullptr,WreckPath);
    if(!S)
    {
        auto* Original=LoadObject<UNiagaraSystem>(nullptr,OriginalFX);if(!Original)return TEXT("ERROR: original ship effect missing");
        S=Cast<UNiagaraSystem>(StaticDuplicateObject(Original,CreatePackage(WreckPackage),TEXT("NS_ShipBurn")));
        if(!S)return TEXT("ERROR: duplicate failed");S->SetFlags(RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(S);
    }
    S->Modify();TSet<FString> Applied;int32 Changed=0;
    for(const auto& H:S->GetEmitterHandles())if(auto* E=H.GetEmitterData())
    {
        for(auto* R:E->GetRenderers())if(auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(R))
        {
            Sprite->Modify();Sprite->FacingMode=ENiagaraSpriteFacingMode::FaceCamera;Sprite->Alignment=ENiagaraSpriteAlignment::Unaligned;
            Sprite->bUseMaterialCutoutTexture=false;Sprite->CutoutTexture=nullptr;Sprite->UpdateCutoutTexture();
            Sprite->bEnableCameraDistanceCulling=false;
            if(FireMaterial && H.GetName()==TEXT("AftVentFire"))Sprite->Material=FireMaterial;
            ++Changed;
        }
        TArray<UNiagaraScript*> Scripts;E->GetScripts(Scripts,false,false);
        for(auto* Script:Scripts)
        {
            TArray<FNiagaraVariable> Variables;Script->RapidIterationParameters.GetParameters(Variables);
            for(const auto& V:Variables)
            {
                const auto* Value=Overrides->Values.Find(V.GetName().ToString());if(!Value)continue;
                const int32 Count=FloatCount(V);const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
                if(!Count || !(*Value)->TryGetArray(Values) || Values->Num()!=Count)return TEXT("ERROR: exact parameter type/array length mismatch; copy not saved");
                TArray<float> Data;for(const auto& F:*Values){double Number=0;if(!F->TryGetNumber(Number) || !FMath::IsFinite(Number))return TEXT("ERROR: invalid parameter number");Data.Add(float(Number));}
                Script->Modify();if(!Script->RapidIterationParameters.SetParameterData(reinterpret_cast<const uint8*>(Data.GetData()),V,false))return TEXT("ERROR: parameter update failed");
                Applied.Add(V.GetName().ToString());++Changed;
            }
        }
    }
    if(Applied.Num()!=Overrides->Values.Num())return TEXT("ERROR: requested parameter not found; copy not saved");
    S->RequestCompile(true);S->WaitForCompilationComplete(true,false);
    if(!S->IsReadyToRun())return TEXT("ERROR: copied Niagara system not ready after compile; not saved");
    S->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    if(!UPackage::SavePackage(S->GetOutermost(),S,*FPackageName::LongPackageNameToFilename(WreckPackage,FPackageName::GetAssetPackageExtension()),Args))return TEXT("ERROR: copied effect save failed");
    return FString::Printf(TEXT("OK: tuned copy compiled/saved; %d updates; original effect preserved"),Changed);
#else
    return TEXT("ERROR: editor required");
#endif
}
