"""Read-only actual imported mesh, original skeleton/weapon and actor audit."""
import json,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None,'Stop PIE before inspecting editor assets'
def vector(v):return [v.x,v.y,v.z]
def query(fn):
    try:return fn()
    except Exception as exc:return {'unavailable':str(exc)}
asset=unreal.load_asset('/Game/ShipSmall/Asset/sweedenmilitaryboat');assert asset
registry=unreal.AssetRegistryHelpers.get_asset_registry()
small=dict(path=asset.get_path_name(),type=asset.get_class().get_name(),assets=[dict(path=str(a.package_name),type=str(a.asset_class_path)) for a in registry.get_assets_by_path('/Game/ShipSmall',True)])
if isinstance(asset,unreal.StaticMesh):
    small['bounds']=query(lambda:str(asset.get_bounding_box()))
    small['lod_count']=query(lambda:unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_lod_count(asset))
    small['lod_sections']=query(lambda:[asset.get_num_sections(i) for i in range(small['lod_count'])])
    small['nanite']=query(lambda:str(asset.get_editor_property('nanite_settings')))
    small['body_setup']=query(lambda:str(asset.get_editor_property('body_setup')))
    small['sockets']=query(lambda:[dict(name=str(s.get_editor_property('socket_name')),position=vector(s.get_editor_property('relative_location'))) for s in asset.get_editor_property('sockets')])
    small['materials']=[dict(slot=str(m.material_slot_name),material=m.material_interface.get_path_name() if m.material_interface else None) for m in asset.get_editor_property('static_materials')]
    small['has_skeleton']=False
report=dict(small=small,actors=[],original_skeleton=None,materials={})
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for a in actors:
    if a.get_component_by_class(unreal.ShipWeaponComponent) or a.get_actor_label().lower().startswith('sweeden'):
        components=[]
        for c in a.get_components_by_class(unreal.SceneComponent):
            parent=c.get_attach_parent()
            entry=dict(name=c.get_name(),type=c.get_class().get_name(),parent=parent.get_name() if parent else None,attach_socket=str(c.get_attach_socket_name()),relative=str(c.get_relative_transform()),world=vector(c.get_world_location()))
            if isinstance(c,unreal.PrimitiveComponent):entry.update(physics=c.is_simulating_physics(),collision=str(c.get_collision_enabled()),visible=c.is_visible(),local_bounds=query(lambda:str(c.get_local_bounds())))
            if isinstance(c,unreal.StaticMeshComponent):entry['mesh']=c.static_mesh.get_path_name() if c.static_mesh else None
            if isinstance(c,unreal.SkeletalMeshComponent):
                mesh=c.get_editor_property('skeletal_mesh_asset');entry['mesh']=mesh.get_path_name() if mesh else None
                if mesh and report['original_skeleton'] is None:
                    skeleton=mesh.get_editor_property('skeleton')
                    report['original_skeleton']=dict(mesh=mesh.get_path_name(),skeleton=skeleton.get_path_name() if skeleton else None,bones=[dict(index=i,name=str(c.get_bone_name(i)),parent=str(c.get_parent_bone(c.get_bone_name(i))),component_transform=str(c.get_socket_transform(c.get_bone_name(i),unreal.RelativeTransformSpace.RTS_COMPONENT))) for i in range(c.get_num_bones())],sockets=[str(v) for v in c.get_all_socket_names()])
            components.append(entry)
        report['actors'].append(dict(label=a.get_actor_label(),path=a.get_path_name(),transform=str(a.get_actor_transform()),components=components))
for data in registry.get_assets_by_path('/Game/ShipSmall',True):
    obj=unreal.load_asset(str(data.package_name))
    if isinstance(obj,unreal.Material):
        report['materials'][obj.get_path_name()]=dict(blend=str(obj.get_editor_property('blend_mode')),shading=str(obj.get_editor_property('shading_model')),two_sided=obj.get_editor_property('two_sided'),base_color_input=query(lambda:str(unreal.MaterialEditingLibrary.get_material_property_input_node(obj,unreal.MaterialProperty.MP_BASE_COLOR))))
    elif isinstance(obj,unreal.Texture2D):
        report['materials'][obj.get_path_name()]=dict(type='Texture2D',srgb=obj.get_editor_property('srgb'),compression=str(obj.get_editor_property('compression_settings')),dimensions=query(lambda:[obj.blueprint_get_size_x(),obj.blueprint_get_size_y()]))
report['dirty_content']=query(lambda:[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()])
report['dirty_maps']=query(lambda:[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()])
(root/'Docs/NavalCombat/SMALL_UNREAL_ASSET_AUDIT.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(dict(small=small,original_skeleton=report['original_skeleton']['skeleton'] if report['original_skeleton'] else None,bones=len(report['original_skeleton']['bones']) if report['original_skeleton'] else 0,actors=[a['label'] for a in report['actors']],materials=report['materials'],dirty_content=report['dirty_content'],dirty_maps=report['dirty_maps']),indent=2))
