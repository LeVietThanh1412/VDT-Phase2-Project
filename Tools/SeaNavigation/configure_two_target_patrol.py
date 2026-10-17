"""Persist the existing two Target Points as an A-first actor-backed patrol.

Run through SpecialAgent after compilation. Never spawns or relocates actors.
"""
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None,"Stop PIE before saving configuration"
labels=["TP_ShipEnemy_AStarDestination","TP_ShipEnemy_OpenSeaDestination"]
descs=unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs if str(d.label) in labels+["BP_ShipEnemy"]])
actors_api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=actors_api.get_all_level_actors()
targets=[next(a for a in actors if a.get_actor_label()==label) for label in labels]
enemy=next(a for a in actors if a.get_class().get_name()=="BP_ShipEnemy_C")
v=lambda p:[p.x,p.y,p.z]
before={a.get_actor_label():dict(world=v(a.get_actor_location()),rotation=[a.get_actor_rotation().pitch,a.get_actor_rotation().yaw,a.get_actor_rotation().roll],scale=v(a.get_actor_scale3d()),path=a.get_path_name(),package=a.get_package().get_name()) for a in targets+[enemy]}
snapshot=root/"Docs/ShipEnemyNavigation/TWO_TARGET_EDITOR_ORIGINAL.json"
if not snapshot.exists():snapshot.write_text(json.dumps(before,indent=2),encoding="utf8")
assert all(a.get_component_by_class(unreal.CesiumGlobeAnchorComponent) for a in targets)
bp=unreal.load_asset("/Game/ShipEnemy/BluePrint/BP_ShipEnemy")
subobjects=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
templates=[]
for handle in subobjects.k2_gather_subobject_data_for_blueprint(bp):
    obj=unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    if isinstance(obj,unreal.ShipEnemyNavigationComponent):templates.append(obj)
assert len(templates)==1
bp.modify();templates[0].modify()
templates[0].set_editor_property("navigation_mode",unreal.ShipNavigationMode.PATROL)
templates[0].set_editor_property("new_navigation_enabled",True)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
validation=unreal.ShipEnemyNavigationComponent.validate_installed_blueprint()
assert validation.startswith("OK:"),validation
# Blueprint compilation can reconstruct components; resolve live instances again.
enemy=next(a for a in actors_api.get_all_level_actors() if a.get_class().get_name()=="BP_ShipEnemy_C")
enemy.modify()
enemy.set_editor_property("DestinationActor",targets[0])
assert enemy.get_editor_property("bAIEnabled"),"Existing AI command branch is disabled"
for a in targets+[enemy]:a.modify();a.set_editor_property("is_spatially_loaded",False)
# Each component edit can reconstruct this Blueprint's SCS components. Never
# retain the old component wrapper across a set_editor_property call.
settings=dict(navigation_mode=unreal.ShipNavigationMode.PATROL,new_navigation_enabled=True,draw_debug=True,
    patrol_point_a=unreal.Vector(),patrol_point_b=unreal.Vector(),patrol_actor_a=targets[0],patrol_actor_b=targets[1])
for key,value in settings.items():
    nav=enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
    nav.modify();nav.set_editor_property(key,value)
    nav=enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
    assert nav.get_editor_property(key)==value,"Reconstruction discarded "+key
nav=enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
assert nav.patrol_actor_a==targets[0] and nav.patrol_actor_b==targets[1]
for a in targets+[enemy]:
    assert v(a.get_actor_location())==before[a.get_actor_label()]["world"],"Configuration changed a transform"
packages=[bp.get_package(),enemy.get_package()]+[a.get_package() for a in targets]
assert unreal.EditorLoadingAndSavingUtils.save_packages(packages,False)
report=dict(validation=validation,mode=str(nav.navigation_mode),target_a=targets[0].get_path_name(),target_b=targets[1].get_path_name(),transforms=before,saved_packages=[p.get_name() for p in packages])
(root/"Docs/ShipEnemyNavigation/TWO_TARGET_SAVED_CONFIG.json").write_text(json.dumps(report,indent=2),encoding="utf8")
print(json.dumps(report))
