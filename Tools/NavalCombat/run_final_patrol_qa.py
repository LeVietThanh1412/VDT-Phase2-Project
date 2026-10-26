"""Final saved-level B/A/B soak with non-mutating frame/cache telemetry.

Reuses the accepted patrol fixture; combat/damage isolation is unchanged.
No camera, ship transform, force, speed, time dilation or cache budget overrides.
"""
import builtins,json,os,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
audit=root/'Tools/NavalCombat/verify_quangninh_saved_state.py'
code=audit.read_text().replace('QUANGNINH_CACHE_SAVED_AUDIT.json','FINAL_SAVED_AUDIT.json').replace('SAVED_STATE_VERIFICATION.json','FINAL_SAVED_STATE_VERIFICATION.json')
exec(compile(code,str(audit),'exec'),{'__name__':'final_saved_audit'})
base=root/'Tools/NavalCombat/verify_patrol_loops.py';scope=dict(OUTPUT='FINAL_INTEGRATED_PATROL.json',LIMIT=480,REQUIRED_LEGS=3,__name__='final_patrol')
perf=dict(handle=None,frames=[],cache=[],last=-1,editor_pid=os.getpid(),error=None,status='RUNNING',had_world=False)
builtins.final_patrol_performance=perf
def persist():
    p=root/'Docs/NavalCombat/FINAL_PATROL_PERFORMANCE.json'
    p.write_text(json.dumps({k:v for k,v in perf.items() if k!='handle'},indent=2),encoding='utf-8')
def tick(dt):
    try:
        w=editor.get_game_world();state=scope.get('state')
        if not w:
            if not perf['had_world']:return
            if state and state['status']=='RUNNING':
                state['status']='INTERRUPTED';state['error']='PIE ended before the patrol fixture completed'
                if state['handle']:unreal.unregister_slate_post_tick_callback(state['handle']);state['handle']=None
                scope['save']()
            perf['status']=state['status'] if state else 'INTERRUPTED';persist();unreal.unregister_slate_post_tick_callback(perf['handle']);return
        perf['had_world']=True;t=unreal.GameplayStatics.get_time_seconds(w)
        if t>=12:perf['frames'].append(dict(t=t,slate_dt=dt))
        if t-perf['last']>=1:
            perf['last']=t
            terrain=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Cesium3DTileset) if a.get_actor_label()=='QuangNinh_Terrain')
            component=terrain.get_component_by_class(unreal.SeaTerrainCollisionComponent)
            perf['cache'].append(dict(t=t,records=component.filtered_tiles,proxies=len(terrain.get_components_by_class(unreal.ProceduralMeshComponent)),land_triangles=component.retained_land_triangles,submerged_triangles=component.retained_submerged_triangles,error=component.last_error))
            if len(perf['cache'])%30==0:persist()
    except Exception:
        perf['error']=traceback.format_exc();perf['status']='PERF_PROBE_ERROR';persist();unreal.unregister_slate_post_tick_callback(perf['handle'])
exec(compile(base.read_text(),str(base),'exec'),scope)
perf['handle']=unreal.register_slate_post_tick_callback(tick)
