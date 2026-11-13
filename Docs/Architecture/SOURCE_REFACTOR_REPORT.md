# Source organization refactor report

> Follow-up: this document records the structure-only checkpoint. The user subsequently requested the separate Windows build repair. See [WINDOWS_BUILD_FIX.md](WINDOWS_BUILD_FIX.md) for the Cesium macro fix, commandlet plugin fix and newer build/packaging results. Original failed receipts below are retained as historical evidence.

Date:2026-11-11. Branch: `refactor/code`. Starting HEAD: `76025f7228b280f1009de309efa2c46351aa2596`.

**Structure and Editor builds verified, with pre-existing game-build and native-test limitations documented below.** All60 C++ files are moved. Gameplay methods, APIs, numerical constants, tags and assets are preserved. No commit, push or merge into main was performed. Manual PIE testing remains the user's responsibility as requested.

The user's latest instruction supersedes creating another branch and rerunning formatting. Their60 uncommitted formatting edits were preserved, with exact original files and a binary-capable patch in `Saved/SourceRefactor/20261111/`. The refactor is left uncommitted to avoid silently committing those pre-existing changes.

## 1. Architecture and complete inventory

The existing single Runtime module remains `VDTPhase2Project`. Root Build.cs and both Target.cs files are unchanged.

- `Public/Ships`: movement and physics-profile interfaces.
- `Public/Navigation`: navigator, immutable sea chart and terrain collision interfaces.
- `Public/Combat`: identity, weapons, combat, projectile and wreck interfaces.
- `Public/Gameplay`: player controller, game mode, settings and free-camera interfaces.
- `Public/Subsystems`: battlefield and geographic sector subsystems.
- `Public/UI`: setup, HUD, details, minimap, kill feed, loading and result interfaces.
- `Private/Ships/Movement`, `Private/Ships/Physics`, `Private/Ships/Editor`: implementations and existing guarded editor helpers.
- `Private/Navigation` and its `Editor` subfolder: routing/collision implementations and existing editor integration.
- `Private/Combat`, `Projectiles`, `Editor`: weapon, damage, projectile, wreck and guarded editor implementations.
- `Private/AI/Tactics`: existing tactical implementation of the battlefield subsystem.
- `Private/Gameplay` and `Pawns`: controller/settings/game mode and free camera implementations.
- `Private/Subsystems`, `Private/UI`: remaining subsystem and presentation implementations.
- `Private/Utilities`: internal water-query helpers.
- `Private/Tests`: existing navigation tests and transient native test probe.
- `Private/VDTPhase2Project.cpp/.h`: unchanged primary-module identity and internal umbrella header.

There are23 public headers,3 private headers and34 private implementations. No extra modules or empty organizational directories were introduced. A physical Editor folder does not create an editor module: existing WITH_EDITOR guards and conditional Build.cs dependencies remain intact. Existing exported interfaces remain public; internal water helpers, test probe and module umbrella remain private.

`NavalBattleUI.h` retains its joint presentation/match declarations, and `ShipCombatComponent.h` retains its weapon/combat/library declarations. Splitting those reflected identities is outside this structure-only scope. No repository plugin includes a moved game header. No asset was moved to match these C++ directories.

Full source/include inventory, plugin source inventory, targets and risks: [SOURCE_REFACTOR_INVENTORY.json](SOURCE_REFACTOR_INVENTORY.json). Per-file consumers, associated headers/implementations, identities and digests: [SOURCE_MIGRATION_MANIFEST.json](SOURCE_MIGRATION_MANIFEST.json).

## 2. Full migration table

This mapping was produced before moving files. Every tracked C++ file was moved with git mv, preserving filenames. The combined work was subsequently unstaged for review.

