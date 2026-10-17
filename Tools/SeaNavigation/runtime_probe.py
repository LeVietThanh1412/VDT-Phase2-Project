# Read-only PIE telemetry. Run through the project's MCP client after starting PIE.
import builtins
import json
from pathlib import Path
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, "PIE must be running"
enemy_class = unreal.load_class(None, "/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy_C")
enemy = unreal.GameplayStatics.get_all_actors_of_class(world, enemy_class)[0]
nav = enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
hull = next(c for c in enemy.get_components_by_class(unreal.PrimitiveComponent) if c.get_name() == "ShipSM")
anchor = enemy.get_component_by_class(unreal.CesiumGlobeAnchorComponent)
path = Path(unreal.Paths.project_dir().replace("\\", "/")) / "Docs/ShipEnemyNavigation/RUNTIME_TELEMETRY.json"
previous = getattr(builtins, "ship_navigation_probe", None)
if previous and previous.get("handle"):
    unreal.unregister_slate_post_tick_callback(previous["handle"])
state = dict(world=world, enemy=enemy, nav=nav, hull=hull, anchor=anchor,
             rows=[], routes=[], events=[], path=path, last=-1, handle=None,
             deadline=unreal.GameplayStatics.get_time_seconds(world)+600, arrived_since=None)
builtins.ship_navigation_probe = state

def sample(delta, state=state, unreal=unreal, json=json):
    try:
        w, a, c, h = state["world"], state["enemy"], state["nav"], state["hull"]
        if a.get_component_by_class(unreal.ShipEnemyNavigationComponent) != c or h not in a.get_components_by_class(unreal.PrimitiveComponent):
            raise RuntimeError("Component reconstruction invalidated this probe; discard the run")
        now = unreal.GameplayStatics.get_time_seconds(w)
        if now - state["last"] < 0.5:
            return
        state["last"] = now
        vector = lambda v: [v.x, v.y, v.z]
        rotation = h.get_world_rotation()
        row = dict(t=now, position=vector(h.get_world_location()),
                   geographic=vector(c.world_to_geographic(h.get_world_location())),
                   ecef=vector(state["anchor"].get_earth_centered_earth_fixed_position()),
                   velocity=vector(h.get_physics_linear_velocity()),
                   rotation=[rotation.pitch, rotation.yaw, rotation.roll],
                   state=str(c.get_editor_property("navigation_state")),
                   waypoint=c.get_editor_property("current_waypoint_index"),
                   speed=c.get_editor_property("desired_speed"),
                   throttle=c.get_editor_property("desired_throttle"),
                   rudder=c.get_editor_property("desired_rudder"),
                   bp_throttle=a.get_editor_property("AIThrottle"),
                   bp_rudder=a.get_editor_property("AIRudder"),
                   success=c.get_editor_property("pathfinding_succeeded"),
                   failure=c.get_editor_property("last_navigation_failure_reason"),
                   uncertain=c.get_editor_property("collision_data_uncertain"),
                   recoveries=c.get_editor_property("recovery_attempt_count"),
                   heading=vector(next(x for x in a.get_components_by_class(unreal.SceneComponent) if x.get_name() == "Arrow").get_forward_vector()),
                   sensor_distances=list(a.get_editor_property("ScanDistances")),
                   destination=vector(c.get_editor_property("current_final_destination")))
        state["rows"].append(row)
        route = [vector(v) for v in c.get_editor_property("current_route")]
        if not state["routes"] or route != state["routes"][-1]["points"]:
            state["routes"].append(dict(t=now, points=route))
        if len(state["rows"]) % 4 == 0:
            try:
                state["path"].write_text(json.dumps({k: state[k] for k in ["rows", "routes", "events"]}, indent=2))
            except OSError as error:
                # A transient artifact-write failure must not stop the runtime probe.
                state["events"].append(dict(artifact_write_error=str(error), t=now))
        speed = sum(v*v for v in row["velocity"][:2]) ** 0.5 / 100
        arrived = c.navigation_state == unreal.ShipNavigationState.ARRIVED and speed < 0.5
        state["arrived_since"] = (state["arrived_since"] or now) if arrived else None
        reason = None
        if state["arrived_since"] and now-state["arrived_since"] >= 5:
            reason = "Arrived and physically settled for five seconds"
        elif c.navigation_state == unreal.ShipNavigationState.FAILED:
            reason = "Navigation failed: " + row["failure"]
        elif now >= state["deadline"]:
            reason = "Bounded runtime deadline reached"
        if reason:
            state["events"].append(dict(t=now, end_reason=reason))
            state["path"].write_text(json.dumps({k: state[k] for k in ["rows", "routes", "events"]}, indent=2))
            unreal.unregister_slate_post_tick_callback(state["handle"])
            state["handle"] = None
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    except Exception as error:
        state["events"].append(dict(error=str(error)))
        unreal.unregister_slate_post_tick_callback(state["handle"])
        state["handle"] = None

state["handle"] = unreal.register_slate_post_tick_callback(sample)
print("Runtime telemetry active", str(path))
