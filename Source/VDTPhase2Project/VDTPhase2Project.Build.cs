// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VDTPhase2Project : ModuleRules
{
	public VDTPhase2Project(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "CesiumRuntime", "AIModule", "UMG", "Water" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "JsonUtilities", "Slate", "SlateCore", "Niagara", "ProceduralMeshComponent", "Chaos", "PhysicsCore" });
		if (Target.bBuildEditor)
		{
            PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "BlueprintGraph", "KismetCompiler", "UMGEditor", "AssetRegistry", "MeshDescription", "StaticMeshDescription", "BlueprintEditorLibrary" });
		}
		// Compact offline regional map is staged into packaged games; no gameplay network service.
		RuntimeDependencies.Add("$(ProjectDir)/Content/SeaNavigation/halong_navigation.json", StagedFileType.UFS);

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