| Original | New | Responsibility |
|---|---|---|
| `Source/VDTPhase2Project/Actor/Pawn/FreeCameraPawn.cpp` | `Source/VDTPhase2Project/Private/Gameplay/Pawns/FreeCameraPawn.cpp` | gameplay |
| `Source/VDTPhase2Project/Actor/Pawn/FreeCameraPawn.h` | `Source/VDTPhase2Project/Public/Gameplay/FreeCameraPawn.h` | gameplay |
| `Source/VDTPhase2Project/CustomPlayerController.cpp` | `Source/VDTPhase2Project/Private/Gameplay/CustomPlayerController.cpp` | gameplay |
| `Source/VDTPhase2Project/CustomPlayerController.h` | `Source/VDTPhase2Project/Public/Gameplay/CustomPlayerController.h` | gameplay |
| `Source/VDTPhase2Project/NavalBattleUI.cpp` | `Source/VDTPhase2Project/Private/UI/NavalBattleUI.cpp` | ui |
| `Source/VDTPhase2Project/NavalBattleUI.h` | `Source/VDTPhase2Project/Public/UI/NavalBattleUI.h` | ui |
| `Source/VDTPhase2Project/NavalBattlefieldSubsystem.cpp` | `Source/VDTPhase2Project/Private/Subsystems/NavalBattlefieldSubsystem.cpp` | gameplay |
| `Source/VDTPhase2Project/NavalBattlefieldSubsystem.h` | `Source/VDTPhase2Project/Public/Subsystems/NavalBattlefieldSubsystem.h` | gameplay |
| `Source/VDTPhase2Project/NavalBattlefieldTactics.cpp` | `Source/VDTPhase2Project/Private/AI/Tactics/NavalBattlefieldTactics.cpp` | ai |
| `Source/VDTPhase2Project/NavalGameMode.cpp` | `Source/VDTPhase2Project/Private/Gameplay/NavalGameMode.cpp` | gameplay |
| `Source/VDTPhase2Project/NavalGameMode.h` | `Source/VDTPhase2Project/Public/Gameplay/NavalGameMode.h` | gameplay |
| `Source/VDTPhase2Project/NavalMatchSettings.cpp` | `Source/VDTPhase2Project/Private/Gameplay/NavalMatchSettings.cpp` | gameplay |
| `Source/VDTPhase2Project/NavalMatchSettings.h` | `Source/VDTPhase2Project/Public/Gameplay/NavalMatchSettings.h` | gameplay |
| `Source/VDTPhase2Project/NavalSectorSubsystem.cpp` | `Source/VDTPhase2Project/Private/Subsystems/NavalSectorSubsystem.cpp` | gameplay |
| `Source/VDTPhase2Project/NavalSectorSubsystem.h` | `Source/VDTPhase2Project/Public/Subsystems/NavalSectorSubsystem.h` | gameplay |
| `Source/VDTPhase2Project/Private/NavalWaterQueries.cpp` | `Source/VDTPhase2Project/Private/Utilities/NavalWaterQueries.cpp` | utilities |
| `Source/VDTPhase2Project/Private/NavalWaterQueries.h` | `Source/VDTPhase2Project/Private/Utilities/NavalWaterQueries.h` | utilities |
| `Source/VDTPhase2Project/SeaNavigationMap.cpp` | `Source/VDTPhase2Project/Private/Navigation/SeaNavigationMap.cpp` | navigation |
| `Source/VDTPhase2Project/SeaNavigationMap.h` | `Source/VDTPhase2Project/Public/Navigation/SeaNavigationMap.h` | navigation |
| `Source/VDTPhase2Project/SeaTerrainCollisionComponent.cpp` | `Source/VDTPhase2Project/Private/Navigation/SeaTerrainCollisionComponent.cpp` | navigation |
| `Source/VDTPhase2Project/SeaTerrainCollisionComponent.h` | `Source/VDTPhase2Project/Public/Navigation/SeaTerrainCollisionComponent.h` | navigation |
| `Source/VDTPhase2Project/ShipArchetypeEditor.cpp` | `Source/VDTPhase2Project/Private/Ships/Editor/ShipArchetypeEditor.cpp` | ships |
| `Source/VDTPhase2Project/ShipCombatComponent.cpp` | `Source/VDTPhase2Project/Private/Combat/ShipCombatComponent.cpp` | combat |
| `Source/VDTPhase2Project/ShipCombatComponent.h` | `Source/VDTPhase2Project/Public/Combat/ShipCombatComponent.h` | combat |
| `Source/VDTPhase2Project/ShipCombatEditor.cpp` | `Source/VDTPhase2Project/Private/Combat/Editor/ShipCombatEditor.cpp` | combat |
| `Source/VDTPhase2Project/ShipDebugEditor.cpp` | `Source/VDTPhase2Project/Private/Combat/Editor/ShipDebugEditor.cpp` | combat |
| `Source/VDTPhase2Project/ShipDestructionEditor.cpp` | `Source/VDTPhase2Project/Private/Combat/Editor/ShipDestructionEditor.cpp` | combat |
| `Source/VDTPhase2Project/ShipEnemyNavigationComponent.cpp` | `Source/VDTPhase2Project/Private/Navigation/ShipEnemyNavigationComponent.cpp` | navigation |
| `Source/VDTPhase2Project/ShipEnemyNavigationComponent.h` | `Source/VDTPhase2Project/Public/Navigation/ShipEnemyNavigationComponent.h` | navigation |
| `Source/VDTPhase2Project/ShipEnemyNavigationEditor.cpp` | `Source/VDTPhase2Project/Private/Navigation/Editor/ShipEnemyNavigationEditor.cpp` | navigation |
| `Source/VDTPhase2Project/ShipEnemyNavigationTests.cpp` | `Source/VDTPhase2Project/Private/Tests/ShipEnemyNavigationTests.cpp` | tests |
| `Source/VDTPhase2Project/ShipEnemyProjectile.cpp` | `Source/VDTPhase2Project/Private/Combat/Projectiles/ShipEnemyProjectile.cpp` | combat |
| `Source/VDTPhase2Project/ShipEnemyProjectile.h` | `Source/VDTPhase2Project/Public/Combat/ShipEnemyProjectile.h` | combat |
| `Source/VDTPhase2Project/ShipMovementComponent.cpp` | `Source/VDTPhase2Project/Private/Ships/Movement/ShipMovementComponent.cpp` | ships |
| `Source/VDTPhase2Project/ShipMovementComponent.h` | `Source/VDTPhase2Project/Public/Ships/ShipMovementComponent.h` | ships |
| `Source/VDTPhase2Project/ShipMovementEditor.cpp` | `Source/VDTPhase2Project/Private/Ships/Editor/ShipMovementEditor.cpp` | ships |
| `Source/VDTPhase2Project/ShipPhysicsProfile.cpp` | `Source/VDTPhase2Project/Private/Ships/Physics/ShipPhysicsProfile.cpp` | ships |
| `Source/VDTPhase2Project/ShipPhysicsProfile.h` | `Source/VDTPhase2Project/Public/Ships/ShipPhysicsProfile.h` | ships |
| `Source/VDTPhase2Project/ShipProjectileDamageComponent.cpp` | `Source/VDTPhase2Project/Private/Combat/ShipProjectileDamageComponent.cpp` | combat |
| `Source/VDTPhase2Project/ShipProjectileDamageComponent.h` | `Source/VDTPhase2Project/Public/Combat/ShipProjectileDamageComponent.h` | combat |
| `Source/VDTPhase2Project/ShipSetupWidget.cpp` | `Source/VDTPhase2Project/Private/UI/ShipSetupWidget.cpp` | ui |
| `Source/VDTPhase2Project/ShipSetupWidget.h` | `Source/VDTPhase2Project/Public/UI/ShipSetupWidget.h` | ui |
| `Source/VDTPhase2Project/ShipWreckBillboardComponent.cpp` | `Source/VDTPhase2Project/Private/Combat/ShipWreckBillboardComponent.cpp` | combat |
| `Source/VDTPhase2Project/ShipWreckBillboardComponent.h` | `Source/VDTPhase2Project/Public/Combat/ShipWreckBillboardComponent.h` | combat |
| `Source/VDTPhase2Project/Tests/ShipWeaponFailureProbe.cpp` | `Source/VDTPhase2Project/Private/Tests/ShipWeaponFailureProbe.cpp` | tests |
| `Source/VDTPhase2Project/Tests/ShipWeaponFailureProbe.h` | `Source/VDTPhase2Project/Private/Tests/ShipWeaponFailureProbe.h` | tests |
| `Source/VDTPhase2Project/UI/ButtonTutorialsUI.cpp` | `Source/VDTPhase2Project/Private/UI/ButtonTutorialsUI.cpp` | ui |
| `Source/VDTPhase2Project/UI/ButtonTutorialsUI.h` | `Source/VDTPhase2Project/Public/UI/ButtonTutorialsUI.h` | ui |
| `Source/VDTPhase2Project/UI/KillFeedWidget.cpp` | `Source/VDTPhase2Project/Private/UI/KillFeedWidget.cpp` | ui |
| `Source/VDTPhase2Project/UI/KillFeedWidget.h` | `Source/VDTPhase2Project/Public/UI/KillFeedWidget.h` | ui |
| `Source/VDTPhase2Project/UI/LoadingScreenWidget.cpp` | `Source/VDTPhase2Project/Private/UI/LoadingScreenWidget.cpp` | ui |
| `Source/VDTPhase2Project/UI/LoadingScreenWidget.h` | `Source/VDTPhase2Project/Public/UI/LoadingScreenWidget.h` | ui |
| `Source/VDTPhase2Project/UI/MatchSummaryWidget.cpp` | `Source/VDTPhase2Project/Private/UI/MatchSummaryWidget.cpp` | ui |
| `Source/VDTPhase2Project/UI/MatchSummaryWidget.h` | `Source/VDTPhase2Project/Public/UI/MatchSummaryWidget.h` | ui |
| `Source/VDTPhase2Project/UI/ShipDetailsUI.cpp` | `Source/VDTPhase2Project/Private/UI/ShipDetailsUI.cpp` | ui |
| `Source/VDTPhase2Project/UI/ShipDetailsUI.h` | `Source/VDTPhase2Project/Public/UI/ShipDetailsUI.h` | ui |
| `Source/VDTPhase2Project/UI/TeamCountBarWidget.cpp` | `Source/VDTPhase2Project/Private/UI/TeamCountBarWidget.cpp` | ui |
| `Source/VDTPhase2Project/UI/TeamCountBarWidget.h` | `Source/VDTPhase2Project/Public/UI/TeamCountBarWidget.h` | ui |
| `Source/VDTPhase2Project/VDTPhase2Project.cpp` | `Source/VDTPhase2Project/Private/VDTPhase2Project.cpp` | module |
| `Source/VDTPhase2Project/VDTPhase2Project.h` | `Source/VDTPhase2Project/Private/VDTPhase2Project.h` | module |

