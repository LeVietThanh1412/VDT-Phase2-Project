"""Serial real PIE starts through the existing editor bridge; no ship driving.

Run from the repository root with an already running, idle Unreal Editor.
Each fixture uses the real prematch menu and world travel. Raw diagnostic
geometry stays in Saved; summary hashes make replay comparisons explicit.
"""
import json,time,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from Tools.SeaNavigation.mcp_client import execute

root=Path(__file__).resolve().parents[2]
seeds=[20261028,1,42,12345,999,2026,707,31415,65537,8675309,20261028]
summary=dict(status='RUNNING',scope='Physical startup and native initial travel; combat disabled only in disposable fixtures',runs=[])
output=root/'Docs/QuangNinhIntegration/SEEDED_SPAWN_SUITE.json'
def save():output.write_text(json.dumps(summary,indent=2),encoding='utf-8')
save()
for index,seed in enumerate(seeds):
    filename=f'SEEDED_SPAWN_RUN_{index+1:02}_{seed}.json'
    path=output.parent/filename
    execute("import unreal; from pathlib import Path; p=Path(unreal.Paths.project_dir())/'Tools/NavalCombat/test_seeded_spawn.py'; exec(compile(p.read_text(encoding='utf-8'),str(p),'exec'),"+repr(dict(SEED=seed,OUTPUT=filename,OBSERVE_SECONDS=60,__name__='seed_suite_'+str(index)))+")")
    started=time.monotonic()
    while time.monotonic()-started<150:
        time.sleep(1)
        if not path.exists():continue
        try:report=json.loads(path.read_text(encoding='utf-8'))
        except (json.JSONDecodeError,PermissionError):continue
        if report.get('status')!='RUNNING':break
    else:raise TimeoutError(f'Seed {seed} fixture did not finish')
    row=dict(seed=seed,status=report['status'],report=filename,layout_sha256=report.get('layout_sha256'),travel_m=report.get('travel_m'),error=report.get('error'),failed_checks={k:v for k,v in report.get('checks',{}).items() if not v})
    summary['runs'].append(row);save();print(json.dumps(row),flush=True)
    time.sleep(2)
summary['distinct_seeds']=len({r['seed'] for r in summary['runs']})
summary['same_seed_replays_equal']=summary['runs'][0]['layout_sha256']==summary['runs'][-1]['layout_sha256']
summary['status']='PASSED' if all(r['status']=='PASSED' for r in summary['runs']) and summary['same_seed_replays_equal'] else 'FAILED'
save();print(summary['status'],flush=True)
