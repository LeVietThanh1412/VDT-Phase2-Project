"""Serialized, bounded real PIE suite. Every fixture uses a fresh saved-level world."""
import builtins,json,time,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());state=dict(handle=None,index=-1,phase='NEXT',rows=[],seen=False,at=0,error=None)
builtins.naval_suite=state
steps=[('initialization','test_initialization.py',12,{}),('player_controls','test_player_controls.py',30,{}),
    ('ranges_turrets','test_ranges.py',120,{}),('damage_ui','test_damage_ui.py',100,{}),('defeat','test_defeat.py',15,{}),
    ('patrol','run_battle_validation.py',2455,dict(MODE='patrol',LIMIT=2400)),
    ('battle','run_battle_validation.py',405,dict(MODE='battle',LIMIT=360)),
    ('focused_retreat','test_retreat.py',155,{}),('real_island_occlusion','test_island_occlusion.py',25,{}),('saved_state_restart','test_initialization.py',12,{})]
if globals().get('SUITE_FILTER'):steps=[s for s in steps if s[0] in SUITE_FILTER]
def save():
    (root/'Docs/NavalCombat/PIE_SUITE.json').write_text(json.dumps({k:v for k,v in state.items() if k not in ['handle','scope']},indent=2))
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();now=time.monotonic()
    try:
        if state['phase']=='NEXT' and now>state['at']:
            if w:return
            state['index']+=1
            if state['index']>=len(steps):
                state['phase']='COMPLETE';save();unreal.unregister_slate_post_tick_callback(state['handle']);return
            name,file,limit,scope=steps[state['index']];state.update(phase='RUNNING',name=name,seen=False,at=now,scope=scope)
            exec((root/'Tools/NavalCombat'/file).read_text(),scope);save()
        elif state['phase']=='RUNNING':
            if w:state['seen']=True
            if state['seen'] and not w:
                state['rows'].append(dict(name=state['name'],completion='PIE_STOPPED_BY_FIXTURE',wall_seconds=now-state['at']))
                state.update(phase='NEXT',at=now+1);save()
            elif now-state['at']>steps[state['index']][2]:
                state['rows'].append(dict(name=state['name'],completion='DRIVER_TIMEOUT',wall_seconds=now-state['at']))
                if w:unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
                state.update(phase='NEXT',at=now+2);save()
    except Exception:
        state['error']=traceback.format_exc();state['phase']='ERROR';save();unreal.unregister_slate_post_tick_callback(state['handle'])
state['handle']=unreal.register_slate_post_tick_callback(tick);save()
