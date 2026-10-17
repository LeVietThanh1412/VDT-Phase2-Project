"""Read-only final saved-state audit, run outside PIE after restoration/restart."""
import hashlib
import json
import math
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
labels=["TP_ShipEnemy_AStarDestination","TP_ShipEnemy_OpenSeaDestination"]
targets=[next(a for a in actors if a.get_actor_label()==label) for label in labels]
enemy=next(a for a in actors if a.get_class().get_name()=="BP_ShipEnemy_C")
nav=enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
original=json.loads((root/"Docs/ShipEnemyNavigation/TWO_TARGET_EDITOR_ORIGINAL.json").read_text())
vec=lambda p:[p.x,p.y,p.z]
transforms={}
for a in targets+[enemy]:
    r=a.get_actor_rotation()
    current=dict(world=vec(a.get_actor_location()),rotation=[r.pitch,r.yaw,r.roll],scale=vec(a.get_actor_scale3d()))
    expected=original[a.get_actor_label()]
    for key,tolerance in [("world",.01),("rotation",1.e-6),("scale",1.e-7)]:
        assert all(abs(x-y)<tolerance for x,y in zip(current[key],expected[key])),a.get_actor_label()+" changed "+key
    transforms[a.get_actor_label()]=current
assert nav.navigation_mode==unreal.ShipNavigationMode.PATROL and nav.new_navigation_enabled
assert nav.patrol_actor_a==targets[0] and nav.patrol_actor_b==targets[1]
assert enemy.get_editor_property("DestinationActor")==targets[0]
assert all(not a.get_editor_property("is_spatially_loaded") for a in targets+[enemy])
points=[a for a in actors if isinstance(a,unreal.TargetPoint)]
assert len(points)==2
validation=unreal.ShipEnemyNavigationComponent.validate_installed_blueprint()
assert validation.startswith("OK:"),validation
bp=unreal.load_asset("/Game/ShipEnemy/BluePrint/BP_ShipEnemy")
dirty={p.get_name() for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())+list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())}
assert not any(p.get_name() in dirty for p in [bp.get_package(),enemy.get_package()]+[a.get_package() for a in targets]),"Required navigation asset is unsaved"
startup_path=root/"Docs/ShipEnemyNavigation/TWO_TARGET_PERSISTENCE_PIE.json"
startup=json.loads(startup_path.read_text()) if startup_path.exists() else {}
fresh_rows=startup.get("rows",[])
assert fresh_rows and fresh_rows[0]["target"]=="A" and fresh_rows[0]["route"],"Missing fresh startup evidence"
assert all(not r["failure"] for r in fresh_rows)
assert max(r["speed_mps"] for r in fresh_rows)>.5
assert any(abs(r["throttle"])>.01 for r in fresh_rows)
assert math.dist(fresh_rows[0]["en_m"],fresh_rows[-1]["en_m"])>10,"Fresh startup has no measured horizontal progress"
native=json.loads((root/"Saved/TwoTargetFinalAutomation/index.json").read_text(encoding="utf-8-sig"))
assert native["succeeded"]==4 and native["failed"]==0
builds={}
for name in ["EDITOR","GAME"]:
    text=(root/f"Docs/ShipEnemyNavigation/TWO_TARGET_FINAL_BUILD_{name}.log").read_text(encoding="utf-8-sig")
    assert "Result: Succeeded" in text
    builds[name]="PASS"
player=root/"Content/Ship/BluePrint/BP_Ship.uasset"
digest=hashlib.sha256(player.read_bytes()).hexdigest()
assert digest=="28e992c9b100958388f584931b69f08d733b492fa33653ec5c165b2d3990b64a"
report=dict(status="PASS",level=editor.get_editor_world().get_name(),blueprint_validation=validation,
    mode=str(nav.navigation_mode),enabled=nav.new_navigation_enabled,target_count=len(points),
    A=targets[0].get_path_name(),B=targets[1].get_path_name(),transforms=transforms,
    original_transforms_preserved=True,required_assets_saved=True,builds=builds,
    native_tests={k:native[k] for k in ["succeeded","failed","notRun"]},
    fresh_editor_startup_samples=len(fresh_rows),fresh_startup_status=startup.get("status"),
    player_sha256=digest,loaded_modules=json.loads((root/"Binaries/Win64/UnrealEditor.modules").read_text()))
(root/"Docs/ShipEnemyNavigation/TWO_TARGET_FINAL_STATE.json").write_text(json.dumps(report,indent=2),encoding="utf8")
print(json.dumps(report))
