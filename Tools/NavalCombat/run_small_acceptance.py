"""Serialize fresh PIE fixtures; stop on a failed fixture, never overlap worlds."""
import builtins,json,time,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
st=dict(handle=None,status='RUNNING',stage=0,results=[],error=None,started=time.monotonic())
builtins.small_acceptance=st
plan=[('test_class_response.py','class_response',dict(CLASS_NAME='Small')),('test_class_response.py','class_response',dict(CLASS_NAME='Medium')),('test_small_blueprint_selection.py','small_selection',{}),('test_small_damage_matrix.py','small_damage_matrix',{}),('verify_small_ai_patrol.py','small_ai_patrol',{}),('test_small_underway.py','small_underway',{})]
def save():
    (root/'Docs/NavalCombat/SMALL_ACCEPTANCE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','scope']},indent=2),encoding='utf-8')
def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle'])
def tick(dt):
    try:
        if time.monotonic()-st['started']>420:raise AssertionError('Small fixture pipeline wall-time bound')
        if 'scope' in st:
            result=st['scope']['st'];status=result.get('status')
            if status and status!='RUNNING' and ed.get_game_world() is None:
                st['results'].append(dict(fixture=plan[st['stage']][0],parameters=plan[st['stage']][2],status=status,error=result.get('error')))
                save()
                if status not in ['PASSED','PHYSICAL_PROFILE_CHECK_PASSED']:finish('FAILED');return
                st['stage']+=1;del st['scope']
                if st['stage']==len(plan):finish('PASSED');return
        if 'scope' not in st and ed.get_game_world() is None:
            script,_,params=plan[st['stage']];p=root/'Tools/NavalCombat'/script;code=p.read_text().replace('Docs/NavalCombat/PROFILE_','Docs/NavalCombat/SMALL_FINAL_PROFILE_')
            scope=dict(params,__name__='small_acceptance_'+str(st['stage']));st['scope']=scope
            exec(compile(code,str(p),'exec'),scope);save()
    except Exception:
        st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick);save()
