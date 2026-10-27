"""Disposable PIE: actual Small player, Enhanced Input and original physics.

Other ships are frozen above the test area to isolate hull tuning. This is not
the multi-ship navigation/seed acceptance test. No player velocity is assigned.
"""
import builtins,json,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
st=dict(handle=None,status='RUNNING',rows=[],checks=[],error=None,last=-1,initialized=False)
builtins.small_underway=st
actions={x:unreal.load_asset('/Game/Ship/Inputs/'+x) for x in ['IA_MoveForwardBackward','IA_MoveLeftRight','IA_Aim','IA_Fire']}
def check(name,value):
    st['checks'].append(dict(name=name,passed=bool(value)));assert value,name
def save():
    (root/'Docs/NavalCombat/SMALL_UNDERWAY.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','player','inputs']},indent=2),encoding='utf-8')
def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=ed.get_game_world()
    if not w:
        if st.get('started'):finish('INTERRUPTED')
        return
    st['started']=True
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<1:return
    try:
        if not st['initialized']:
            p=unreal.GameplayStatics.get_player_pawn(w,0);st['player']=p
            st['inputs']=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary')).call_method('GetLocalPlayerSubSystemFromPlayerController',args=(p.get_controller(),unreal.EnhancedInputLocalPlayerSubsystem.static_class()))
            for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn):
                weapon=a.get_component_by_class(unreal.ShipWeaponComponent)
                if not weapon:continue
                a.can_be_damaged=False;c=a.get_component_by_class(unreal.ShipCombatComponent);n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=False
                if n:n.stop_navigation()
                if a!=p:
                    next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM').set_simulate_physics(False)
                    a.set_actor_tick_enabled(False);a.set_actor_location(a.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
            check('apply real Small',p.get_component_by_class(unreal.ShipMovementComponent).apply_ship_class(unreal.ShipHullClass.SMALL))
            check('Small health 38',p.get_component_by_class(unreal.ShipWeaponComponent).max_health==38 and p.get_component_by_class(unreal.ShipWeaponComponent).current_health==38)
            st['initialized']=True
        p=st['player'];m=p.get_component_by_class(unreal.ShipMovementComponent);weapon=p.get_component_by_class(unreal.ShipWeaponComponent)
        parts={c.get_name():c for c in p.get_components_by_class(unreal.PrimitiveComponent)};h=parts['ShipSM'];driver=parts['Ship'];gun=parts['SmallGunPitch']
        value=1 if 4<t<14 else .3 if 14<=t<22 else -1 if 22<=t<30 else 0
        rudder=1 if 14<=t<22 else 0
        for action,v in [('IA_MoveForwardBackward',value),('IA_MoveLeftRight',rudder),('IA_Aim',1 if 32<t<37 else 0),('IA_Fire',1 if 35<t<35.2 else 0)]:
            st['inputs'].inject_input_vector_for_action(actions[action],unreal.Vector(v,0,0),[],[])
        if t-st['last']>.05:
            st['last']=t;anchor=p.get_component_by_class(unreal.CesiumGlobeAnchorComponent);llh=anchor.get_longitude_latitude_height()
            bone=driver.get_socket_transform('front_gun_v_jnt');gt=gun.get_world_transform()
            st['rows'].append(dict(t=t,speed=m.signed_forward_speed(),height=llh.z,roll=p.get_actor_rotation().roll,pitch=p.get_actor_rotation().pitch,
                yaw_rate=unreal.MathLibrary.dot_vector_vector(h.get_physics_angular_velocity_in_degrees(),h.get_up_vector()),water=p.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body(),
                physics=h.is_simulating_physics(),actor_scale=str(p.get_actor_scale3d()),gun_position_error_cm=(bone.translation-gt.translation).length(),gun_axis_dot=unreal.MathLibrary.dot_vector_vector(bone.rotation.rotator().get_forward_vector(),gt.rotation.rotator().get_forward_vector()),shots=weapon.shots_fired))
            if len(st['rows'])%30==0:save()
        for at,label in [(10,'NORMAL'),(34,'GUN')]:
            if t>at and label not in st.setdefault('screenshots',[]):
                unreal.SystemLibrary.execute_console_command(w,'Shot filename='+str(root/'Docs/NavalCombat'/('SMALL_FIXED_'+label+'.png')));st['screenshots'].append(label)
        if t>38:
            r=st['rows'];steady=[x for x in r if 5<x['t']<30]
            check('one simulated body',sum(c.is_simulating_physics() for c in parts.values())==1)
            check('hidden original hull has no collision',driver.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and not driver.is_visible())
            low,high=h.get_local_bounds();check('compact physical length 66 m',abs(high.x-low.x-6600)<1)
            check('actual forward speed 28-30',28<max(x['speed'] for x in r)<30)
            check('reverse limit',-7.6<min(x['speed'] for x in r)<-5)
            check('underway stable',all(abs(x['roll'])<45 and abs(x['pitch'])<25 and -2<x['height']<15 for x in steady))
            st['steady_water_fraction']=sum(x['water'] for x in steady)/len(steady)
            st['max_gun_position_error_cm']=max(x['gun_position_error_cm'] for x in r)
            check('visible gun follows exact firing bone',st['max_gun_position_error_cm']<.5 and min(x['gun_axis_dot'] for x in r)>.999)
            check('real input fired original projectile',weapon.shots_fired>0)
            check('Small source base damage',weapon.base_projectile_damage==6)
            finish('PASSED')
    except Exception:
        st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick)
save()
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
