"""Serial real-editor tests; stop on failure, never treat launch as a pass."""
import unreal,builtins,time,json,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
tests=globals().get('TESTS',[('verify_red_assault.py','red_assault_probe',{'FAILOVER':True}),('verify_hull_stability.py','hull_stability_probe',{}),('verify_shared_magazine.py','magazine_probe',{})])
tests=tests[:int(globals().get('LIMIT_TESTS',len(tests)))]
suite=dict(status='RUNNING',index=-1,results=[],next_at=0,started=time.monotonic(),error=None)
builtins.remaining_suite=suite

def save():(root/'Saved/REMAINING_PIE_SUITE.json').write_text(json.dumps({k:v for k,v in suite.items() if k!='handle'},indent=2),encoding='utf-8')
def tick(dt):
 try:
  if suite['index']>=0:
   state=getattr(builtins,tests[suite['index']][1])
   if state['status']=='RUNNING':return
   if len(suite['results'])<=suite['index']:
    suite['results'].append(dict(script=tests[suite['index']][0],status=state['status'],error=state.get('error')));suite['next_at']=time.monotonic()+2;save()
    if state['status']!='PASSED':suite['status']='FAILED';unreal.unregister_slate_post_tick_callback(suite['handle']);save();return
  if ed.get_game_world() or time.monotonic()<suite['next_at']:return
  suite['index']+=1
  if suite['index']>=len(tests):
   suite['status']='PASSED';unreal.unregister_slate_post_tick_callback(suite['handle']);save();return
  name,key,options=tests[suite['index']];p=root/'Tools/NavalCombat'/name;save();exec(compile(p.read_text(encoding='utf-8'),str(p),'exec'),dict(options,__name__='suite_'+key))
 except Exception:suite['status']='FAILED';suite['error']=traceback.format_exc();unreal.unregister_slate_post_tick_callback(suite['handle']);save()
suite['handle']=unreal.register_slate_post_tick_callback(tick);save()
