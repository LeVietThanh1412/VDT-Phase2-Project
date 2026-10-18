"""Persistent MainMapLevel 3v3 configuration; idempotent, reuses original anchors."""
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir());assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
plan=json.loads((root/'Docs/NavalCombat/BATTLEFIELD_PLAN.json').read_text())
chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
descs=unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
names=[x['label'] for x in plan['ships']]+list(plan['targets'])+['PlayerStart','BP_AntiShip']
unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs if str(d.label) in names])
actors=api.get_all_level_actors();lookup={a.get_actor_label():a for a in actors}
original=lookup['BP_ShipEnemy'];cls=original.get_class()
geo=original.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
v=lambda x:[x.x,x.y,x.z]
pose=lambda a:dict(location=v(a.get_actor_location()),rotation=[a.get_actor_rotation().pitch,a.get_actor_rotation().yaw,a.get_actor_rotation().roll],scale=v(a.get_actor_scale3d()))
anchors={name:lookup[name] for name in list(plan['targets'])[:2]}
before={name:pose(actor) for name,actor in anchors.items()}
snapshot=root/'Docs/NavalCombat/BATTLE_EDITOR_BEFORE.json'
if not snapshot.exists():snapshot.write_text(json.dumps({a.get_actor_label():pose(a) for a in actors if isinstance(a,unreal.Pawn) or isinstance(a,unreal.PlayerStart) or isinstance(a,unreal.TargetPoint)},indent=2))
def world(en,height):
    ecef=unreal.Vector(*chart['origin_ecef'])+unreal.Vector(*chart['east_axis'])*en[0]+unreal.Vector(*chart['north_axis'])*en[1]
    llh=geo.transform_unreal_position_to_longitude_latitude_height(geo.transform_earth_centered_earth_fixed_position_to_unreal(ecef));llh.z=height
    return geo.transform_longitude_latitude_height_position_to_unreal(llh)
def anchor(actor):
    actor.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    if not actor.get_component_by_class(unreal.CesiumGlobeAnchorComponent):
        handles=sub.k2_gather_subobject_data_for_instance(actor)
        parent=next(h for h in handles if unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h))==actor)
        _,error=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.CesiumGlobeAnchorComponent))
        assert not str(error),str(error)
    actor.modify();actor.set_editor_property('is_spatially_loaded',False)
changed=[]
for name,en in list(plan['targets'].items())[2:]:
    target=lookup.get(name)
    if not target:
        target=api.spawn_actor_from_class(unreal.TargetPoint,world(en,0));assert target
        target.set_actor_label(name);anchor(target)
    anchors[name]=target;changed.append(target)
for i,spec in enumerate(plan['ships'][1:]):
    ship=lookup.get(spec['label'])
    if not ship:
        ship=api.spawn_actor_from_class(cls,world(spec['en_m'],6.8),unreal.Rotator(pitch=0,yaw=spec['world_yaw'],roll=0));assert ship
        ship.set_actor_label(spec['label'])
    ship.modify();ship.set_editor_property('is_spatially_loaded',False)
    ship.set_actor_location(world(spec['en_m'],6.8),False,True)
    ship.set_actor_rotation(unreal.Rotator(pitch=0,yaw=spec['world_yaw'],roll=0),True)
    ship.set_editor_property('auto_possess_ai',unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED)
    ship.set_editor_property('ai_controller_class',unreal.AIController.static_class())
    ship.tags=list(set(list(ship.tags)+[unreal.Name('Battle3v3'),unreal.Name('Friendly' if spec['team']==0 else 'Enemy')]))
    assert ship.get_editor_property('bAIEnabled')
    # Each SCS edit may reconstruct all components. Resolve every time.
    settings=dict(new_navigation_enabled=True,navigation_mode=unreal.ShipNavigationMode.PATROL,role=unreal.ShipEnemyRole.FIGHTER,
        patrol_actor_a=anchors[spec['patrol_a']],patrol_actor_b=anchors[spec['patrol_b']],patrol_point_a=unreal.Vector(),patrol_point_b=unreal.Vector(),
        cruise_speed_mps=8,patrol_wait_seconds=4+i*2,patrol_startup_delay_seconds=i*4,safe_ship_separation_m=300,draw_debug=True)
    for key,value in settings.items():
        component=ship.get_component_by_class(unreal.ShipEnemyNavigationComponent);component.modify();component.set_editor_property(key,value)
    combat=dict(faction=spec['team'],enable_combat=True,enable_hit_and_run=True,detection_radius_m=1800,engagement_radius_m=1200,
        lose_target_radius_m=2200,minimum_retreat_distance_m=250,safe_reengagement_distance_m=600,fire_cooldown_seconds=4,target_scan_interval=.5,draw_debug=True)
    for key,value in combat.items():
        component=ship.get_component_by_class(unreal.ShipCombatComponent);component.modify();component.set_editor_property(key,value)
    weapon=ship.get_component_by_class(unreal.ShipWeaponComponent)
    assert weapon and ship.get_component_by_class(unreal.ShipEnemyNavigationComponent).patrol_actor_a==anchors[spec['patrol_a']]
    changed.append(ship)
# GameMode creates the only player pawn. No second placed BP_Ship is added.
player_start=lookup['PlayerStart'];player_start.modify();player_start.set_actor_location(world(plan['ships'][0]['en_m'],14.7),False,True)
player_start.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-30,roll=0),True);anchor(player_start);changed.append(player_start)
# Existing unrelated anti-ship pawn is a legacy combat participant. Retain its
# level actor and assets for editing, but exclude it from this six-ship game.
for actor in actors:
    if actor.get_class().get_name()=='BP_AntiShip_C':
        actor.modify();actor.set_editor_property('is_editor_only_actor',True);changed.append(actor)
assert all(pose(anchors[name])==value for name,value in before.items()),'Existing enemy target transform changed'
all_actors=api.get_all_level_actors()
ships=[x for x in all_actors if x.get_class()==cls]
assert len(ships)==5 and len([x for x in all_actors if x.get_class().get_name()=='BP_Ship_C'])==0
assert all(len([x for x in all_actors if x.get_actor_label()==name])==1 for name in anchors)
assert unreal.ShipEnemyNavigationComponent.validate_installed_blueprint().startswith('OK:')
packages=list({x.get_package() for x in changed})
assert unreal.EditorLoadingAndSavingUtils.save_packages(packages,False)
report=dict(ships=[dict(label=x.get_actor_label(),pose=pose(x),team=x.get_component_by_class(unreal.ShipCombatComponent).faction,
    a=x.get_component_by_class(unreal.ShipEnemyNavigationComponent).patrol_actor_a.get_actor_label(),
    b=x.get_component_by_class(unreal.ShipEnemyNavigationComponent).patrol_actor_b.get_actor_label(),package=x.get_package().get_name()) for x in ships],
    player_start=pose(player_start),enemy_anchors_unchanged=before,packages=[p.get_name() for p in packages])
(root/'Docs/NavalCombat/BATTLE_SAVED_CONFIGURATION.json').write_text(json.dumps(report,indent=2),encoding='utf8')
print(json.dumps(report,indent=2))
