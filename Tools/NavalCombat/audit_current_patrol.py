"""Read the current saved/loaded scene before altering patrol assignments."""
import json,re
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
descs=unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs if re.search(r'Ship|Enemy|Friendly|TP_|PlayerStart',str(d.label),re.I)])
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
vec=lambda p:[p.x,p.y,p.z]
origin=unreal.GameplayStatics.get_world_origin_location(ed.get_editor_world())
def metric(a,g):
    q=g.transform_unreal_position_to_earth_centered_earth_fixed(a.get_actor_location()+unreal.Vector(origin.x,origin.y,origin.z))
    q=vec(q);return [sum((q[i]-chart['origin_ecef'][i])*chart[k][i] for i in range(3)) for k in ['east_axis','north_axis']]
def prop(obj,key):
    try:return str(obj.get_editor_property(key))
    except Exception:return None
ships=[];targets=[]
for a in actors:
    n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
    if n:
        anchor=a.get_component_by_class(unreal.CesiumGlobeAnchorComponent);g=anchor.resolve_georeference()
        c=a.get_component_by_class(unreal.ShipCombatComponent)
        hull=next((x for x in a.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM'),None)
        row=dict(label=a.get_actor_label(),path=a.get_path_name(),class_name=a.get_class().get_name(),world=vec(a.get_actor_location()),en_m=metric(a,g),
            yaw=a.get_actor_rotation().yaw,ai_enabled=prop(a,'bAIEnabled'),auto_ai=prop(a,'auto_possess_ai'),auto_player=prop(a,'auto_possess_player'),
            controller_class=prop(a,'ai_controller_class'),spatial=prop(a,'is_spatially_loaded'),new_navigation=n.new_navigation_enabled,
            mode=str(n.navigation_mode),role=str(n.role),start_b=n.start_at_patrol_b,offset_a=vec(n.patrol_offset_a),offset_b=vec(n.patrol_offset_b),
            cruise_mps=n.cruise_speed_mps,arrival_m=n.arrival_radius_m,start_delay=n.patrol_startup_delay_seconds,wait=n.patrol_wait_seconds,
            team=c.faction if c else None,combat=c.enable_combat if c else None,physics=hull.is_simulating_physics() if hull else None,
            mass_kg=hull.get_mass() if hull else None,linear_damping=hull.get_linear_damping() if hull else None,
            angular_damping=hull.get_angular_damping() if hull else None)
        for key,t in [('a',n.patrol_actor_a),('b',n.patrol_actor_b)]:
            row[key]=dict(label=t.get_actor_label(),path=t.get_path_name(),world=vec(t.get_actor_location()),en_m=metric(t,g)) if t else None
        ships.append(row)
    if isinstance(a,unreal.TargetPoint):targets.append(dict(label=a.get_actor_label(),path=a.get_path_name(),world=vec(a.get_actor_location())))
report=dict(level=ed.get_editor_world().get_path_name(),ships=ships,targets=targets,
    player_starts=[dict(label=a.get_actor_label(),world=vec(a.get_actor_location())) for a in actors if isinstance(a,unreal.PlayerStart)])
(root/'Docs/NavalCombat'/globals().get('OUTPUT','CURRENT_PATROL_AUDIT.json')).write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
