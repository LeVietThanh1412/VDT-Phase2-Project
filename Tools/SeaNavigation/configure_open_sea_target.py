"""Install the requested open-sea target without relocating existing markers."""
import json
from pathlib import Path
import unreal

assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
actors_api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
descs=unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs if str(d.label)=="BP_ShipEnemy"])
bp=unreal.load_asset("/Game/ShipEnemy/BluePrint/BP_ShipEnemy")
subobjects=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
templates=[]
for h in subobjects.k2_gather_subobject_data_for_blueprint(bp):
    obj=unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h))
    if isinstance(obj,unreal.ShipEnemyNavigationComponent):templates.append(obj)
assert len(templates)==1
bp.modify();templates[0].modify()
templates[0].set_editor_property("navigation_mode",unreal.ShipNavigationMode.DESTINATION)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
validation=unreal.ShipEnemyNavigationComponent.validate_installed_blueprint()
assert validation.startswith("OK:"),validation
actors=actors_api.get_all_level_actors()
enemy=next(a for a in actors if a.get_class().get_name()=="BP_ShipEnemy_C")
geo=enemy.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
label="TP_ShipEnemy_OpenSeaDestination"
target=next((a for a in actors if a.get_actor_label()==label),None)
if target is None:
    # Fixed ENU chart (-600,-2200)m: independently checked open water and a
    # direct safe segment from the user's placed ship; not a gameplay constant.
    chart=json.loads((Path(unreal.Paths.project_dir())/"Content/SeaNavigation/halong_navigation.json").read_text())
    ecef=unreal.Vector(*chart["origin_ecef"])-unreal.Vector(*chart["east_axis"])*600-unreal.Vector(*chart["north_axis"])*2200
    llh=geo.transform_unreal_position_to_longitude_latitude_height(geo.transform_earth_centered_earth_fixed_position_to_unreal(ecef))
    llh.z=0
    target=actors_api.spawn_actor_from_class(unreal.TargetPoint,geo.transform_longitude_latitude_height_position_to_unreal(llh))
    assert target
    target.set_actor_label(label)
    target.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    handles=subobjects.k2_gather_subobject_data_for_instance(target)
    parent=next(h for h in handles if unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h))==target)
    _,error=subobjects.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.CesiumGlobeAnchorComponent))
    assert not str(error),str(error)
assert target.get_component_by_class(unreal.CesiumGlobeAnchorComponent)
target.modify();target.set_editor_property("is_spatially_loaded",False)
enemy.modify();enemy.set_editor_property("DestinationActor",target)
enemy.set_editor_property("is_spatially_loaded",False)
nav=enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
nav.modify();nav.set_editor_property("navigation_mode",unreal.ShipNavigationMode.DESTINATION)
nav.set_editor_property("draw_debug",True)
packages=[bp.get_package(),enemy.get_package(),target.get_package()]
assert unreal.EditorLoadingAndSavingUtils.save_packages(packages,False)
v=lambda p:[p.x,p.y,p.z]
report=dict(validation=validation,target=target.get_actor_label(),target_path=target.get_path_name(),
    world=v(target.get_actor_location()),llh=v(target.get_component_by_class(unreal.CesiumGlobeAnchorComponent).get_longitude_latitude_height()),
    mode=str(nav.navigation_mode),packages=[p.get_name() for p in packages])
(Path(unreal.Paths.project_dir())/"Docs/ShipEnemyNavigation/OPEN_SEA_TARGET_SETUP.json").write_text(json.dumps(report,indent=2))
print(json.dumps(report))
