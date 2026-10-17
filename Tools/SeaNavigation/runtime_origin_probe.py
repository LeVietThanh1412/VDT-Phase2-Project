# Run during the bounded PIE probe. Changes only the disposable PIE world.
import builtins
import json
import unreal

s = builtins.ship_navigation_probe
assert s["handle"], "Runtime probe must be active"
a, c, w = s["enemy"], s["nav"], s["world"]
geo = s["anchor"].resolve_georeference()
target = a.get_editor_property("DestinationActor")
target_anchor = target.get_component_by_class(unreal.CesiumGlobeAnchorComponent)
assert target_anchor, "Saved destination marker requires a globe anchor"
shift = a.call_method("AddComponentByClass", (unreal.CesiumOriginShiftComponent.static_class(), False, unreal.Transform(), False))
shift.call_method("SetMode", (unreal.CesiumOriginShiftMode.DISABLED,))
vector = lambda v: [v.x, v.y, v.z]
state = dict(probe=s, geo=geo, shift=shift, target_anchor=target_anchor, stage="cesium",
             started=unreal.GameplayStatics.get_time_seconds(w), frames=[], handle=None,
             origin_before=vector(geo.get_origin_earth_centered_earth_fixed()),
             ship_before=vector(s["anchor"].get_earth_centered_earth_fixed_position()),
             target_before=vector(target_anchor.get_earth_centered_earth_fixed_position()),
             route_before=[vector(v) for v in c.current_route], goal_before=vector(c.current_final_destination))
builtins.ship_navigation_origin_probe = state

def tick(delta, state=state, unreal=unreal, json=json):
    try:
        s, geo, shift = state["probe"], state["geo"], state["shift"]
        c, w = s["nav"], s["world"]
        vec = lambda v: [v.x, v.y, v.z]
        distance = lambda a, b: sum((x-y)**2 for x,y in zip(a,b))**0.5
        now = unreal.GameplayStatics.get_time_seconds(w)
        origin = vec(geo.get_origin_earth_centered_earth_fixed())
        if state["stage"] == "cesium" and distance(origin, state["origin_before"]) > 1:
            shift.call_method("SetMode", (unreal.CesiumOriginShiftMode.DISABLED,))
            state["cesium_changed_at"] = now
            state["cesium_origin_after"] = origin
            state["cesium_ship_delta_m"] = distance(vec(s["anchor"].get_earth_centered_earth_fixed_position()), state["ship_before"])
            state["stage"] = "cesium_frames"
        if state["stage"] in ["cesium_frames", "rebase_frames"]:
            state["frames"].append(dict(t=now, stage=state["stage"], throttle=c.desired_throttle,
                rudder=c.desired_rudder, route_retained=[vec(v) for v in c.current_route] == state["route_before"],
                goal_retained=vec(c.current_final_destination) == state["goal_before"],
                target_delta_m=distance(vec(state["target_anchor"].get_earth_centered_earth_fixed_position()), state["target_before"]),
                state=str(c.navigation_state)))
            count = sum(f["stage"] == state["stage"] for f in state["frames"])
            if count >= 12 and state["stage"] == "cesium_frames":
                state["rebase_before"] = vec(unreal.GameplayStatics.get_world_origin_location(w))
                state["rebase_ship_before"] = vec(s["anchor"].get_earth_centered_earth_fixed_position())
                state["stage"] = "rebase"
                unreal.GameplayStatics.set_world_origin_location(w, unreal.IntVector(50000,-25000,0))
            elif count >= 12 and state["stage"] == "rebase_frames":
                state["passed"] = (all(f["route_retained"] and f["goal_retained"] and f["target_delta_m"] < 0.01 for f in state["frames"])
                    and state["cesium_ship_delta_m"] < 10 and state["rebase_ship_delta_m"] < 10
                    and any(f["throttle"] == 0 and f["rudder"] == 0 for f in state["frames"] if f["stage"] == "cesium_frames")
                    and any(f["throttle"] == 0 and f["rudder"] == 0 for f in state["frames"] if f["stage"] == "rebase_frames"))
                state["stage"] = "complete"
        if state["stage"] == "rebase" and vec(unreal.GameplayStatics.get_world_origin_location(w)) != state["rebase_before"]:
            state["rebase_after"] = vec(unreal.GameplayStatics.get_world_origin_location(w))
            state["rebase_ship_delta_m"] = distance(vec(s["anchor"].get_earth_centered_earth_fixed_position()), state["rebase_ship_before"])
            state["stage"] = "rebase_frames"
        if now-state["started"] > 10 and state["stage"] != "complete":
            state["passed"] = False
            state["error"] = "Origin probe timed out"
            state["stage"] = "complete"
        if state["stage"] == "complete":
            shift.call_method("SetMode", (unreal.CesiumOriginShiftMode.DISABLED,))
            report = {k:v for k,v in state.items() if k not in ["probe","geo","shift","target_anchor","handle"]}
            s["events"].append(dict(runtime_origin_test=report))
            s["path"].with_name("RUNTIME_ORIGIN_RESULTS.json").write_text(json.dumps(report,indent=2))
            unreal.unregister_slate_post_tick_callback(state["handle"])
            state["handle"] = None
    except Exception as error:
        state["error"] = str(error)
        shift.call_method("SetMode", (unreal.CesiumOriginShiftMode.DISABLED,))
        unreal.unregister_slate_post_tick_callback(state["handle"])
        state["handle"] = None

state["handle"] = unreal.register_slate_post_tick_callback(tick)
shift.call_method("SetDistance", (1000.0,))
shift.call_method("SetMode", (unreal.CesiumOriginShiftMode.CHANGE_CESIUM_GEOREFERENCE,))
print("Real runtime CesiumOriginShift followed by Unreal world-origin rebasing requested")
