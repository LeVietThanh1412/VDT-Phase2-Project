"""Disposable physical turnaround fixture, explicitly not full-cycle acceptance.

Starts the hull at a previously observed arrival to diagnose turning quickly.
No editor-world transform or asset is changed. Full acceptance starts separately
from the original saved ship transform and must complete all three real legs.
"""
import unreal
import json
import math
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
enemy=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.load_class(None,"/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy_C"))[0]
nav=enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
hull=next(x for x in enemy.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=="ShipSM")
root=Path(unreal.Paths.project_dir())
run=json.loads((root/"Docs/ShipEnemyNavigation/TWO_TARGET_SLOW_TURN_FAILURE.json").read_text())
row=next(x for x in reversed(run["rows"]) if x["completed_legs"]==0)
yaw=math.degrees(math.atan2(row["heading"][1],row["heading"][0]))
nav.stop_navigation()
position=unreal.Vector(*row["world"])
target=nav.patrol_actor_a.get_actor_location()
dx,dy=target.x-position.x,target.y-position.y
# Place this diagnostic fixture 20m inside the observed acceptance boundary,
# so buoyancy settling cannot keep it just outside arrival by a few centimetres.
length=math.hypot(dx,dy);position+=unreal.Vector(dx/length*2000,dy/length*2000,0)
enemy.set_actor_location_and_rotation(position,unreal.MathLibrary.make_rot_from_x(unreal.Vector(*row["heading"])),False,True)
hull.set_physics_linear_velocity(unreal.Vector());hull.set_physics_angular_velocity_in_degrees(unreal.Vector())
nav.start_patrol()
source=(root/"Tools/SeaNavigation/verify_two_target_pie.py").read_text()
exec(compile(source,"verify_two_target_pie.py","exec"),globals())
state["path"]=root/"Docs/ShipEnemyNavigation/TWO_TARGET_FOCUSED_TURN.json"
state["deadline_sim"]=180;state["deadline_wall"]=180
state["events"].append(dict(test_fixture="Hull placed 20m inside previously observed A arrival; this run cannot establish full-cycle acceptance",setup_world=vec(position),setup_yaw=yaw))
print("Focused physical turn test started; disposable PIE fixture, saved level untouched")
