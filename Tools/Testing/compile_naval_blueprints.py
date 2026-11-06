"""Compile project Blueprint classes without saving/reconstructing map assets."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir());reg=unreal.AssetRegistryHelpers.get_asset_registry();rows=[]
for data in reg.get_assets_by_path('/Game',True):
    kind=str(data.asset_class_path.asset_name)
    if kind not in ['Blueprint','AnimBlueprint','WidgetBlueprint']:continue
    path=str(data.package_name);bp=data.get_asset();assert bp,path
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    generated=bp.generated_class();assert generated,path+' missing generated class'
    rows.append(dict(asset=path,kind=kind,generated_class=generated.get_path_name(),compiled=True))
out=root/'Saved/NAVAL_BLUEPRINT_COMPILE.json'
out.write_text(json.dumps(dict(status='COMPILE_REQUESTS_COMPLETED_REQUIRES_LOG_CHECK',blueprints=rows,count=len(rows)),indent=2),encoding='utf-8')
print('Compiled',len(rows),'Blueprints; check Editor log for compiler diagnostics')
