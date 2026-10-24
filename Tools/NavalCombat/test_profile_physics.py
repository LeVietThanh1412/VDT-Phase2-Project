"""Same fresh possessed player, same real input sequence, selected hull profile."""
import builtins,json,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());kind=globals().get('CLASS_NAME','Small')
st=dict(handle=None,kind=kind,rows=[],error=None,initialized=False,last=-1)
builtins.profile_physics_test=st
actions={x:unreal.load_asset('/Game/Ship/Inputs/'+x) for x in ['IA_MoveForwardBackward','IA_MoveLeftRight']}
def finish(status):
    st['status']=status
    (root/('Docs/NavalCombat/PROFILE_'+kind.upper()+'_PHYSICS.json')).write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','player','inputs']},indent=2))
    unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<1:return
    try:
        if not st['initialized']:
            p=unreal.GameplayStatics.get_player_pawn(w,0);st['player']=p
            st['inputs']=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary')).call_method('GetLocalPlayerSubSystemFromPlayerController',args=(unreal.GameplayStatics.get_player_controller(w,0),unreal.EnhancedInputLocalPlayerSubsystem.static_class()))
            for s in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C')):
                s.can_be_damaged=False;c=s.get_component_by_class(unreal.ShipCombatComponent);n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=False
                if n:n.stop_navigation()
                if s!=p:
                    h=next(x for x in s.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM');h.set_simulate_physics(False);s.set_actor_tick_enabled(False)
                    s.set_actor_location(s.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
            m=p.get_component_by_class(unreal.ShipMovementComponent)
            assert m.apply_ship_class(getattr(unreal.ShipHullClass,kind.upper()))
            st['initialized']=True
        p=st['player'];m=p.get_component_by_class(unreal.ShipMovementComponent);h=next(x for x in p.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM')
        value=1 if 3<t<6 else .3 if 6<=t<9 else 0;rudder=1 if 6<=t<9 else 0
        st['inputs'].inject_input_vector_for_action(actions['IA_MoveForwardBackward'],unreal.Vector(value,0,0),[],[])
        st['inputs'].inject_input_vector_for_action(actions['IA_MoveLeftRight'],unreal.Vector(rudder,0,0),[],[])
        if t-st['last']>.02:
            st['last']=t;g=p.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
            llh=g.transform_unreal_position_to_longitude_latitude_height(h.get_world_location());yaw=unreal.MathLibrary.dot_vector_vector(h.get_physics_angular_velocity_in_degrees(),h.get_up_vector())
            st['rows'].append(dict(t=t,speed=m.signed_forward_speed(),yaw_rate=yaw,height=llh.z,pitch=p.get_actor_rotation().pitch,roll=p.get_actor_rotation().roll,
                physics=h.is_simulating_physics(),mass=h.get_mass(),buoyancy=p.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body(),profile=str(m.hull_class)))
        if t>12:
            rows=st['rows'];start=min(rows,key=lambda r:abs(r['t']-3))
            samples=[r for r in rows if .12<r['t']-3<.3]
            st['initial_acceleration_mps2']=sum((r['speed']-start['speed'])/(r['t']-3) for r in samples)/len(samples)
            turn=[r for r in rows if 6.5<r['t']<8]
            st['peak_turn_yaw_rate']=max(abs(r['yaw_rate']) for r in turn)
            assert all(r['physics'] for r in rows)
            assert all(-10<r['height']<35 and abs(r['roll'])<65 and abs(r['pitch'])<45 for r in rows),('unstable class',kind)
            assert any(r['buoyancy'] for r in rows),'no water overlap'
            finish('PHYSICAL_PROFILE_CHECK_PASSED')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