The manifest additionally contains included-by consumers, paired files, export/reflection classification and migration risks for every row. The pre-migration plan remains in [SOURCE_REFACTOR_PLAN.md](SOURCE_REFACTOR_PLAN.md).

## 3. Dependencies and compatibility

Project-local includes now use module-relative Public/Private paths, such as `Ships/ShipMovementComponent.h`, `Navigation/SeaNavigationMap.h`, `Combat/ShipCombatComponent.h` and `Utilities/NavalWaterQueries.h`. No broad include directories were added. The original dependency sequence is preserved, except for mandatory generated-header placement. No engine include was removed or reordered, and no new local include graph edges/cycles were introduced. Public headers do not include private game headers.

All generated filenames, reflected class/struct/enum names, functions, properties and `/Script/VDTPhase2Project` identities are unchanged. All41 expected native reflected objects load, and39 existing Blueprints compile without saves. No Core Redirects are needed. All known repository consumers are migrated; out-of-repository C++ consumers cannot be inspected and must use the new qualified include paths.

Two active source-path consumers were updated: `Tools/NavalCombat/summarize_seeded_evidence.py` resolves both recorded historical commits and the current moved source tree, and `Tools/SeaNavigation/write_two_target_report.py` emits the new current paths. Both11-file provenance lookup modes were exercised. Historical documentation/evidence retains its historical paths.

