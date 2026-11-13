#include "Ships/ShipMovementComponent.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "BlueprintEditorLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/Texture2D.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StaticMeshAttributes.h"
#include "UObject/SavePackage.h"
#endif

FString UShipMovementComponent::InspectArchetypeSources() {
#if WITH_EDITOR
  auto *Small = LoadObject<UStaticMesh>(
      nullptr,
      TEXT("/Game/ShipSmall/Asset/sweedenmilitaryboat.sweedenmilitaryboat"));
  auto *Original = LoadObject<USkeletalMesh>(
      nullptr,
      TEXT("/Game/Ship/Assets/VigilanteContent/Vehicles/West_Ship_DDG102/"
           "Sk_West_Ship_DDG102.Sk_West_Ship_DDG102"));
  if (!Small || !Original)
    return TEXT("ERROR: imported boat or original skeletal mesh missing");
  auto Root = MakeShared<FJsonObject>();
  Root->SetStringField(TEXT("small_type"), Small->GetClass()->GetName());
  Root->SetStringField(TEXT("small_bounds_cm"),
                       Small->GetBoundingBox().ToString());
  Root->SetBoolField(TEXT("small_has_skeleton"), false);
  Root->SetNumberField(TEXT("small_lods"), Small->GetNumLODs());
  Root->SetNumberField(TEXT("small_fallback_lod0_vertices"),
                       Small->GetNumVertices(0));
  TArray<TSharedPtr<FJsonValue>> Sockets;
  for (const UStaticMeshSocket *Socket : Small->Sockets)
    if (Socket)
      Sockets.Add(MakeShared<FJsonValueString>(Socket->SocketName.ToString()));
  Root->SetArrayField(TEXT("small_sockets"), Sockets);
  if (auto *Body = Small->GetBodySetup()) {
    Root->SetNumberField(TEXT("small_simple_collision_elements"),
                         Body->AggGeom.GetElementCount());
    Root->SetNumberField(TEXT("small_collision_trace_flag"),
                         int32(Body->CollisionTraceFlag));
  }
  if (auto *Texture = LoadObject<UTexture2D>(
          nullptr, TEXT("/Game/ShipSmall/Asset/shaded.shaded"))) {
    Root->SetNumberField(TEXT("texture_source_width"),
                         Texture->Source.GetSizeX());
    Root->SetNumberField(TEXT("texture_source_height"),
                         Texture->Source.GetSizeY());
    Root->SetNumberField(TEXT("texture_max_size"), Texture->MaxTextureSize);
  }
  if (auto *Material = LoadObject<UMaterial>(
          nullptr, TEXT("/Game/ShipSmall/Asset/model.model"))) {
    TArray<TSharedPtr<FJsonValue>> Expressions;
    for (UMaterialExpression *Expression : Material->GetExpressions()) {
      auto Entry = MakeShared<FJsonObject>();
      Entry->SetStringField(TEXT("class"), Expression->GetClass()->GetName());
      if (const auto *Sample =
              Cast<UMaterialExpressionTextureSample>(Expression))
        Entry->SetStringField(TEXT("texture"),
                              GetPathNameSafe(Sample->Texture));
      Expressions.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Root->SetArrayField(TEXT("small_material_expressions"), Expressions);
  }
  const auto &Skeleton = Original->GetRefSkeleton();
  TArray<TSharedPtr<FJsonValue>> Bones;
  for (int32 Index = 0; Index < Skeleton.GetNum(); ++Index) {
    auto Entry = MakeShared<FJsonObject>();
    Entry->SetNumberField(TEXT("index"), Index);
    Entry->SetStringField(TEXT("name"), Skeleton.GetBoneName(Index).ToString());
    Entry->SetNumberField(TEXT("parent"), Skeleton.GetParentIndex(Index));
    Entry->SetStringField(TEXT("reference_local_pose"),
                          Skeleton.GetRefBonePose()[Index].ToString());
    Bones.Add(MakeShared<FJsonValueObject>(Entry));
  }
  Root->SetArrayField(TEXT("original_bones"), Bones);
  if (auto *Socket = Original->FindSocket(TEXT("front_gun_v_socket"))) {
    Root->SetStringField(TEXT("original_muzzle_bone"),
                         Socket->BoneName.ToString());
    Root->SetStringField(TEXT("original_muzzle_relative_position"),
                         Socket->RelativeLocation.ToString());
    Root->SetStringField(TEXT("original_muzzle_relative_rotation"),
                         Socket->RelativeRotation.ToString());
  }
  TArray<int64> Dominant;
  Dominant.Init(0, Skeleton.GetNum());
  TArray<TSharedPtr<FJsonValue>> Sections;
  if (const auto *Imported = Original->GetImportedModel();
      Imported && Imported->LODModels.Num()) {
    const auto &LOD = Imported->LODModels[0];
    Root->SetNumberField(TEXT("original_imported_lod0_indices"),
                         LOD.IndexBuffer.Num());
    for (const auto &Section : LOD.Sections) {
      auto Entry = MakeShared<FJsonObject>();
      Entry->SetNumberField(TEXT("material"), Section.MaterialIndex);
      Entry->SetNumberField(TEXT("vertices"), Section.SoftVertices.Num());
      Entry->SetNumberField(TEXT("triangles"), Section.NumTriangles);
      for (const auto &Vertex : Section.SoftVertices) {
        int32 Best = 0;
        for (int32 I = 1; I < UE_ARRAY_COUNT(Vertex.InfluenceWeights); ++I)
          if (Vertex.InfluenceWeights[I] > Vertex.InfluenceWeights[Best])
            Best = I;
        const int32 Local = Vertex.InfluenceBones[Best];
        if (Section.BoneMap.IsValidIndex(Local) &&
            Dominant.IsValidIndex(Section.BoneMap[Local]))
          ++Dominant[Section.BoneMap[Local]];
      }
      Sections.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }
  Root->SetArrayField(TEXT("original_lod0_sections"), Sections);
  TArray<TSharedPtr<FJsonValue>> Weights;
  for (int32 I = 0; I < Dominant.Num(); ++I) {
    auto Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("bone"), Skeleton.GetBoneName(I).ToString());
    Entry->SetNumberField(TEXT("dominant_vertices"), double(Dominant[I]));
    Weights.Add(MakeShared<FJsonValueObject>(Entry));
  }
  Root->SetArrayField(TEXT("original_dominant_skinning"), Weights);
  FString Result;
  auto Writer = TJsonWriterFactory<>::Create(&Result);
  FJsonSerializer::Serialize(Root, Writer);
  return Result;
#else
  return TEXT("ERROR: editor operation");
#endif
}

FString UShipMovementComponent::BuildSmallEquipment() {
#if WITH_EDITOR
  auto *Original = LoadObject<USkeletalMesh>(
      nullptr,
      TEXT("/Game/Ship/Assets/VigilanteContent/Vehicles/West_Ship_DDG102/"
           "Sk_West_Ship_DDG102.Sk_West_Ship_DDG102"));
  auto *Boat = LoadObject<UStaticMesh>(
      nullptr,
      TEXT("/Game/ShipSmall/Asset/sweedenmilitaryboat.sweedenmilitaryboat"));
  if (!Original || !Boat || !Original->GetImportedModel() ||
      Original->GetImportedModel()->LODModels.IsEmpty())
    return TEXT("ERROR: source meshes missing");
  auto Save = [](UObject *Asset) {
    Asset->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(Asset->GetOutermost(), Asset,
                                 *FPackageName::LongPackageNameToFilename(
                                     Asset->GetOutermost()->GetName(),
                                     FPackageName::GetAssetPackageExtension()),
                                 Args);
  };
  const auto &Ref = Original->GetRefSkeleton();
  const auto &LOD = Original->GetImportedModel()->LODModels[0];
  UStaticMesh *Guns[2] = {};
  FString Result = TEXT("OK: Small equipment\n");
  for (int32 Part = 0; Part < 2; ++Part) {
    const FName Bone = Part ? TEXT("front_gun_v_jnt") : TEXT("front_gun_h_jnt");
    const int32 BoneIndex = Ref.FindBoneIndex(Bone);
    if (BoneIndex == INDEX_NONE)
      return TEXT("ERROR: actual gun bone missing");
    FTransform Pivot = FTransform::Identity;
    for (int32 I = BoneIndex; I != INDEX_NONE; I = Ref.GetParentIndex(I))
      Pivot = Pivot * Ref.GetRefBonePose()[I];
    const FString Name =
                      Part ? TEXT("SM_SmallGunPitch") : TEXT("SM_SmallGunYaw"),
                  Package = TEXT("/Game/NavalBattle/Equipment/") + Name;
    auto *Mesh =
        LoadObject<UStaticMesh>(nullptr, *(Package + TEXT(".") + Name));
    const bool NewMesh = Mesh == nullptr;
    if (!Mesh) {
      Mesh = NewObject<UStaticMesh>(CreatePackage(*Package), FName(*Name),
                                    RF_Public | RF_Standalone);
      FAssetRegistryModule::AssetCreated(Mesh);
    }
    FMeshDescription Description;
    FStaticMeshAttributes Attributes(Description);
    Attributes.Register();
    Attributes.GetVertexInstanceUVs().SetNumChannels(1);
    if (NewMesh)
      Mesh->GetStaticMaterials().Reset();
    int32 Triangles = 0;
    for (const auto &Section : LOD.Sections) {
      const auto Group = Description.CreatePolygonGroup();
      const FName Slot(
          *FString::Printf(TEXT("SourceMaterial%d"), Section.MaterialIndex));
      Attributes.GetPolygonGroupMaterialSlotNames()[Group] = Slot;
      if (NewMesh)
        Mesh->GetStaticMaterials().Add(FStaticMaterial(
            Original->GetMaterials()[Section.MaterialIndex].MaterialInterface,
            Slot));
      TArray<int32> Dominant;
      Dominant.Reserve(Section.SoftVertices.Num());
      for (const auto &V : Section.SoftVertices) {
        int32 Best = 0;
        for (int32 I = 1; I < UE_ARRAY_COUNT(V.InfluenceWeights); ++I)
          if (V.InfluenceWeights[I] > V.InfluenceWeights[Best])
            Best = I;
        Dominant.Add(Section.BoneMap[V.InfluenceBones[Best]]);
      }
      for (uint32 T = 0; T < Section.NumTriangles; ++T) {
        int32 Indices[3];
        bool Include = true;
        for (int32 C = 0; C < 3; ++C) {
          Indices[C] = int32(LOD.IndexBuffer[Section.BaseIndex + T * 3 + C]) -
                       Section.BaseVertexIndex;
          Include &= Dominant.IsValidIndex(Indices[C]) &&
                     Dominant[Indices[C]] == BoneIndex;
        }
        if (!Include)
          continue;
        TArray<FVertexInstanceID> Corners;
        for (int32 C = 0; C < 3; ++C) {
          const auto &V = Section.SoftVertices[Indices[C]];
          const auto Vertex = Description.CreateVertex();
          Attributes.GetVertexPositions()[Vertex] =
              FVector3f(Pivot.InverseTransformPosition(FVector(V.Position)));
          const auto Instance = Description.CreateVertexInstance(Vertex);
          Corners.Add(Instance);
          Attributes.GetVertexInstanceNormals()[Instance] = FVector3f(
              Pivot.InverseTransformVectorNoScale(FVector(V.TangentZ)));
          Attributes.GetVertexInstanceTangents()[Instance] = FVector3f(
              Pivot.InverseTransformVectorNoScale(FVector(V.TangentX)));
          Attributes.GetVertexInstanceBinormalSigns()[Instance] =
              GetBasisDeterminantSign(FVector(V.TangentX), FVector(V.TangentY),
                                      FVector(V.TangentZ));
          Attributes.GetVertexInstanceUVs().Set(Instance, 0, V.UVs[0]);
          Attributes.GetVertexInstanceColors()[Instance] =
              FVector4f(FLinearColor(V.Color));
        }
        Description.CreateTriangle(Group, Corners);
        ++Triangles;
      }
    }
    if (Triangles < 10)
      return TEXT("ERROR: actual gun skin extraction empty");
    // BuildFromMeshDescriptions creates render resources. Existing saved
    // equipment must not have its live resources replaced during a repeat
    // setup pass; geometry is unchanged and only hull sockets need updating.
    if (NewMesh) {
      TArray<const FMeshDescription *> Meshes{&Description};
      Mesh->BuildFromMeshDescriptions(Meshes);
      if (!Save(Mesh))
        return TEXT("ERROR: gun mesh save failed");
    }
    Guns[Part] = Mesh;
    Result += FString::Printf(TEXT("%s: %d triangles, pivot %s\n"), *Name,
                              Triangles, *Pivot.GetLocation().ToString());
  }
  const FString HullName = TEXT("SM_SmallPhysicsHull"),
                HullPackage = TEXT("/Game/NavalBattle/Equipment/") + HullName;
  auto *Hull =
      LoadObject<UStaticMesh>(nullptr, *(HullPackage + TEXT(".") + HullName));
  const bool NewHull = Hull == nullptr;
  if (!Hull) {
    Hull = NewObject<UStaticMesh>(CreatePackage(*HullPackage), FName(*HullName),
                                  RF_Public | RF_Standalone);
    FAssetRegistryModule::AssetCreated(Hull);
  }
  // A low-detail watertight wedge follows the measured 67 x 18.7 m boat.
  TArray<FVector> Points;
  for (double Z : {-400., 400.})
    for (const FVector2D &P :
         {FVector2D(-3300, -850), FVector2D(-3300, 850), FVector2D(1800, 850),
          FVector2D(3300, 0), FVector2D(1800, -850)})
      Points.Add(FVector(P.X, P.Y, Z));
  FMeshDescription HullDescription;
  FStaticMeshAttributes HullAttributes(HullDescription);
  HullAttributes.Register();
  const auto Group = HullDescription.CreatePolygonGroup();
  auto Triangle = [&](int32 A, int32 B, int32 C) {
    TArray<FVertexInstanceID> Corners;
    for (int32 I : {A, B, C}) {
      const auto V = HullDescription.CreateVertex();
      HullAttributes.GetVertexPositions()[V] = FVector3f(Points[I]);
      Corners.Add(HullDescription.CreateVertexInstance(V));
    }
    HullDescription.CreateTriangle(Group, Corners);
  };
  for (int32 I = 1; I < 4; ++I) {
    Triangle(0, I + 1, I);
    Triangle(5, 5 + I, 6 + I);
  }
  for (int32 I = 0; I < 5; ++I) {
    const int32 J = (I + 1) % 5;
    Triangle(I, J, J + 5);
    Triangle(I, J + 5, I + 5);
  }
  if (NewHull)
    Hull->BuildFromMeshDescriptions(
        TArray<const FMeshDescription *>{&HullDescription});
  Hull->CreateBodySetup();
  auto *OriginalHull = LoadObject<UStaticMesh>(
      nullptr, TEXT("/Game/Ship/Assets/SM/untitled.untitled"));
  if (!OriginalHull || !OriginalHull->FindSocket(TEXT("5")))
    return TEXT("ERROR: original rudder socket contract missing");
  Hull->Sockets.Reset();
  for (const UStaticMeshSocket *SourceSocket : OriginalHull->Sockets)
    if (SourceSocket) {
      auto *Socket = NewObject<UStaticMeshSocket>(Hull);
      Socket->SocketName = SourceSocket->SocketName;
      Socket->RelativeLocation = SourceSocket->RelativeLocation * .425;
      Socket->RelativeRotation = SourceSocket->RelativeRotation;
      Socket->RelativeScale = FVector(1);
      Hull->Sockets.Add(Socket);
    }
  auto *Body = Hull->GetBodySetup();
  Body->AggGeom.EmptyElements();
  FKConvexElem Convex;
  Convex.VertexData = Points;
  Convex.UpdateElemBox();
  Body->AggGeom.ConvexElems.Add(Convex);
  Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
  Body->InvalidatePhysicsData();
  Body->CreatePhysicsMeshes();
  if (!Save(Hull))
    return TEXT("ERROR: hull save failed");
  auto Profile = [&](const TCHAR *Name) {
    const FString Package = FString(TEXT("/Game/NavalBattle/Profiles/")) + Name;
    auto *P =
        LoadObject<UShipPhysicsProfile>(nullptr, *(Package + TEXT(".") + Name));
    if (!P) {
      P = NewObject<UShipPhysicsProfile>(CreatePackage(*Package), FName(Name),
                                         RF_Public | RF_Standalone);
      FAssetRegistryModule::AssetCreated(P);
    }
    return P;
  };
  auto *Medium = Profile(TEXT("DA_MediumShip"));
  Medium->SetDefaults(EShipHullClass::Medium);
  if (!Save(Medium))
    return TEXT("ERROR: Medium profile save failed");
  auto *Small = Profile(TEXT("DA_SmallShip"));
  Small->SetDefaults(EShipHullClass::Small);
  Small->HullCollisionMesh = Hull;
  Small->StaticVisualMesh = Boat;
  Small->GunYawMesh = Guns[0];
  Small->GunPitchMesh = Guns[1];
  if (!Save(Small))
    return TEXT("ERROR: Small profile save failed");
  return Result + TEXT("Saved Small/Medium profiles and compact physical hull; "
                       "Large profile untouched");
#else
  return TEXT("ERROR: editor operation");
#endif
}

FString UShipMovementComponent::SetupSmallBlueprints() {
#if WITH_EDITOR
  auto *Base = LoadObject<UBlueprint>(
      nullptr, TEXT("/Game/Ship/BluePrint/BP_Ship.BP_Ship"));
  auto *Enemy = LoadObject<UBlueprint>(
      nullptr, TEXT("/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy"));
  auto *P = LoadObject<UShipPhysicsProfile>(
      nullptr, TEXT("/Game/NavalBattle/Profiles/DA_SmallShip.DA_SmallShip"));
  if (!Base || !Enemy || !P || !P->StaticVisualMesh || !P->GunYawMesh ||
      !P->GunPitchMesh)
    return TEXT("ERROR: source Blueprint/equipment missing");
  auto *RootNode = Base->SimpleConstructionScript->FindSCSNode(TEXT("ShipSM"));
  if (!RootNode)
    return TEXT("ERROR: original physical root node missing");
  auto *Medium = LoadObject<UShipPhysicsProfile>(
      nullptr, TEXT("/Game/NavalBattle/Profiles/DA_MediumShip.DA_MediumShip"));
  if (!Medium)
    return TEXT("ERROR: Medium profile missing");
  bool BaseChanged = false;
  for (auto *Node : Base->SimpleConstructionScript->GetAllNodes())
    if (auto *Movement = Cast<UShipMovementComponent>(Node->ComponentTemplate))
      if (Movement->HullClass != EShipHullClass::Medium ||
          Movement->Profile != Medium) {
        Movement->Modify();
        Movement->HullClass = EShipHullClass::Medium;
        Movement->Profile = Medium;
        BaseChanged = true;
      }
  if (BaseChanged) {
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Base);
    FCompilerResultsLog BaseLog;
    FKismetEditorUtilities::CompileBlueprint(
        Base, EBlueprintCompileOptions::None, &BaseLog);
    if (BaseLog.NumErrors || Base->Status == BS_Error)
      return TEXT("ERROR: original Medium default compile failed");
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Base->GetOutermost(), Base,
                               *FPackageName::LongPackageNameToFilename(
                                   Base->GetOutermost()->GetName(),
                                   FPackageName::GetAssetPackageExtension()),
                               Args))
      return TEXT("ERROR: original Medium default save failed");
  }
  FString Result = TEXT("OK: dedicated Small Blueprints\n");
  for (int32 AI = 0; AI < 2; ++AI) {
    const FString Name = AI ? TEXT("BP_ShipSmallAI") : TEXT("BP_ShipSmall");
    const FString Package = TEXT("/Game/ShipSmall/BluePrint/") + Name;
    UClass *Parent = AI ? Enemy->GeneratedClass : Base->GeneratedClass;
    auto *BP = LoadObject<UBlueprint>(nullptr, *(Package + TEXT(".") + Name));
    if (!BP) {
      BP = FKismetEditorUtilities::CreateBlueprint(
          Parent, CreatePackage(*Package), FName(*Name), BPTYPE_Normal,
          UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
      if (!BP)
        return TEXT("ERROR: child creation failed");
      FAssetRegistryModule::AssetCreated(BP);
    }
    // Preserve any existing graphs/components in the user-created asset.
    if (BP->ParentClass != Parent)
      UBlueprintEditorLibrary::ReparentBlueprint(BP, Parent);
    BP->Modify();
    auto *Handler = BP->GetInheritableComponentHandler(true);
    for (auto *Node : Base->SimpleConstructionScript->GetAllNodes()) {
      const FComponentKey Key(Node);
      auto *Template = Handler->GetOverridenComponentTemplate(Key);
      if (!Template)
        Template = Handler->CreateOverridenComponentTemplate(Key);
      if (!Template)
        continue;
      if (Node->GetVariableName() == TEXT("ShipSM"))
        if (auto *Hull = Cast<UStaticMeshComponent>(Template)) {
          Hull->SetStaticMesh(P->HullCollisionMesh);
          Hull->SetHiddenInGame(true);
          Hull->SetVisibility(false);
        }
      if (Node->GetVariableName() == TEXT("Ship"))
        if (auto *Driver = Cast<USkeletalMeshComponent>(Template)) {
          Driver->SetRelativeTransform(P->WeaponDriverTransform);
          Driver->SetVisibility(false, false);
          Driver->SetHiddenInGame(true, false);
          Driver->SetCollisionEnabled(ECollisionEnabled::NoCollision);
          Driver->ComponentTags.AddUnique(TEXT("NavalSmallDriver"));
          Driver->VisibilityBasedAnimTickOption =
              EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        }
      if (auto *Movement = Cast<UShipMovementComponent>(Template)) {
        Movement->HullClass = EShipHullClass::Small;
        Movement->Profile = P;
      }
    }
    const FName Names[] = {TEXT("SmallBoatVisual"), TEXT("SmallGunYaw"),
                           TEXT("SmallGunPitch")};
    UStaticMesh *Meshes[] = {P->StaticVisualMesh, P->GunYawMesh,
                             P->GunPitchMesh};
    for (int32 I = 0; I < 3; ++I) {
      auto *Node = BP->SimpleConstructionScript->FindSCSNode(Names[I]);
      if (!Node) {
        Node = BP->SimpleConstructionScript->CreateNode(
            UStaticMeshComponent::StaticClass(), Names[I]);
        BP->SimpleConstructionScript->AddNode(Node);
        Node->SetParent(RootNode);
      }
      auto *Part = Cast<UStaticMeshComponent>(Node->ComponentTemplate);
      if (!Part)
        return TEXT("ERROR: existing Small component type conflict");
      Part->SetStaticMesh(Meshes[I]);
      Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
      Part->SetVisibility(true);
      Part->SetHiddenInGame(false);
      Part->SetCanEverAffectNavigation(false);
      if (I == 0)
        Part->SetRelativeTransform(P->StaticVisualTransform);
      else {
        // Gun pivots were verified from the source skeleton.
        const FVector Pivot = I == 1 ? FVector(4907.657, -5.639, 773.625)
                                     : FVector(4893.446, -5.673, 997.056);
        Part->SetRelativeTransform(FTransform(
            FQuat::Identity, P->WeaponDriverTransform.TransformPosition(Pivot),
            P->WeaponDriverTransform.GetScale3D()));
      }
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None,
                                             &Log);
    if (Log.NumErrors || BP->Status == BS_Error)
      return TEXT("ERROR: Small child compile failed; not saved");
    BP->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(
            BP->GetOutermost(), BP,
            *FPackageName::LongPackageNameToFilename(
                Package, FPackageName::GetAssetPackageExtension()),
            Args))
      return TEXT("ERROR: Small child save failed");
    Result += Name + TEXT(": inherited original gameplay, authored visible "
                          "equipment, compiled/saved\n");
  }
  return Result;
#else
  return TEXT("ERROR: editor operation");
#endif
}
