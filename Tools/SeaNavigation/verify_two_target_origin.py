"""Frame-sampled Cesium and Unreal origin changes in disposable, real PIE."""
import builtins
import json
import math
from pathlib import Path
import unreal

world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
enemy=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.load_class(None,"/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy_C"))[0]
nav=enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
geo=enemy.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
hull=next(c for c in enemy.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=="ShipSM")
vec=lambda p:[p.x,p.y,p.z]
def ecef(p):
    o=unreal.GameplayStatics.get_world_origin_location(world)
    return vec(geo.transform_unreal_position_to_earth_centered_earth_fixed(p+unreal.Vector(o.x,o.y,o.z)))
def sample():
    active=enemy.get_editor_property("DestinationActor")
    return dict(t=unreal.GameplayStatics.get_time_seconds(world),ship_ecef=ecef(hull.get_world_location()),
        A_ecef=ecef(nav.patrol_actor_a.get_actor_location()),B_ecef=ecef(nav.patrol_actor_b.get_actor_location()),
        route=[vec(p) for p in nav.current_route],goal=vec(nav.current_final_destination),
        endpoint_error_m=(nav.geographic_to_world(nav.current_final_destination)-active.get_actor_location()).length()/100,
        state=str(nav.navigation_state),throttle=nav.desired_throttle,rudder=nav.desired_rudder,
        speed_mps=hull.get_physics_linear_velocity().length()/100,
        origin=vec(unreal.GameplayStatics.get_world_origin_location(world)),geo_origin=vec(geo.get_origin_longitude_latitude_height()))
s=dict(handle=None,stage="cesium",frames=[],before=sample(),stage_frames=0)
builtins.two_target_origin_probe=s
assert s["before"]["route"]
def tick(dt):
    try:
        row=sample();row["stage"]=s["stage"];s["frames"].append(row);s["stage_frames"]+=1
        if s["stage_frames"]<15:return
        if s["stage"]=="cesium":
            s["stage"]="rebase";s["stage_frames"]=0;s["before_rebase"]=sample()
            unreal.GameplayStatics.set_world_origin_location(world,unreal.IntVector(50000,-25000,0))
            return
        before=s["before"];frames=s["frames"]
        s["passed"]=(all(r["route"]==before["route"] and r["goal"]==before["goal"] and r["endpoint_error_m"]<.01
            and math.dist(r["A_ecef"],before["A_ecef"])<.01 and math.dist(r["B_ecef"],before["B_ecef"])<.01 for r in frames)
            and frames[0]["geo_origin"]!=before["geo_origin"] and frames[-1]["origin"]==[50000,-25000,0]
            and all(any(r["throttle"]==0 and r["rudder"]==0 for r in frames if r["stage"]==stage) for stage in ["cesium","rebase"])
            and math.dist(frames[0]["ship_ecef"],before["ship_ecef"])<10
            and math.dist(frames[15]["ship_ecef"],s["before_rebase"]["ship_ecef"])<10)
        s["stage"]="complete"
    except Exception as error:s["passed"]=False;s["error"]=str(error);s["stage"]="complete"
    if s["stage"]=="complete":
        (Path(unreal.Paths.project_dir())/"Docs/ShipEnemyNavigation/TWO_TARGET_ORIGIN.json").write_text(json.dumps({k:v for k,v in s.items() if k!="handle"},indent=2),encoding="utf8")
        unreal.unregister_slate_post_tick_callback(s["handle"]);s["handle"]=None
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
s["handle"]=unreal.register_slate_post_tick_callback(tick)
origin=geo.get_origin_longitude_latitude_height()
geo.call_method("SetOriginLongitudeLatitudeHeight",(unreal.Vector(origin.x+.001,origin.y+.001,origin.z),))
print("Testing Cesium georeference setter, then Unreal world-origin rebase; disposable PIE only")
