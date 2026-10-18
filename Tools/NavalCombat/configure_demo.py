"""Run in the live editor through SpecialAgent; no spawned replacement actors."""
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=api.get_all_level_actors()
enemy=next(a for a in actors if a.get_class().get_name()=="BP_ShipEnemy_C")
a=next(a for a in actors if a.get_actor_label()=="TP_ShipEnemy_AStarDestination")
b=next(a for a in actors if a.get_actor_label()=="TP_ShipEnemy_OpenSeaDestination")
before={x.get_actor_label():str(x.get_actor_transform()) for x in [enemy,a,b]}
# The user moved A into a rejected cell after the navigation acceptance test.
# Restore that existing actor's verified horizontal position, retaining its Z,
# orientation and scale. No coastline data or navigation algorithm is changed.
rotation=a.get_actor_rotation()
a.modify();a.set_actor_location(unreal.Vector(-40620,117950,a.get_actor_location().z),False,True)
a.set_actor_rotation(rotation,True)
enemy.modify();assert enemy.get_editor_property("bAIEnabled"),"Existing Blueprint AI force branch disabled"
settings=dict(new_navigation_enabled=True,navigation_mode=unreal.ShipNavigationMode.PATROL,
    role=unreal.ShipEnemyRole.FIGHTER,patrol_actor_a=a,patrol_actor_b=b,draw_debug=True)
for key,value in settings.items():
    n=enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
    n.modify();n.set_editor_property(key,value)
    assert enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent).get_editor_property(key)==value
c=enemy.get_component_by_class(unreal.ShipCombatComponent)
assert c and enemy.get_component_by_class(unreal.ShipWeaponComponent)
c.modify();c.set_editor_property("draw_debug",True)
# Blueprint defaults also start in Fighter; references remain instance-only.
bp=unreal.load_asset("/Game/ShipEnemy/BluePrint/BP_ShipEnemy")
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
for h in sub.k2_gather_subobject_data_for_blueprint(bp):
    obj=unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h))
    if isinstance(obj,unreal.ShipEnemyNavigationComponent):
        obj.modify();obj.set_editor_property("role",unreal.ShipEnemyRole.FIGHTER)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.ShipEnemyNavigationComponent.validate_installed_blueprint().startswith("OK:")
# Compile can reconstruct instance components.
enemy=next(x for x in api.get_all_level_actors() if x.get_class().get_name()=="BP_ShipEnemy_C")
n=enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
assert n.role==unreal.ShipEnemyRole.FIGHTER and n.patrol_actor_a==a and n.patrol_actor_b==b
packages=[bp.get_package(),enemy.get_package(),a.get_package()]
assert unreal.EditorLoadingAndSavingUtils.save_packages(packages,False)
report=dict(before=before,after={x.get_actor_label():str(x.get_actor_transform()) for x in [enemy,a,b]},
    role=str(n.role),A=a.get_path_name(),B=b.get_path_name(),navigation_validation=unreal.ShipEnemyNavigationComponent.validate_installed_blueprint())
(root/"Docs/NavalCombat/DEMO_CONFIG.json").write_text(json.dumps(report,indent=2),encoding="utf8")
print(json.dumps(report))
