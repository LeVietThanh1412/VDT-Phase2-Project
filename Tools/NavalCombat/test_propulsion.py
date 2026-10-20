"""Real Enhanced Input W/S and original AddForce; no velocity overrides."""
import builtins,json,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());st=dict(handle=None,phase='init',rows=[],events=[],at=0,last=-1,error=None)
builtins.propulsion_test=st
action=unreal.load_asset('/Game/Ship/Inputs/IA_MoveForwardBackward')
def finish(status):
    st['status']=status
    (root/'Docs/NavalCombat/PROPULSION_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','player','inputs']},indent=2))
    unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def phase(name,t):st.update(phase=name,at=t)
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<1:return
    try:
        if st['phase']=='init':
            p=unreal.GameplayStatics.get_player_pawn(w,0);st['player']=p
            st['inputs']=next(x for x in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not x.get_name().startswith('Default__'))
            for s in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C')):
                s.can_be_damaged=False;c=s.get_component_by_class(unreal.ShipCombatComponent);n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=False
                if n:n.stop_navigation()
                if s!=p:
                    h=next(x for x in s.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM');h.set_simulate_physics(False)
                    s.set_actor_location(s.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
            phase('settle',t)
        p=st['player'];m=p.get_component_by_class(unreal.ShipMovementComponent);speed=m.signed_forward_speed();elapsed=t-st['at'];value=0
        if st['phase']=='settle' and elapsed>2 and abs(speed)<.1:phase('forward',t);st['events'].append(dict(phase='forward',t=t,start_speed=speed))
        if st['phase']=='forward':
            value=1
            if t-st['at']>5:phase('brake',t)
        elif st['phase']=='brake':
            value=-1 if speed>.8 else 0
            if speed<=.8:phase('rest',t)
        elif st['phase']=='rest' and elapsed>5 and abs(speed)<.1:
            phase('reverse',t);st['events'].append(dict(phase='reverse',t=t,start_speed=speed))
        elif st['phase']=='reverse':
            value=-1
            if t-st['at']>6:
                f=[r['speed'] for r in st['rows'] if r['phase']=='forward'];r=[r['speed'] for r in st['rows'] if r['phase']=='reverse']
                assert 24<max(f)<=26.5,('forward cap',max(f))
                assert -7.5<=min(r)<-5.5,('reverse cap',min(r))
                st['peak_forward']=max(f);st['peak_reverse']=min(r)
                # Use the same short initial interval before either speed limiter
                # engages, with actual elapsed time and measured starting speed.
                accelerations={}
                for event in st['events']:
                    samples=[x for x in st['rows'] if x['phase']==event['phase'] and .12<x['t']-event['t']<.3]
                    assert samples,('missing acceleration samples',event)
                    accelerations[event['phase']]=sum(abs((x['speed']-event['start_speed'])/(x['t']-event['t'])) for x in samples)/len(samples)
                st['measured_acceleration_mps2']=accelerations
                st['acceleration_ratio']=accelerations['reverse']/accelerations['forward']
                assert .25<st['acceleration_ratio']<.7,st['acceleration_ratio']
                finish('PROPULSION_CHECKS_PASSED');return
        st['inputs'].inject_input_vector_for_action(action,unreal.Vector(value,0,0),[],[])
        if t-st['last']>.02:
            st['last']=t;st['rows'].append(dict(t=t,phase=st['phase'],speed=speed,input=value,player=p.is_player_controlled()))
        if t>45:raise AssertionError('propulsion fixture timed out')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
