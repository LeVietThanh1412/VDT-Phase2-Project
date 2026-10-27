"""Compare the saved fleet with the accepted QuangNinh layout after restart."""
import contextlib, io, json, math
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
p=root/'Tools/NavalCombat/audit_current_patrol.py'
with contextlib.redirect_stdout(io.StringIO()):
    exec(compile(p.read_text(),str(p),'exec'),{'OUTPUT':'QUANGNINH_CACHE_SAVED_AUDIT.json'})
before=json.loads((root/'Docs/NavalCombat/QUANGNINH_SAVED_PATROL_AUDIT.json').read_text())
after=json.loads((root/'Docs/NavalCombat/QUANGNINH_CACHE_SAVED_AUDIT.json').read_text())

def same(a,b):
    if isinstance(a,dict):return a.keys()==b.keys() and all(same(a[k],b[k]) for k in a)
    if isinstance(a,list):return len(a)==len(b) and all(same(x,y) for x,y in zip(a,b))
    if isinstance(a,float):return math.isclose(a,b,rel_tol=0,abs_tol=1e-6)
    return a==b

differences=[]
for old in before['ships']:
    new=next(a for a in after['ships'] if a['label']==old['label'])
    for key in ['world','team','a','b','offset_a','offset_b','cruise_mps','arrival_m','start_b','mode','role','ai_enabled','auto_ai','spatial']:
        if not same(old[key],new[key]):differences.append(dict(ship=old['label'],field=key,before=old[key],after=new[key]))
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
terrain=next(a for a in actors if a.get_actor_label()=='QuangNinh_Terrain')
f=terrain.get_component_by_class(unreal.SeaTerrainCollisionComponent)
validation=unreal.ShipEnemyNavigationComponent.validate_installed_blueprint()
checks=dict(
    five_saved_AI=len(after['ships'])==5,
    eleven_existing_targets=len(after['targets'])==11,
    fleet_and_live_endpoint_settings_unchanged=not differences,
    player_start_unchanged=same(before['player_starts'],after['player_starts']),
    final_filter_saved=bool(f and f.filter_sea_level_dem and abs(f.sea_level_tolerance_m-.05)<1e-6),
    original_force_graph_valid=validation.startswith('OK:'),
    single_georeference=sum(isinstance(a,unreal.CesiumGeoreference) for a in actors)==1,
)
report=dict(checks=checks,differences=differences,validation=validation,passed=all(checks.values()),coordinate_absolute_tolerance=1e-6)
(root/'Docs/QuangNinhIntegration/SAVED_STATE_VERIFICATION.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report))
assert report['passed'], 'Saved QuangNinh configuration changed'
