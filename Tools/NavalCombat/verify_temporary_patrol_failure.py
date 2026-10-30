"""Reject a temporary land endpoint, then recover when the live marker is valid.

Only a disposable PIE target marker is moved. The physical ship is untouched.
"""
import builtins,json,math,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
st=dict(status='RUNNING',phase='init',last=-1,rows=[],error=None)
builtins.temporary_patrol_failure=st

def save():
    (root/'Docs/NavalCombat/NAVIGATION_TEMPORARY_ENDPOINT.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','subject','original','controller']},indent=2),encoding='utf-8')

def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle'])
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()

def tick(dt):
    w=ed.get_game_world()
    if not w:return
    try:
        t=unreal.GameplayStatics.get_time_seconds(w)
        if t<1:return
        for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn):
            if (c:=a.get_component_by_class(unreal.ShipCombatComponent)):c.enable_combat=False
        if st['phase']=='init':
            a=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_actor_label()=='BP_ShipEnemy2')
            n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
            g=a.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
            st.update(subject=a,original=n.patrol_actor_b.get_actor_location(),controller=a.get_controller().get_path_name(),phase='unreachable')
            chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
            blocked=[14150,4050] # Verified excluded cell inside candidate 1.
            ecef=unreal.Vector(*chart['origin_ecef'])+unreal.Vector(*chart['east_axis'])*blocked[0]+unreal.Vector(*chart['north_axis'])*blocked[1]
            n.patrol_actor_b.set_actor_location(g.transform_earth_centered_earth_fixed_position_to_unreal(ecef),False,True)
            st['temporary_blocked_target_en_m']=blocked
        a=st['subject'];n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
        if t-st['last']>=1:
            st['last']=t;st['rows'].append(dict(t=t,phase=st['phase'],diagnostics=json.loads(n.get_navigation_diagnostics())))
            save()
        if st['phase']=='unreachable' and t>=18:
            failed=[r['diagnostics'] for r in st['rows'] if r['diagnostics']['navigation_state']=='EShipNavigationState::Failed']
            assert failed and all(not r['path_success'] and r['throttle']==0 and r['failure'] for r in failed),'invalid endpoint was not explicitly rejected'
            st['rejected_samples']=len(failed)
            n.patrol_actor_b.set_actor_location(st['original'],False,True)
            st.update(phase='restored',restored_at=t,restore_position=[a.get_actor_location().x,a.get_actor_location().y,a.get_actor_location().z])
        if st['phase']=='restored' and t>=st['restored_at']+18:
            p=a.get_actor_location();distance=math.dist([p.x,p.y,p.z],st['restore_position'])/100
            st['checks']=dict(explicit_failure_with_neutral_drive=st['rejected_samples']>0,
                live_target_restoration_replans=n.pathfinding_succeeded and not n.last_navigation_failure_reason,
                movement_resumes=distance>100,controller_preserved=a.get_controller().get_path_name()==st['controller'],
                no_teleport_recovery=n.reposition_reinitializations==0)
            st['post_restore_displacement_m']=distance
            finish('PASSED' if all(st['checks'].values()) else 'FAILED')
        elif t>55:raise AssertionError('Temporary endpoint test time bound')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')

st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