UBT regenerated VSCode project files successfully. Generated compile databases/response files now use the new paths. Existing workspace and user Python settings stayed unchanged, with pre-generation copies retained under Saved. Build.cs dependencies, runtime-staged JSON files, Target.cs rules, uproject and Config remain unchanged. Default map and GameMode still reference `/Game/Levels/MainMapLevel` and `/Game/Ship/BluePrint/GMBP.GMBP_C`.

## 4. Formatting and UHT syntax repairs

No formatter was invoked; formatter version was not established. The user's existing60-file formatting is preserved. No new .clang-format was imposed and no C# file was formatted.

The actual initial Editor baseline failed before moving files: formatting placed includes after .generated.h in9 headers and split4 UPROPERTY tooltip strings into adjacent literals that UHT cannot parse. Those structural syntax defects were repaired by moving each unchanged generated include last and rejoining each tooltip with the same string value. Names, categories, defaults, constants and gameplay logic are unchanged. The repaired pre-move Editor baseline then built successfully.

The original-to-repaired proof checks all60 files against the saved user worktree, allowing only the recorded include and metadata-literal repairs. Every final non-include body is byte-exact against the independently saved repaired baseline. The portable checker also accepts Git's normal CRLF/LF conversion. It independently checks the include sequence, source count, filename identities, Public/Private visibility and all23 generated-header constraints.

