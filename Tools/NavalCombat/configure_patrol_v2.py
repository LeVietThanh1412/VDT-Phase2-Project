"""Apply the audited V2 water layout to existing five ships, with explicit endpoints."""
import json
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
plan=json.loads((root/'Docs/NavalCombat/PATROL_V2_PLAN.json').read_text())
chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
names={s['label'] for s in plan['ships']}|set(plan['targets'])|{'PlayerStart'}
unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs() if str(d.label) in names])
actors=api.get_all_level_actors();lookup={a.get_actor_label():a for a in actors}
assert all(s['label'] in lookup for s in plan['ships']),'Current ships changed; reaudit before configuring'
geo=lookup['BP_ShipEnemy'].get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
def world(en,height):
    p=unreal.Vector(*chart['origin_ecef'])+unreal.Vector(*chart['east_axis'])*en[0]+unreal.Vector(*chart['north_axis'])*en[1]
    llh=geo.transform_unreal_position_to_longitude_latitude_height(geo.transform_earth_centered_earth_fixed_position_to_unreal(p))
    llh.z=height
    return geo.transform_longitude_latitude_height_position_to_unreal(llh)
def mark(a):a.modify();a.set_editor_property('is_spatially_loaded',False)
changed=[]
for name,en in plan['targets'].items():
    a=lookup.get(name)
    if a is None:
        a=api.spawn_actor_from_class(unreal.TargetPoint,world(en,0));a.set_actor_label(name);lookup[name]=a
        handles=sub.k2_gather_subobject_data_for_instance(a)
        parent=next(h for h in handles if unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h))==a)
        _,error=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.CesiumGlobeAnchorComponent));assert not str(error)
    mark(a)
    # Keep the valid original A and Friendly anchors at their exact world transforms.
    if name not in ['TP_ShipEnemy_AStarDestination','TP_Friendly_Patrol_A','TP_Friendly_Patrol_B']:
        a.root_component.set_mobility(unreal.ComponentMobility.MOVABLE);a.set_actor_location(world(en,0),False,True)
    changed.append(a)
for i,spec in enumerate(plan['ships']):
    a=lookup[spec['label']];mark(a)
    a.set_actor_location(world(spec['spawn_en_m'],6.8),False,True);a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=spec['yaw'],roll=0),True)
    assert a.get_editor_property('bAIEnabled');a.set_editor_property('auto_possess_ai',unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED)
    a.set_editor_property('auto_possess_player',unreal.AutoReceiveInput.DISABLED);a.set_editor_property('ai_controller_class',unreal.AIController.static_class())
    for key,value in dict(new_navigation_enabled=True,navigation_mode=unreal.ShipNavigationMode.PATROL,role=unreal.ShipEnemyRole.FIGHTER,
        patrol_actor_a=lookup[spec['a']],patrol_actor_b=lookup[spec['b']],patrol_offset_a=unreal.Vector(),patrol_offset_b=unreal.Vector(),
        start_at_patrol_b=True,cruise_speed_mps=20,arrival_radius_m=80,patrol_wait_seconds=3,patrol_startup_delay_seconds=i*2,
        braking_deceleration_mps2=1.5,max_lateral_acceleration_mps2=.32,safe_ship_separation_m=350,draw_debug=False).items():
        n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent);n.modify();n.set_editor_property(key,value)
    c=a.get_component_by_class(unreal.ShipCombatComponent);c.modify();c.set_editor_property('faction',spec['team']);c.set_editor_property('enable_combat',True)
    a.set_editor_property('DestinationActor',lookup[spec['b']]);changed.append(a)
player=lookup['PlayerStart'];mark(player);player.set_actor_location(world(plan['player_en_m'],14.7),False,True)
player.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-90,roll=0),True);changed.append(player)
assert unreal.ShipEnemyNavigationComponent.validate_installed_blueprint().startswith('OK:')
assert unreal.EditorLoadingAndSavingUtils.save_packages(list({a.get_package() for a in changed}),False)
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
print('Saved five existing AI, ten distinct endpoint actors, zero patrol offsets, water-validated starts, and one PlayerStart.')
