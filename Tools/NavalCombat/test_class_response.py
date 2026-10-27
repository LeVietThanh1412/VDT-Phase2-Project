"""Compare actual input response after water contact and settling, not load time."""
import builtins,json,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());kind=globals().get('CLASS_NAME','Small');ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
st=dict(handle=None,status='RUNNING',kind=kind,rows=[],error=None,phase='init',at=0,last=-1)
builtins.class_response=st
actions={k:unreal.load_asset('/Game/Ship/Inputs/'+k) for k in ['IA_MoveForwardBackward','IA_MoveLeftRight']}
def save():
    (root/('Docs/NavalCombat/SMALL_RESPONSE_'+kind.upper()+'.json')).write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','player','inputs']},indent=2),encoding='utf-8')
def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=ed.get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<1:return
    try:
        if st['phase']=='init':
            p=unreal.GameplayStatics.get_player_pawn(w,0);st['player']=p
            st['inputs']=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary')).call_method('GetLocalPlayerSubSystemFromPlayerController',args=(p.get_controller(),unreal.EnhancedInputLocalPlayerSubsystem.static_class()))
            for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn):
                if not a.get_component_by_class(unreal.ShipWeaponComponent):continue
                a.can_be_damaged=False;c=a.get_component_by_class(unreal.ShipCombatComponent);n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=False
                if n:n.stop_navigation()
                if a!=p:
                    next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM').set_simulate_physics(False)
                    a.set_actor_tick_enabled(False);a.set_actor_location(a.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
            assert p.get_component_by_class(unreal.ShipMovementComponent).apply_ship_class(getattr(unreal.ShipHullClass,kind.upper()))
            if kind=='Small':
                h=next(c for c in p.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
                assert all(h.does_socket_exist(name) for name in ['1','2','3','4','5']),'Small missing shared force/buoyancy socket contract'
                location=h.get_socket_transform('5',unreal.RelativeTransformSpace.RTS_COMPONENT).translation
                assert (location-unreal.Vector(1942.25,-323,0)).length()<.1,'Small rudder application socket must fit its compact hull'
                st['rudder_socket_cm']=[location.x,location.y,location.z]
            st.update(phase='settle',at=t)
        p=st['player'];m=p.get_component_by_class(unreal.ShipMovementComponent);h=next(c for c in p.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM');b=p.get_component_by_class(unreal.BuoyancyComponent)
        elapsed=t-st['at'];throttle=0;rudder=0
        if st['phase']=='settle' and t>6 and b.is_in_water_body() and abs(h.get_physics_linear_velocity().z)<50:
            st.update(phase='forward',at=t,initial_speed=m.signed_forward_speed());elapsed=0
        if st['phase']=='forward':
            throttle=1
            if elapsed>4:st.update(phase='turn',at=t);elapsed=0
        if st['phase']=='turn':
            throttle=.3;rudder=1
            if elapsed>7:
                r=st['rows'];forward=[x for x in r if x['phase']=='forward'];turn=[x for x in r if x['phase']=='turn' and x['elapsed']>2]
                samples=[x for x in forward if .12<x['elapsed']<.3];assert samples
                st['initial_acceleration_mps2']=sum((x['speed']-st['initial_speed'])/x['elapsed'] for x in samples)/len(samples)
                st['peak_speed_mps']=max(x['speed'] for x in forward);st['mean_turn_deg_s']=sum(abs(x['yaw_rate']) for x in turn)/len(turn)
                st['peak_turn_deg_s']=max(abs(x['yaw_rate']) for x in turn);st['max_roll_deg']=max(abs(x['roll']) for x in r)
                assert all(x['physics'] and abs(x['roll'])<45 and abs(x['pitch'])<25 for x in r)
                assert st['initial_acceleration_mps2']>5 and st['mean_turn_deg_s']>.2
                finish('PASSED');return
        st['inputs'].inject_input_vector_for_action(actions['IA_MoveForwardBackward'],unreal.Vector(throttle,0,0),[],[])
        st['inputs'].inject_input_vector_for_action(actions['IA_MoveLeftRight'],unreal.Vector(rudder,0,0),[],[])
        if t-st['last']>.025:
            st['last']=t;st['rows'].append(dict(t=t,phase=st['phase'],elapsed=elapsed,speed=m.signed_forward_speed(),yaw_rate=unreal.MathLibrary.dot_vector_vector(h.get_physics_angular_velocity_in_degrees(),h.get_up_vector()),roll=p.get_actor_rotation().roll,pitch=p.get_actor_rotation().pitch,physics=h.is_simulating_physics(),water=b.is_in_water_body()))
        if t>32:raise AssertionError('class response fixture timeout')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick);save();unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
