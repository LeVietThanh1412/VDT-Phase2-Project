"""Real PIE telemetry until A -> B -> A or a bounded failure/deadline.

Execute in an isolated Python namespace through SpecialAgent. Does not relocate
the ship or either target, and does not save assets. Native code owns movement.
"""
import builtins
import json
import math
from pathlib import Path
import time
import unreal

world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world,"PIE is not running"
enemy=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.load_class(None,"/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy_C"))[0]
nav=enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
assert nav.navigation_mode==unreal.ShipNavigationMode.PATROL or globals().get("ALLOW_DESTINATION_FIXTURE",False)
target_a,target_b=nav.patrol_actor_a,nav.patrol_actor_b
assert target_a and target_b and target_a!=target_b
hull=next(c for c in enemy.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=="ShipSM")
heading=next(c for c in enemy.get_components_by_class(unreal.SceneComponent) if c.get_name()=="Arrow")
anchor=enemy.get_component_by_class(unreal.CesiumGlobeAnchorComponent)
geo=anchor.resolve_georeference()
root=Path(unreal.Paths.project_dir())
chart=json.loads((root/"Content/SeaNavigation/halong_navigation.json").read_text())
vec=lambda p:[p.x,p.y,p.z]
def ecef(p):
    origin=unreal.GameplayStatics.get_world_origin_location(world)
    return geo.transform_unreal_position_to_earth_centered_earth_fixed(p+unreal.Vector(origin.x,origin.y,origin.z))
def metric(p):
    q=vec(ecef(p));d=[q[i]-chart["origin_ecef"][i] for i in range(3)]
    return [sum(d[i]*chart[key][i] for i in range(3)) for key in ["east_axis","north_axis"]]
def sample():
    active=enemy.get_editor_property("DestinationActor")
    p=hull.get_world_location();m=metric(p)
    target=active.get_actor_location() if active else p
    route=[vec(q) for q in nav.current_route]
    return dict(t=unreal.GameplayStatics.get_time_seconds(world),world=vec(p),ecef=vec(ecef(p)),en_m=m,
        geographic=vec(nav.world_to_geographic(p)),velocity=vec(hull.get_physics_linear_velocity()),
        speed_mps=hull.get_physics_linear_velocity().length()/100,heading=vec(heading.get_forward_vector()),
        up=vec(hull.get_up_vector()),scan_distances_cm=list(enemy.get_editor_property("ScanDistances")),
        target="A" if active==target_a else "B" if active==target_b else "INVALID",
        target_actor=active.get_name() if active else None,target_world=vec(target),
        target_a_world=vec(target_a.get_actor_location()),target_b_world=vec(target_b.get_actor_location()),
        target_a_en=metric(target_a.get_actor_location()),target_b_en=metric(target_b.get_actor_location()),
        distance_m=math.dist(m,metric(target)),distance_a_m=math.dist(m,metric(target_a.get_actor_location())),
        distance_b_m=math.dist(m,metric(target_b.get_actor_location())),
        requested_llh=vec(nav.current_final_destination),requested_world=vec(nav.geographic_to_world(nav.current_final_destination)),
        requested_error_m=(nav.geographic_to_world(nav.current_final_destination)-target).length()/100,
        final_chart_error_m=math.dist(route[-1][:2],metric(target)) if route else None,
        state=str(nav.navigation_state),waypoint=nav.current_waypoint_index,completed_legs=nav.patrol_legs_completed,
        throttle=nav.desired_throttle,rudder=nav.desired_rudder,bp_throttle=enemy.get_editor_property("AIThrottle"),
        bp_rudder=enemy.get_editor_property("AIRudder"),desired_speed=nav.desired_speed,
        path_success=nav.pathfinding_succeeded,search_expansions=nav.last_search_expansions,
        search_ms=nav.last_search_milliseconds,recovery_attempts=nav.recovery_attempt_count,
        failure=nav.last_navigation_failure_reason,route=route)

previous=getattr(builtins,"two_target_pie_probe",None)
if previous and previous.get("handle"):unreal.unregister_slate_post_tick_callback(previous["handle"])
state=dict(status="RUNNING",rows=[],events=[],routes=[],handle=None,last=-1,started_wall=time.monotonic(),
    started_sim=unreal.GameplayStatics.get_time_seconds(world),deadline_sim=1800,deadline_wall=1200,
    initial_legs=nav.patrol_legs_completed,observed_route_generations=0,minimum_distance_m={"A":1.e99,"B":1.e99},
    time_dilation=2.0,failed_since=None,path=root/"Docs/ShipEnemyNavigation/TWO_TARGET_PIE.json")
builtins.two_target_pie_probe=state
unreal.GameplayStatics.set_global_time_dilation(world,state["time_dilation"])

def save():
    report={k:state[k] for k in ["status","rows","events","routes","initial_legs","observed_route_generations","minimum_distance_m","time_dilation"]}
    state["path"].write_text(json.dumps(report,indent=2),encoding="utf8")
def finish(reason):
    state["status"]=reason
    save()
    unreal.unregister_slate_post_tick_callback(state["handle"]);state["handle"]=None
    unreal.GameplayStatics.set_global_time_dilation(world,1.)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    try:
        assert enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)==nav,"Component reconstructed; run invalidated"
        now=unreal.GameplayStatics.get_time_seconds(world)
        if now-state["last"]<.5:return
        state["last"]=now
        row=sample();state["rows"].append(row)
        row["observed_route_generations"]=state["observed_route_generations"]
        for name in ["A","B"]:state["minimum_distance_m"][name]=min(state["minimum_distance_m"][name],row["distance_"+name.lower()+"_m"])
        if row["route"] and (not state["routes"] or row["route"]!=state["routes"][-1]["points"]):
            state["observed_route_generations"]+=1
            state["routes"].append(dict(t=now,target=row["target"],points=row["route"],generation=state["observed_route_generations"]))
        last_legs=state.get("last_legs",state["initial_legs"])
        if row["completed_legs"]>last_legs:
            state["events"].append(dict(t=now,completed_legs=row["completed_legs"],previous_target=state.get("last_target"),active_target=row["target"],distance_a_m=row["distance_a_m"],distance_b_m=row["distance_b_m"]))
        state["last_legs"]=row["completed_legs"];state["last_target"]=row["target"]
        failed=nav.navigation_state==unreal.ShipNavigationState.FAILED
        state["failed_since"]=(state["failed_since"] or now) if failed else None
        if row["completed_legs"]-state["initial_legs"]>=3:finish("COMPLETE_A_B_A")
        elif state["failed_since"] and now-state["failed_since"]>20:finish("NAVIGATION_FAILED")
        elif now-state["started_sim"]>state["deadline_sim"] or time.monotonic()-state["started_wall"]>state["deadline_wall"]:finish("BOUNDED_DEADLINE")
        elif len(state["rows"])%8==0:save()
    except Exception as e:
        state["events"].append(dict(error=str(e)));state["status"]="PROBE_ERROR";save()
        unreal.unregister_slate_post_tick_callback(state["handle"]);state["handle"]=None

state["handle"]=unreal.register_slate_post_tick_callback(tick)
print("PIE telemetry started; controller and Blueprint own movement")
