"""Bounded live PIE checks. Mutates only disposable PIE actors; never saves them."""
import builtins
import json
import math
from pathlib import Path
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, "Start PIE first"
enemy = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.load_class(
    None, "/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy_C"))[0]
nav = enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
target = enemy.get_editor_property("DestinationActor")
anchor = enemy.get_component_by_class(unreal.CesiumGlobeAnchorComponent)
target_anchor = target.get_component_by_class(unreal.CesiumGlobeAnchorComponent)
geo = anchor.resolve_georeference()
hull = next(c for c in enemy.get_components_by_class(unreal.PrimitiveComponent) if c.get_name() == "ShipSM")
root = Path(unreal.Paths.project_dir())
chart = json.loads((root / "Content/SeaNavigation/halong_navigation.json").read_text())
defaults = json.loads((root / "Data/SeaNavigation/patrol_defaults.json").read_text())
vec = lambda p: [p.x, p.y, p.z]
route = lambda: [vec(p) for p in nav.current_route]

def metric(p):
    origin = unreal.GameplayStatics.get_world_origin_location(world)
    ecef = geo.transform_unreal_position_to_earth_centered_earth_fixed(p + unreal.Vector(origin.x, origin.y, origin.z))
    d = [ecef.x-chart["origin_ecef"][0], ecef.y-chart["origin_ecef"][1], ecef.z-chart["origin_ecef"][2]]
    return [sum(d[i]*chart[axis][i] for i in range(3)) for axis in ["east_axis", "north_axis"]]

def snapshot():
    requested = nav.geographic_to_world(nav.current_final_destination)
    points = route()
    m = metric(target.get_actor_location())
    return dict(t=unreal.GameplayStatics.get_time_seconds(world), state=str(nav.navigation_state),
                success=nav.pathfinding_succeeded, failure=nav.last_navigation_failure_reason,
                actor=target.get_name(), actor_world=vec(target.get_actor_location()),
                requested_world=vec(requested), requested_llh=vec(nav.current_final_destination),
                actor_requested_m=(requested-target.get_actor_location()).length()/100,
                final_chart_error_m=math.dist(points[-1][:2], m) if points else None,
                target_ecef=vec(target_anchor.get_earth_centered_earth_fixed_position()),
                ship_ecef=vec(geo.transform_unreal_position_to_earth_centered_earth_fixed(hull.get_world_location()+unreal.Vector(*vec(unreal.GameplayStatics.get_world_origin_location(world))))),
                ship_target_m=math.dist(metric(hull.get_world_location()), m),
                speed_mps=hull.get_physics_linear_velocity().length()/100,
                throttle=nav.desired_throttle, rudder=nav.desired_rudder,
                bp_throttle=enemy.get_editor_property("AIThrottle"), bp_rudder=enemy.get_editor_property("AIRudder"),
                waypoint=nav.current_waypoint_index, route=points)

s = dict(stage="initial", started=unreal.GameplayStatics.get_time_seconds(world), since=0,
         checks={}, samples=[], events=[], handle=None, original_target=target.get_actor_location(),
         deadline=unreal.GameplayStatics.get_time_seconds(world)+240)
builtins.target_mismatch_verification = s
s["samples"].append(snapshot())
s["blocked_llh"]=json.loads((root/"Docs/ShipEnemyNavigation/TARGET_MISMATCH_BEFORE.json").read_text())["target_llh"]

