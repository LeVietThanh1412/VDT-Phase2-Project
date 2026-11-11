"""Run inside Unreal's Python commandlet; resolve native types and compile BPs.

No asset saves or PIE. Invoke UnrealEditor-Cmd with
-run=pythonscript -script=<this file> -NullRHI -Unattended.
SpecialAgent is EditorNoCommandlet, so no plugin exclusions are required.
Inspect the commandlet log for compiler errors before accepting the receipt.
"""
import unreal,json,hashlib
from pathlib import Path
root=Path(unreal.Paths.project_dir());folder=root/'Saved/SourceRefactor';folder.mkdir(parents=True,exist_ok=True)
expected=json.loads((root/'Docs/Architecture/SOURCE_MIGRATION_MANIFEST.json').read_text(encoding='utf-8'))['expected_reflected_types'];types=[]
for item in expected:
    obj=unreal.load_class(None,item['path']) if item['kind']=='class' else unreal.load_object(None,item['path'])
    assert obj,item['path'];assert obj.get_path_name()==item['path']
    types.append(dict(**item,loaded=True))
registry=unreal.AssetRegistryHelpers.get_asset_registry();registry.search_all_assets(True);blueprints=[]
for data in registry.get_assets_by_path('/Game',True):
    kind=str(data.asset_class_path.asset_name)
    if kind not in ['Blueprint','AnimBlueprint','WidgetBlueprint']:continue
    bp=data.get_asset();assert bp,str(data.package_name)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cls=bp.generated_class();assert cls,str(data.package_name)
    blueprints.append(dict(asset=str(data.package_name),generated_class=cls.get_path_name()))
assert len(blueprints)>=39,len(blueprints)
assert (root/'Content/Levels/MainMapLevel.umap').is_file()
result=dict(status='REQUESTS_COMPLETED_REQUIRES_LOG_CHECK',types=types,blueprints=blueprints,count=len(blueprints),native_dll_sha256=hashlib.sha256((root/'Binaries/Win64/UnrealEditor-VDTPhase2Project.dll').read_bytes()).hexdigest(),asset_saves=0,scope='Commandlet class resolution and Blueprint compilation only; no PIE or gameplay certification.')
(folder/'REFLECTION_BLUEPRINT_CHECK.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print('Refactor verification: resolved',len(types),'native types and compiled',len(blueprints),'Blueprints without saving assets')
