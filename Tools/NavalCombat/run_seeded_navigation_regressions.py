"""Serial island-support, mixed-class and repeated seeded-patrol PIE checks.

Waits for the startup suite if it is currently running. Never competes for an
editor or changes a running fixture's ships. Failures remain explicit.
"""
import json,time,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root))
from Tools.SeaNavigation.mcp_client import execute
folder=root/'Docs/QuangNinhIntegration'
plan=[
    dict(script='verify_quangninh_collision_regression.py',params={},replace=('LAND_COLLISION_FINAL_FILTER.json','LAND_COLLISION_OPERATIONAL_LOD.json'),output=folder/'LAND_COLLISION_OPERATIONAL_LOD.json',expected='LAND_COLLISION_VERIFIED',timeout=240),
    dict(script='test_seeded_spawn.py',params=dict(SEED=42,HULL_CLASSES=[0,1,2,0,1,2],OBSERVE_SECONDS=60,OUTPUT='SEEDED_ALL_CLASSES_STARTUP.json'),output=folder/'SEEDED_ALL_CLASSES_STARTUP.json',expected='PASSED',timeout=180),
    dict(script='test_seeded_spawn.py',params=dict(SEED=20261028,REQUIRED_LEGS=3,LOOP_LIMIT_SECONDS=600,WALL_LIMIT_SECONDS=750,OUTPUT='SEEDED_REPEATED_PATROL.json'),output=folder/'SEEDED_REPEATED_PATROL.json',expected='PASSED',timeout=800)]
result=dict(status='WAITING_FOR_SEED_SUITE',runs=[]);output=folder/'SEEDED_NAVIGATION_REGRESSIONS.json'
def save():output.write_text(json.dumps(result,indent=2),encoding='utf-8')
save();beg=time.monotonic()
while True:
    suite=json.loads((folder/'SEEDED_SPAWN_SUITE.json').read_text(encoding='utf-8'))
    if suite['status']!='RUNNING':break
    if time.monotonic()-beg>1800:raise TimeoutError('Startup suite did not finish')
    time.sleep(2)
time.sleep(3);result['status']='RUNNING';save()
for stage in plan:
    code="import unreal; from pathlib import Path; p=Path(unreal.Paths.project_dir())/'Tools/NavalCombat'/"+repr(stage['script'])+"; source=p.read_text(encoding='utf-8'); "
    if 'replace' in stage:code+='source=source.replace('+repr(stage['replace'][0])+','+repr(stage['replace'][1])+'); '
    code+='exec(compile(source,str(p),\'exec\'),'+repr(dict(stage['params'],__name__='seed_regression'))+')'
    execute(code);beg=time.monotonic()
    while time.monotonic()-beg<stage['timeout']:
        time.sleep(1)
        if not stage['output'].exists() or stage['output'].stat().st_mtime<time.time()-(time.monotonic()-beg)-2:continue
        try:j=json.loads(stage['output'].read_text(encoding='utf-8'))
        except (json.JSONDecodeError,PermissionError):continue
        if j.get('status') not in [None,'RUNNING']:break
    else:raise TimeoutError(stage['script'])
    row=dict(script=stage['script'],parameters=stage['params'],status=j['status'],passed=j['status']==stage['expected'],report=str(stage['output'].relative_to(root)),error=j.get('error'))
    result['runs'].append(row);save();print(json.dumps(row),flush=True);time.sleep(3)
result['status']='PASSED' if all(r['passed'] for r in result['runs']) else 'FAILED';save();print(result['status'],flush=True)