For refactor-only review, use [this focused patch](../../Saved/SourceRefactor/20261111/REFACTOR_ONLY_VS_USER_FORMATTING.patch). It compares against the user's formatted source and detects all60 renames. Ordinary Git diff against HEAD also includes the pre-existing formatting.

## 5. Build and verification evidence

UE5.6.1; UBT selected MSVC14.51.36252 and Windows SDK10.0.26100.0. The non-preferred compiler warning is retained; toolchain/engine source was not changed to mask it.

| Check | Actual result |
|---|---|
| Initial formatted Editor baseline | Failed UHT before moves |
| Repaired pre-move Editor | Passed |
| First utilities attempt | Temporary local-header lookup errors; fixed before continuing |
| Corrected utilities, ships, navigation, AI, combat, gameplay, UI, tests, module | All9 incremental Editor builds passed |
| Project regeneration | Passed |
| Final Editor Development, forced Unity/non-adaptive | Passed |
| Final Editor Development, normal settings | Passed |
| Shipping before moves |72 Cesium compiler errors |
| Final Game Development | Same72 baseline error signatures |
| Final Game Shipping | Same72 baseline error signatures |
| Native identity/Blueprint commandlet |41 native types,39 Blueprints; exit0, no compiler errors |
| Source/include/generated-header verifier |60 files and23 generated headers passed |
| Native navigation automation |3 passed,1 failed; same assertion reproduced before the moves |
| PIE/gameplay | Deferred to user as explicitly requested |
| Windows packaging | Not attempted: required game compilation fails |

[SOURCE_REFACTOR_VERIFICATION.json](SOURCE_REFACTOR_VERIFICATION.json) contains19 build/project-generation receipts, log hashes, source preservation, Blueprint evidence and current/control test outcomes. Raw logs and snapshots are in `Saved/SourceRefactor/20261111/`.

### Exact reproduction commands (PowerShell from project root)

```powershell
$refactorProject = 'E:/LeVietThanh1412-Product/VDT-Phase2-Project/VDTPhase2Project.uproject'
$refactorBuild = 'F:/Epic Games/UE_5.6/Engine/Build/BatchFiles/Build.bat'
& $refactorBuild VDTPhase2ProjectEditor Win64 Development "-Project=$refactorProject" -WaitMutex -NoHotReloadFromIDE
& $refactorBuild VDTPhase2ProjectEditor Win64 Development "-Project=$refactorProject" -WaitMutex -NoHotReloadFromIDE -ForceUnity -DisableAdaptiveUnity
& $refactorBuild VDTPhase2Project Win64 Development "-Project=$refactorProject" -WaitMutex -NoHotReloadFromIDE
& $refactorBuild VDTPhase2Project Win64 Shipping "-Project=$refactorProject" -WaitMutex -NoHotReloadFromIDE
& 'F:/Epic Games/UE_5.6/Engine/Binaries/ThirdParty/DotNet/8.0.300/win-x64/dotnet.exe' 'F:/Epic Games/UE_5.6/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll' -ProjectFiles "-Project=$refactorProject" -Game -Engine -VSCode
python Tools/Testing/verify_source_organization.py --output Saved/SourceRefactor/STRUCTURAL_VERIFICATION.json
```

```powershell
$refactorCmd = 'F:/Epic Games/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
& $refactorCmd $refactorProject '-DisablePlugins=SpecialAgent,FunplayMCP' -run=pythonscript '-script=E:/LeVietThanh1412-Product/VDT-Phase2-Project/Tools/Testing/verify_source_refactor_assets.py' -NullRHI -NoSound -NoSplash -NoAutoSave -Unattended
& $refactorCmd $refactorProject /Engine/Maps/Entry '-DisablePlugins=SpecialAgent,FunplayMCP' -NullRHI -NoSound -NoSplash -NoAutoSave -Unattended '-ExecCmds=Automation RunTests ShipEnemyNavigation' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=E:/LeVietThanh1412-Product/VDT-Phase2-Project/Saved/SourceRefactor/Automation'
```

