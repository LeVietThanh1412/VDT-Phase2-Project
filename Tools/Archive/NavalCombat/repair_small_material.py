"""Repair the confirmed disconnected supplied diffuse texture; keep source art."""
import unreal,json
from pathlib import Path
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None,'Stop PIE before material edits'
m=unreal.load_asset('/Game/ShipSmall/Asset/model');texture=unreal.load_asset('/Game/ShipSmall/Asset/shaded')
assert m and texture
before={str(p):str(unreal.MaterialEditingLibrary.get_material_property_input_node(m,p)) for p in [unreal.MaterialProperty.MP_BASE_COLOR,unreal.MaterialProperty.MP_ROUGHNESS,unreal.MaterialProperty.MP_METALLIC]}
sample=unreal.load_object(None,'/Game/ShipSmall/Asset/model.model:MaterialExpressionTextureSampleParameter2D_0')
assert isinstance(sample,unreal.MaterialExpressionTextureSampleParameter2D),'Expected actual imported shaded texture parameter'
assert sample.get_editor_property('texture')==texture
m.modify()
assert unreal.MaterialEditingLibrary.connect_material_property(sample,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
for label,value,prop in [('Roughness',.65,unreal.MaterialProperty.MP_ROUGHNESS),('Metallic',.1,unreal.MaterialProperty.MP_METALLIC)]:
    scalar=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant,-100,200 if label=='Roughness' else 350)
    scalar.set_editor_property('r',value);assert unreal.MaterialEditingLibrary.connect_material_property(scalar,'',prop)
unreal.MaterialEditingLibrary.recompile_material(m)
assert unreal.EditorAssetLibrary.save_loaded_asset(m)
after={str(p):str(unreal.MaterialEditingLibrary.get_material_property_input_node(m,p)) for p in [unreal.MaterialProperty.MP_BASE_COLOR,unreal.MaterialProperty.MP_ROUGHNESS,unreal.MaterialProperty.MP_METALLIC]}
assert unreal.MaterialEditingLibrary.get_material_property_input_node(m,unreal.MaterialProperty.MP_BASE_COLOR)==sample
Path(unreal.Paths.project_dir(),'Docs/NavalCombat/SMALL_MATERIAL_REPAIR.json').write_text(json.dumps(dict(before=before,after=after,source_texture=texture.get_path_name(),source_art_recolored=False,roughness=.65,metallic=.1),indent=2),encoding='utf-8')
print('OK: original 4K shaded atlas connected directly to Base Color; repaired Phong conversion defaults; saved')
