"""Replay a recorded approach in disposable PIE to identify an avoidance blocker.

Fixture setup is explicit and cannot establish full-loop physical acceptance.
"""
import unreal
import json
import math
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
root=Path(unreal.Paths.project_dir())
enemy=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.load_class(None,"/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy_C"))[0]
nav=enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
b=nav.patrol_actor_b
enemy.set_editor_property("DestinationActor",b)
nav=enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
nav.stop_navigation()
hull=next(x for x in enemy.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=="ShipSM")
run=json.loads((root/"Docs/ShipEnemyNavigation/TWO_TARGET_RETURN_APPROACH_FAILURE.json").read_text())
row=next(x for x in run["rows"] if x["t"]>=850)
direction=unreal.Vector(*row["heading"])
enemy.set_actor_location_and_rotation(unreal.Vector(*row["world"]),unreal.MathLibrary.make_rot_from_x(direction),False,True)
hull.set_physics_linear_velocity(unreal.Vector(*row["velocity"]))
hull.set_physics_angular_velocity_in_degrees(unreal.Vector())
nav.update_navigation(.016)
nav.set_geographic_destination(nav.world_to_geographic(b.get_actor_location()))
ALLOW_DESTINATION_FIXTURE=True
exec(compile((root/"Tools/SeaNavigation/verify_two_target_pie.py").read_text(),"verify_two_target_pie.py","exec"),globals())
state["path"]=root/"Docs/ShipEnemyNavigation/TWO_TARGET_OBSTACLE_DIAGNOSTIC.json"
state["deadline_sim"]=300;state["deadline_wall"]=240
state["events"].append(dict(test_fixture="Replay of measured approach at t=850s, explicit B command; not full-cycle evidence",setup=row))
print("Diagnostic return replay started; inspecting actual hull-sweep hit actors")