Plugin exclusions apply only to headless checks, not normal project configuration. All gameplay modules remain enabled. Inspect individual test results: TestExit returns process0 even when one native assertion fails. A zero process exit is not a passing test suite.

## 6. Regression analysis

**Fixed structural defects:** the initial formatted worktree had UHT errors. A first partial-migration local include error was corrected before the next group; all later group builds and both final Editor modes pass. Non-include source and the dependency graph are preserved.

**Existing game-build blocker:** the baseline and final runtime builds have72 identical error signatures: Windows max macro collision in CesiumGltf PropertyTransformations.h (C4003) and resulting C2589/C2760/C3878 errors. Engine/third-party files and required module configuration were not altered to hide it. The attached historical log also mentions Slate GetCurrentTime/GetTickCount; current standalone game builds reproduce the Cesium family, not a new Slate regression. Packaging is blocked by its compile prerequisite.

**Existing native assertion:** both the refactored DLL and an independently built copy of repaired pre-move source fail `Component.IsolatedWorld`: `A real obstruction inside the turning envelope suppresses steering`. PublicState, Geometry and RealOSM pass in both complete runs. The isolated control copies the same Config, shares the same Content through a local junction and includes the same independently validated OSM fixture. Its source comes from the exact pre-move snapshot. An earlier incomplete control omitted that Data fixture; it is retained as diagnostic evidence and corrected before comparison. Neither production collision/steering nor the test assertions were changed to make tests green.

**Headless editor-plugin issue:** the first Python commandlet stopped in SpecialAgent's unchanged unconditional Slate status-widget construction, before the project checker ran. Optional editor MCP plugins were excluded for the validation processes only. The portable checker then finished with exit0 and0 commandlet errors/warnings. Plugin files and normal Editor settings are untouched.

No PIE, natural battle, physics motion or cooked-game certification is inferred from source hashes/builds. The preservation checker is a one-off refactor acceptance guard; future intentional gameplay changes require reviewing/updating its baseline.

## 7. Git state and review

Branch remains `refactor/code`, HEAD `76025f7228b280f1009de309efa2c46351aa2596`; local main remains `a1ea046232503b836101caa844eb50dd1defad1f`. Commits created:0. No push, branch switch, main merge or history rewrite occurred.

Pre-existing formatting made a silent combined commit inappropriate, so the explicitly allowed uncommitted-review path was used. Source moves, includes, tooling and reports remain uncommitted/unstaged. Intent-to-add entries make new paths visible without staging file contents. Old deletions/new additions in Source Control are moves, not lost features. The focused patch shows all60 renames without formatting noise.

UBT's VSCode generator also rewrote `.ignore`, adding two search-exclusion patterns. The generated version was archived and the original clean search rules restored; no unrelated `.ignore` change remains.

Content, Config, Plugins, uproject, Build.cs and Target.cs have no diff against the original HEAD. No Unreal asset/third-party file was renamed or deleted. Only empty old source directories were removed. User IDE settings/workspace were backed up before regeneration; only generated compile databases/response files changed. Review and run manual tests before choosing how to commit.

## 8. Remaining risks and manual handoff

1. Resolve the separate Cesium Windows macro build issue before expecting Game Development/Shipping executables or packaging. It was explicitly outside the structure-only change scope.
2. Investigate the unchanged isolated-world obstruction assertion separately; this native suite is not entirely green.
3. Manually run MainMapLevel: manual A/B and seeded setup; Medium/Small movement and both rudder directions; buoyancy; aim/fire/reload; six-ship AI navigation/combat/retreat; death/counters/result; return/restart; spectator controls. This is the deferred user validation.
4. Check any out-of-repository C++ consumers for new include paths. Known repository consumers and reflected identities are verified.
5. Keep the original formatting snapshot and review patch until review is finished. The source-only baseline/control project and Content junction are ignored evidence under Saved, not runtime assets or an additional Git worktree.
6. Game Development Studio's asset/visual/performance CLI was not on PATH; none of those workflows was required or executed. Native Unreal tools supplied the build/reference evidence for this source-only task.