def tick(dt):
    try:
        assert enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent) == nav, "Component reconstructed; discard run"
        now = unreal.GameplayStatics.get_time_seconds(world)
        if now-s.get("last", -1) < .25:
            return
        s["last"] = now
        row = snapshot()
        s["samples"].append(row)
        s["checks"]["I_commands_match_blueprint"] = s["checks"].get("I_commands_match_blueprint", True) and (
            row["throttle"] == row["bp_throttle"] and row["rudder"] == row["bp_rudder"])
        good = row["success"] and row["actor_requested_m"] < 1.e-4 and row["final_chart_error_m"] < 1.e-4
        stage = s["stage"]
        if stage == "initial" and good:
            s["checks"]["open_sea_target_uses_direct_two_point_route"]=len(route())==2
            s["events"].append(dict(stage=stage,sample=row))
            target.set_actor_location(nav.geographic_to_world(unreal.Vector(*defaults["patrol_b_llh"])),False,True)
            s["stage"]="water"
        elif stage == "water" and good:
            s["checks"].update(A_live_actor_matches_request=True, C_exact_final_waypoint=True)
            s["events"].append(dict(stage=stage, sample=row))
            s["stable_route"] = route();s["since"] = now;s["stage"] = "stationary"
        elif stage == "stationary" and now-s["since"] > .75:
            s["checks"]["D_stationary_route_reused"] = route() == s["stable_route"]
            target.set_actor_location(target.get_actor_location()+unreal.Vector(1000,0,0), False, True)
            s["stage"] = "moved"
        elif stage == "moved" and good and route() != s["stable_route"]:
            s["checks"]["D_moving_actor_replanned"] = True
            s["events"].append(dict(stage=stage, sample=row))
            target.set_actor_location(nav.geographic_to_world(unreal.Vector(*s["blocked_llh"])), False, True);s["stage"] = "blocked"
        elif stage == "blocked" and nav.navigation_state == unreal.ShipNavigationState.FAILED:
            s["checks"]["E_moving_into_exclusion_stops"] = not row["route"] and row["throttle"] == 0 and row["rudder"] == 0
            target.set_actor_location(nav.geographic_to_world(unreal.Vector(*defaults["patrol_b_llh"])), False, True)
            s["stage"] = "recover"
        elif stage == "recover" and good:
            s["checks"]["D_recovers_after_target_returns_to_water"] = True
            s["origin_route"] = route();s["target_ecef"] = row["target_ecef"]
            s["geo_origin"] = geo.get_origin_longitude_latitude_height()
            geo.set_origin_longitude_latitude_height(s["geo_origin"]+unreal.Vector(.005,.005,0))
            s["since"] = now;s["stage"] = "georeference"
        elif stage == "georeference" and now-s["since"] > 1:
            s["checks"]["H_cesium_shift_retains_route_and_alignment"] = good and route() == s["origin_route"] and math.dist(row["target_ecef"],s["target_ecef"]) < .01
            s["events"].append(dict(stage=stage, sample=row))
            unreal.GameplayStatics.set_world_origin_location(world, unreal.IntVector(50000,-25000,0))
            s["since"] = now;s["stage"] = "rebase"
        elif stage == "rebase" and now-s["since"] > 1:
            s["checks"]["H_unreal_rebase_retains_route_and_alignment"] = good and route() == s["origin_route"] and math.dist(row["target_ecef"],s["target_ecef"]) < .01
            s["events"].append(dict(stage=stage, sample=row))
            unreal.GameplayStatics.set_world_origin_location(world, unreal.IntVector(0,0,0))
            geo.set_origin_longitude_latitude_height(s["geo_origin"])
            s["since"]=now;s["stage"]="restore"
        elif stage == "restore" and now-s["since"] > 1:
            # A short physical approach ahead in accepted water, after the
            # asynchronous world rebase has finished. Move only the test target.
            heading=next(c for c in enemy.get_components_by_class(unreal.SceneComponent) if c.get_name()=="Arrow").get_forward_vector()
            point=hull.get_world_location()+heading*12500
            point.z=target.get_actor_location().z
            target.set_actor_location(point,False,True)
            s["physical_start"] = row["ship_ecef"];s["physical_start_time"] = now
            s["stage"] = "physical"
        elif stage == "physical":
            s["checks"]["I_physics_displacement_m"] = math.dist(row["ship_ecef"],s["physical_start"])
            if nav.navigation_state == unreal.ShipNavigationState.ARRIVED and row["ship_target_m"] <= nav.arrival_radius_m and row["speed_mps"] < .5:
                s["settled_since"] = s.get("settled_since",now)
                if now-s["settled_since"] > 3:
                    s["checks"]["physical_arrival_within_tolerance"] = True;s["stage"]="complete"
            else:
                s.pop("settled_since",None)
        if now >= s["deadline"]:
            s["events"].append(dict(timeout_stage=s["stage"]));s["stage"]="complete"
        if len(s["samples"]) % 4 == 0 or s["stage"] == "complete":
            report={k:s[k] for k in ["stage","checks","samples","events"]}
            (root/"Docs/ShipEnemyNavigation/TARGET_MISMATCH_RUNTIME.json").write_text(json.dumps(report,indent=2))
        if s["stage"] == "complete":
            unreal.unregister_slate_post_tick_callback(s["handle"]);s["handle"]=None
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    except Exception as e:
        s["events"].append(dict(error=str(e)));s["stage"]="error"
        (root/"Docs/ShipEnemyNavigation/TARGET_MISMATCH_RUNTIME.json").write_text(json.dumps({k:s[k] for k in ["stage","checks","samples","events"]},indent=2))
        unreal.unregister_slate_post_tick_callback(s["handle"]);s["handle"]=None

s["handle"]=unreal.register_slate_post_tick_callback(tick)
print("Target mismatch runtime verification started (240 s simulation deadline)")
