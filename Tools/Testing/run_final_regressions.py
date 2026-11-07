"""Serialized final PIE matrix. Stops on the first failed fixture."""
import unreal,builtins,json,time,traceback,hashlib
from pathlib import Path
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
tests=globals().get('TESTS',[
 ('Tools/Testing/verify_final_demo.py','final_demo_probe',dict(KIND='nonlethal',SEED=20261028,OUTPUT='FINAL_CONTACT_20261028.json',CAPTURE=False)),
 ('Tools/Testing/verify_final_demo.py','final_demo_probe',dict(KIND='nonlethal',SEED=20261029,OUTPUT='FINAL_CONTACT_20261029.json',CAPTURE=False)),
 ('Tools/Testing/verify_final_demo.py','final_demo_probe',dict(KIND='nonlethal',SEED=20261030,OUTPUT='FINAL_CONTACT_20261030.json',CAPTURE=False)),
 ('Tools/Testing/verify_final_demo.py','final_demo_probe',dict(KIND='patrol',FULL_AI=False,PLAYER_TEAM=1,OUTPUT='FINAL_RED_PLAYER.json',OBSERVE_SECONDS=35,CAPTURE=False)),
 ('Tools/Testing/verify_final_demo.py','final_demo_probe',dict(KIND='patrol',FULL_AI=False,BLUE_COUNT=2,RED_COUNT=1,HULL_CLASSES=[2,1,0,2,1,0],OUTPUT='FINAL_ASYMMETRIC.json',OBSERVE_SECONDS=35,CAPTURE=False)),
 ('Tools/Testing/verify_final_demo.py','final_demo_probe',dict(KIND='turns',BATTLE_MODE=1,OUTPUT='FINAL_TURNS.json',OBSERVE_SECONDS=120,CAPTURE=False)),
 ('Tools/Testing/verify_core_gameplay.py','core_gameplay_probe',{}),
 ('Tools/NavalCombat/verify_shared_magazine.py','magazine_probe',{}),
 ('Tools/NavalCombat/verify_hull_stability.py','hull_stability_probe',dict(OUTPUT='FINAL_HULL_STABILITY.json')),
])
output=globals().get('OUTPUT','FINAL_REGRESSION_SUITE.json')
suite=dict(status='RUNNING',index=-1,results=[],next_at=0,error=None,started=time.monotonic(),native_dll_sha256=hashlib.sha256((root/'Binaries/Win64/UnrealEditor-VDTPhase2Project.dll').read_bytes()).hexdigest());builtins.final_regression_suite=suite

def save():(root/'Saved'/output).write_text(json.dumps({k:v for k,v in suite.items() if k!='handle'},indent=2),encoding='utf-8')
def tick(dt):
 try:
  if suite['index']>=0:
   state=getattr(builtins,tests[suite['index']][1])
   if state['status']=='RUNNING':return
   if len(suite['results'])<=suite['index']:
    suite['results'].append(dict(script=tests[suite['index']][0],options=tests[suite['index']][2],status=state['status'],error=state.get('error')));suite['next_at']=time.monotonic()+2;save()
    if state['status']!='PASSED':suite['status']='FAILED';unreal.unregister_slate_post_tick_callback(suite['handle']);save();return
  if ed.get_game_world() or time.monotonic()<suite['next_at']:return
  suite['index']+=1
  if suite['index']>=len(tests):suite['status']='PASSED';unreal.unregister_slate_post_tick_callback(suite['handle']);save();return
  path,key,options=tests[suite['index']];p=root/path;save();exec(compile(p.read_text(encoding='utf-8-sig'),str(p),'exec'),dict(options))
 except Exception:suite['status']='FAILED';suite['error']=traceback.format_exc();unreal.unregister_slate_post_tick_callback(suite['handle']);save()
suite['handle']=unreal.register_slate_post_tick_callback(tick);save()
