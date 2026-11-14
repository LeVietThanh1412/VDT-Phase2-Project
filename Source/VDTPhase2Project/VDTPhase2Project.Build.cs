// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VDTPhase2Project : ModuleRules
{
	public VDTPhase2Project(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "CesiumRuntime", "AIModule", "UMG", "Water" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "JsonUtilities", "Slate", "SlateCore", "Niagara", "ProceduralMeshComponent", "Chaos", "PhysicsCore", "HTTP" });
		if (Target.bBuildEditor)
		{
            PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "BlueprintGraph", "KismetCompiler", "UMGEditor", "AssetRegistry", "MeshDescription", "StaticMeshDescription", "BlueprintEditorLibrary" });
		}
		// Compact offline regional map is staged into packaged games; no gameplay network service.
		RuntimeDependencies.Add("$(ProjectDir)/Content/SeaNavigation/halong_navigation.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Content/SeaNavigation/quangninh_sectors.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Content/SeaNavigation/quangninh_demo_routes.json", StagedFileType.UFS);
		// The menu's composite font reads the TTF directly from the content directory.
		RuntimeDependencies.Add("$(ProjectDir)/Content/Fonts/BlackOpsOne-Regular.ttf", StagedFileType.NonUFS);
		RuntimeDependencies.Add("$(ProjectDir)/Content/Fonts/OFL.txt", StagedFileType.NonUFS);

		// Cesium opens terrain and TMS tiles through file:// URIs, outside pak files.
		string TerrainRoot = System.IO.Path.Combine(ModuleDirectory, "../../Data/UnrealTerrainSet/QuangNinh");
		if (!System.IO.File.Exists(System.IO.Path.Combine(TerrainRoot, "Terrain/layer.json")) ||
			!System.IO.File.Exists(System.IO.Path.Combine(TerrainRoot, "Imagery/tilemapresource.xml")))
			throw new BuildException("Restore Data/UnrealTerrainSet/QuangNinh (terrain and imagery) before building.");
		foreach (string TerrainFile in System.IO.Directory.EnumerateFiles(TerrainRoot, "*", System.IO.SearchOption.AllDirectories))
		{
			string Extension = System.IO.Path.GetExtension(TerrainFile).ToLowerInvariant();
			if (Extension == ".terrain" || Extension == ".png" || Extension == ".json" || Extension == ".xml")
				RuntimeDependencies.Add(System.IO.Path.GetFullPath(TerrainFile), StagedFileType.NonUFS);
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
