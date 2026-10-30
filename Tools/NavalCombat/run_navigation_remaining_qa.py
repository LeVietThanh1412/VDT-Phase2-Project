"""Serialize independent phase-1 PIE fixtures and stop on actual failure."""
import builtins,json,time,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
plan=[('verify_editor_reposition_pie.py',{},'PASSED'),
      ('verify_navigation_repair.py',dict(MODE='runtime_reposition'),'PASSED'),
      ('verify_navigation_island_pie.py',{},'PASSED'),
      ('verify_quangninh_collision_regression.py',{},'LAND_COLLISION_VERIFIED'),
      ('test_roles.py',dict(OUTPUT='NAVIGATION_COMBAT_RETURN_PIE.json'),'ALL_ROLE_CHECKS_PASSED')]
st=dict(status='RUNNING',stage=0,results=[],started=time.monotonic(),error=None)
builtins.navigation_remaining_qa=st

def save():
    (root/'Docs/NavalCombat/NAVIGATION_PHASE1_ACCEPTANCE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','scope']},indent=2),encoding='utf-8')

def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle'])

def tick(dt):
    try:
        if time.monotonic()-st['started']>1800:raise AssertionError('Navigation phase-1 fixture pipeline time bound')
        if 'scope' in st:
            result=st['scope']['st'];status=result.get('status')
            if status and status!='RUNNING' and ed.get_game_world() is None:
                item=plan[st['stage']];st['results'].append(dict(fixture=item[0],parameters=item[1],status=status,error=result.get('error')));save()
                if status!=item[2]:finish('FAILED');return
                st['stage']+=1;del st['scope']
                if st['stage']==len(plan):finish('PASSED');return
        if 'scope' not in st and ed.get_game_world() is None:
            name,params,_=plan[st['stage']];p=root/'Tools/NavalCombat'/name
            code=p.read_text(encoding='utf-8').replace('LAND_COLLISION_FINAL_FILTER.json','LAND_COLLISION_SKIRT_FIX.json')
            scope=dict(params,__name__='nav_qa_'+str(st['stage']));st['scope']=scope
            exec(compile(code,str(p),'exec'),scope);save()
    except Exception:st['error']=traceback.format_exc();finish('FAILED')

st['handle']=unreal.register_slate_post_tick_callback(tick);save()
