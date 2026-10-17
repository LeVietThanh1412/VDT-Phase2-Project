"""Move A by 2m inside its accepted cell in disposable PIE; verify fresh goal."""
import builtins
import json
from pathlib import Path
import unreal

world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
enemy=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.load_class(None,"/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy_C"))[0]
nav=enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
target=nav.patrol_actor_a
vec=lambda p:[p.x,p.y,p.z]
old_goal=nav.current_final_destination
old_route=[vec(p) for p in nav.current_route]
original=target.get_actor_location()
s=dict(handle=None,before_world=vec(original),before_goal=vec(old_goal),before_route=old_route,
    started=unreal.GameplayStatics.get_time_seconds(world))
builtins.two_target_live_move=s
target.set_actor_location(original+unreal.Vector(200,0,0),False,True)
def tick(dt):
    if unreal.GameplayStatics.get_time_seconds(world)-s["started"]<4:return
    try:
        assert enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)==nav
        requested=nav.geographic_to_world(nav.current_final_destination)
        s.update(after_world=vec(target.get_actor_location()),after_goal=vec(nav.current_final_destination),
            after_route=[vec(p) for p in nav.current_route],endpoint_error_m=(requested-target.get_actor_location()).length()/100,
            state=str(nav.navigation_state),failure=nav.last_navigation_failure_reason)
        # Unreal property wrappers can view live storage; compare the serialized
        # before/after values, not a retained mutable FVector wrapper.
        s["passed"]=(s["after_goal"]!=s["before_goal"] and s["after_route"]!=old_route and nav.pathfinding_succeeded
            and s["endpoint_error_m"]<.01 and nav.navigation_state!=unreal.ShipNavigationState.FAILED)
    except Exception as error:s["passed"]=False;s["error"]=str(error)
    (Path(unreal.Paths.project_dir())/"Docs/ShipEnemyNavigation/TWO_TARGET_LIVE_TARGET_MOVE.json").write_text(json.dumps({k:v for k,v in s.items() if k!="handle"},indent=2),encoding="utf8")
    unreal.unregister_slate_post_tick_callback(s["handle"]);s["handle"]=None
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
s["handle"]=unreal.register_slate_post_tick_callback(tick)
print("PIE-only live A move requested; no editor transform or asset is modified")
