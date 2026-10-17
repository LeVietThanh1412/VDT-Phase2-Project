"""Temporarily move saved A to confirmed open water, or restore its full transform.

Execute with ACTION='setup' or 'restore', while PIE is stopped. Never spawns actors.
"""
import builtins
import base64
import json
import math
from pathlib import Path
import unreal

assert ACTION in ["setup","restore"]
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
a=next(x for x in actors if x.get_actor_label()=="TP_ShipEnemy_AStarDestination")
b=next(x for x in actors if x.get_actor_label()=="TP_ShipEnemy_OpenSeaDestination")
path=Path(unreal.Paths.project_dir())/"Docs/ShipEnemyNavigation/TWO_TARGET_RELOCATION.json"
vec=lambda p:[p.x,p.y,p.z]
def transform(actor):
    r=actor.get_actor_rotation()
    return dict(world=vec(actor.get_actor_location()),rotation=[r.pitch,r.yaw,r.roll],scale=vec(actor.get_actor_scale3d()))
if ACTION=="setup":
    assert not getattr(builtins,"two_target_relocation_original",None),"Restore the existing relocation first"
    builtins.two_target_relocation_original=a.get_actor_transform()
    report=dict(original_A=transform(a),original_B=transform(b),status="TEMPORARY_RELOCATION_SAVED")
    # The fixed chart east axis displaces B by 300m into verified accepted water.
    chart=json.loads((Path(unreal.Paths.project_dir())/"Content/SeaNavigation/halong_navigation.json").read_text())
    geo=a.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
    o=unreal.GameplayStatics.get_world_origin_location(editor.get_editor_world());offset=unreal.Vector(o.x,o.y,o.z)
    p=geo.transform_unreal_position_to_earth_centered_earth_fixed(b.get_actor_location()+offset)
    axis=unreal.Vector(*chart["east_axis"])
    candidate=p+axis*300
    delta=[candidate.to_tuple()[i]-chart["origin_ecef"][i] for i in range(3)]
    en=[sum(delta[i]*chart[key][i] for i in range(3)) for key in ["east_axis","north_axis"]]
    x=math.floor((en[0]-chart["min_east"])/chart["cell_size"]);y=math.floor((en[1]-chart["min_north"])/chart["cell_size"])
    cells=base64.b64decode(chart["cells_base64"])
    assert 0<=x<chart["width"] and 0<=y<chart["height"] and cells[y*chart["width"]+x],"Test position is not accepted water"
    report["temporary_A_en_m"]=en;report["temporary_A_cell_accepted"]=True
    new_world=geo.transform_earth_centered_earth_fixed_position_to_unreal(candidate)-offset
    a.modify();a.set_actor_location(new_world,False,True)
    report["temporary_A"]=transform(a)
else:
    report=json.loads(path.read_text())
    original=getattr(builtins,"two_target_relocation_original",None)
    if original is None:
        t=report["original_A"]
        original=unreal.Transform(location=unreal.Vector(*t["world"]),rotation=unreal.Rotator(pitch=t["rotation"][0],yaw=t["rotation"][1],roll=t["rotation"][2]),scale=unreal.Vector(*t["scale"]))
    a.modify();a.set_actor_transform(original,False,True)
    # Cesium adjusts orientation when the geographic position changes. Restore
    # the exact saved orientation after that move, at the now-fixed position.
    saved_rotation=report["original_A"]["rotation"]
    a.set_actor_rotation(unreal.Rotator(pitch=saved_rotation[0],yaw=saved_rotation[1],roll=saved_rotation[2]),True)
    a.set_actor_scale3d(unreal.Vector(*report["original_A"]["scale"]))
    report["restored_A"]=transform(a);report["restored_B"]=transform(b)
    assert (a.get_actor_location()-unreal.Vector(*report["original_A"]["world"])).length()<.01
    assert all(abs(x-y)<1.e-7 for key in ["rotation","scale"] for x,y in zip(report["restored_A"][key],report["original_A"][key]))
    assert transform(b)==report["original_B"]
    report["status"]="ORIGINAL_TRANSFORM_RESTORED_AND_SAVED"
    builtins.two_target_relocation_original=None
assert unreal.EditorLoadingAndSavingUtils.save_packages([a.get_package()],False)
path.write_text(json.dumps(report,indent=2),encoding="utf8")
print(json.dumps(report))
