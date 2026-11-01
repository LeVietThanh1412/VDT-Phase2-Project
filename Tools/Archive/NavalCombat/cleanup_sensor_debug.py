"""Back up, mute only unlinked diagnostic defaults, compile, validate and save.

No trace execution/data pin links or actor/nav settings are changed.
"""
import datetime,hashlib,json,shutil
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
backup=root/'Saved'/('SensorDebugRecovery_'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d_%H%M%S'));backup.mkdir(parents=True)
paths=['Ship/BluePrint/BP_Ship','ShipEnemy/BluePrint/BP_ShipEnemy']
before={}
for path in paths:
    p=root/'Content'/(path+'.uasset');shutil.copy2(p,backup/p.name);before[path]=hashlib.sha256(p.read_bytes()).hexdigest()
result=unreal.ShipCombatLibrary.disable_sensor_debug_defaults();assert result.startswith('OK:'),result
compile_result=unreal.ShipEnemyNavigationComponent.compile_battle_blueprints();assert compile_result.startswith('OK:'),compile_result
validation=unreal.ShipEnemyNavigationComponent.validate_installed_blueprint();assert validation.startswith('OK:'),validation
for path in paths:assert unreal.EditorAssetLibrary.save_asset('/Game/'+path,False)
report=dict(result=result,compile=compile_result,force_graph=validation,backup=str(backup),before=before,after={p:hashlib.sha256((root/'Content'/(p+'.uasset')).read_bytes()).hexdigest() for p in paths})
(root/'Docs/NavalCombat/SENSOR_DEBUG_CLEANUP.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report))
