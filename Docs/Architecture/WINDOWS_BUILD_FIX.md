# Windows build and packaging repair — 2026-11-11

This follow-up addresses the user's new packaging-failure log after the source-layout refactor. Historical failed build receipts in `SOURCE_REFACTOR_VERIFICATION.json` remain intact. The working branch is `refactor/code`; this repair does not commit, push or merge changes.

## Causes and changes

1. **Windows macros imported by Cesium expected-lite.** In the exception-disabled Game target, `nonstd/expected.hpp` takes its SEH path and directly imports `windows.h`. The SDK's `min`, `max` and `GetCurrentTime` macros then corrupt Cesium/GLM/Unreal declarations. MSVC preprocessing traces in `Saved/BuildMacroFix/20261111/` establish the actual include chain. Importing the SDK also introduces GDI's `OPAQUE`, which conflicts with `CesiumGltf::Material::AlphaMode::OPAQUE`.

   Added `Private/Utilities/NavalCesiumCompatibility.h`: on Windows it initializes the SDK through Unreal's `WindowsHWrapper.h`, then removes the remaining GDI `OPAQUE` macro. Eleven implementation files include this private header immediately before their first direct Cesium import. Existing include order otherwise stays intact. No Engine/Cesium file, toolchain, exception setting, gameplay algorithm or asset is changed. The imported file list is recorded in `FINAL_COMPATIBILITY_INCLUDES.json` under the evidence directory.

2. **Editor-only SpecialAgent widget crashes cooking.** After compilation succeeded, a real UAT cook failed with `CurrentBaseApplication.IsValid()` from `SMCPStatusBarWidget::Construct`, called by `FSpecialAgentModule::StartupModule`. Changed only its project-local plugin module type from `Editor` to `EditorNoCommandlet`. Unreal's module loader then retains the module for interactive Editor sessions and excludes it from commandlets, where there is no Slate application. No gameplay plugin is disabled.

3. **Transient Editor DLL lock during verification.** An Editor opened during compilation and prevented linking `UnrealEditor-VDTPhase2Project.dll` (`LNK1104`). At the user's restart request, Save All was invoked and the Editor closed normally. The same Editor build then linked successfully without further code changes. This lock is distinct from the compiler and cooking defects above.

## Verification and evidence

Raw logs, immutable pre-fix copies and the focused `BUILD_FIX_ONLY.patch` are retained in `Saved/BuildMacroFix/20261111/`. This patch compares against the already-formatted, already-relocated source, avoiding the unrelated formatting/move diff against Git HEAD.

| Check | Result |
|---|---|
| Game Win64 Development | Passed — `GAME_DEVELOPMENT_COMMON_BOUNDARY.log` |
| Game Win64 Shipping | Passed — `GAME_SHIPPING.log` |
| Editor Win64 Development | Passed after closing the Editor — `EDITOR_DEVELOPMENT_RETRY.log` |
| Exact non-include bytes of all eleven changed original cpp files | Passed — `BUILD_FIX_PRESERVATION.json` |
| Original 60 source files / 23 generated headers | Passed — `STRUCTURAL_VERIFICATION.json` |
| Native identities and Blueprint commandlet, normal plugin configuration | Passed — 41 native types, 39 Blueprints; exit 0, 0 errors and 0 warnings in `REFLECTION_BLUEPRINT.log`; no assets saved |
| Interactive Editor restart requested by the user | Passed — MainMapLevel viewport observed after restart; SpecialAgent status widget registered normally; `EDITOR_RESTART_SMOKE.log` |
| Full UAT Development packaging | Passed — `PACKAGE_DEVELOPMENT_FIXED.log`; all 700 packages cooked, 0 errors / 0 warnings; cook, pak/IoStore, stage and archive completed; UAT exit 0 |

The source-organization guard now explicitly allows one compatibility header with a pinned normalized hash and exactly eleven approved import sites. Original function-body digests and dependency-order baselines are retained. Unexpected extra files, modified bodies, missing imports and an import after Cesium still fail verification.

The final machine receipt is [WINDOWS_BUILD_FIX_VERIFICATION.json](WINDOWS_BUILD_FIX_VERIFICATION.json), including build-log and packaged-artifact hashes. The Windows Development archive is `Saved/BuildMacroFix/20261111/Package/Windows/`; launch `VDTPhase2Project.exe` there. Shipping compilation passed separately; no Shipping package was generated.

The first full cook took about 22 minutes compiling uncached shaders for the configured SM5/SM6 formats. Opening the interactive Editor during cooking briefly crossed the cooker's free-memory threshold; it was saved and closed normally to free roughly 4 GB, then reopened after packaging. No renderer, quality or shader-format setting was changed.

The non-preferred MSVC warning and existing Niagara/Chaos deprecation warnings are separate from these errors. No claim of new PIE, full battle, steering or buoyancy verification follows from native builds or source hashes. The previously documented isolated-world navigation assertion remains outside this build-compatibility repair.

## Reproduction

From the project root in PowerShell:

```powershell
$buildFixProject = "$PWD/VDTPhase2Project.uproject"
$buildFixBuild = 'F:/Epic Games/UE_5.6/Engine/Build/BatchFiles/Build.bat'
& $buildFixBuild VDTPhase2Project Win64 Development "-Project=$buildFixProject" -WaitMutex -NoHotReloadFromIDE
& $buildFixBuild VDTPhase2Project Win64 Shipping "-Project=$buildFixProject" -WaitMutex -NoHotReloadFromIDE
# Save and close interactive Unreal Editors before rebuilding their DLL:
& $buildFixBuild VDTPhase2ProjectEditor Win64 Development "-Project=$buildFixProject" -WaitMutex -NoHotReloadFromIDE
python Tools/Testing/verify_source_organization.py
& 'F:/Epic Games/UE_5.6/Engine/Build/BatchFiles/RunUAT.bat' BuildCookRun "-project=$buildFixProject" -nop4 -utf8output -platform=Win64 -clientconfig=Development -target=VDTPhase2Project -build -cook -stage -pak -iostore -archive "-archivedirectory=$PWD/Saved/BuildMacroFix/20261111/Package" -unattended -nocompileeditor -skipbuildeditor -installed
```

The local terrain/imagery URLs and local dataset requirements remain unchanged. Successful packaging alone would not prove that those external files exist on another computer.
