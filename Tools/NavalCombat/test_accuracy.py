"""Seeded shots with original Blueprint projectiles and real collision outcomes.
The moving victim uses real geographic A*/physics; fixture placements occur only
at scenario boundaries. No hit probabilities or fabricated hit results.
"""
import builtins,json,math,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
exec((root/'Tools/NavalCombat/test_roles.py').read_text().split('def tick(dt):')[0])
st=dict(handle=None,phase='init',rows=[],results=[],index=-1,last=-1,at=0,error=None,shots=[])
builtins.accuracy_test=st
cases=[('stationary','Easy',24),('stationary','Normal',24),('stationary','Hard',24),('moving','Easy',24),('moving','Normal',24),('moving','Hard',24)]
def world(s,p,height=14.7):
    g=s.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
    ecef=unreal.Vector(*chart['origin_ecef'])+unreal.Vector(*chart['east_axis'])*p[0]+unreal.Vector(*chart['north_axis'])*p[1]
    llh=g.transform_unreal_position_to_longitude_latitude_height(g.transform_earth_centered_earth_fixed_position_to_unreal(ecef));llh.z=height
    return g.transform_longitude_latitude_height_position_to_unreal(llh)
def relocate(s,p,yaw):
    s.set_actor_location(world(s,p),False,True);s.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),True)
def save():
    (root/'Docs/NavalCombat/ACCURACY_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','subject','threat','markers']},indent=2))
def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def begin(t):
    st['index']+=1;mode,skill,count=cases[st['index']];s=st['subject'];v=st['threat'];c=s.get_component_by_class(unreal.ShipCombatComponent);n=v.get_component_by_class(unreal.ShipEnemyNavigationComponent)
    # Drain completed scenario before moving either fixture; all existing rounds
    # have naturally reached their bounded lifetime during the drain interval.
    relocate(s,[750,-2150],-90);hull(v).set_simulate_physics(False);relocate(v,[750,-1250],-90 if mode=='stationary' else 0)
    n.stop_navigation()
    if mode=='moving':
        n.patrol_actor_a=st['markers'][0];n.patrol_actor_b=st['markers'][1];n.patrol_offset_a=unreal.Vector();n.patrol_offset_b=unreal.Vector()
        n.arrival_radius_m=40;n.patrol_wait_seconds=1;n.cruise_speed_mps=8;n.patrol_startup_delay_seconds=0;n.start_at_patrol_b=True;n.start_patrol();hull(v).set_simulate_physics(True)
    c.set_shooting_skill(getattr(unreal.ShipAISkill,skill.upper()));c.enable_combat=True;c.scan_targets()
    weapon=s.get_component_by_class(unreal.ShipWeaponComponent)
    st.update(phase='shoot',at=t,initial_shots=weapon.shots_fired,initial_hits=weapon.hits,last_shots=weapon.shots_fired,case=dict(mode=mode,skill=skill,count=count),case_rows=[])
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<1:return
    try:
        if st['phase']=='init':
            ships=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'))
            s=next(s for s in ships if s.get_actor_label()=='BP_ShipEnemy');v=next(s for s in ships if s.get_actor_label()=='Friendly_AI_1');st.update(subject=s,threat=v)
            for other in ships:
                other.can_be_damaged=False;c=other.get_component_by_class(unreal.ShipCombatComponent);n=other.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=False;c.enable_hit_and_run=False
                if n:n.stop_navigation()
                hull(other).set_simulate_physics(False)
                if other not in [s,v]:other.set_actor_location(other.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
            c=s.get_component_by_class(unreal.ShipCombatComponent);c.fire_cooldown_seconds=1;c.accuracy_seed=20261020
            markers=[];gameplay=unreal.get_default_object(unreal.GameplayStatics.static_class())
            for en in [[650,-1250],[1050,-1250]]:
                transform=unreal.Transform(location=world(s,en))
                actor=gameplay.call_method('BeginDeferredActorSpawnFromClass',args=(w,unreal.TargetPoint.static_class(),transform,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
                gameplay.call_method('FinishSpawningActor',args=(actor,transform,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT));markers.append(actor)
            st['markers']=markers;begin(t)
        s=st['subject'];v=st['threat'];c=s.get_component_by_class(unreal.ShipCombatComponent);weapon=s.get_component_by_class(unreal.ShipWeaponComponent)
        if weapon.shots_fired>st['last_shots']:
            mesh=s.get_component_by_class(unreal.SkeletalMeshComponent)
            forward=mesh.get_socket_rotation('front_gun_v_socket').get_forward_vector();launch=weapon.last_launch_velocity.normal()
            st['shots'].append(dict(t=t,case=st['index'],error=[c.current_aim_error_degrees.x,c.current_aim_error_degrees.y],muzzle_launch_dot=unreal.MathLibrary.dot_vector_vector(forward,launch)))
            st['last_shots']=weapon.shots_fired
        if t-st['last']>.5:
            st['last']=t;row=dict(t=t,case=st['index'],phase=st['phase'],shots=weapon.shots_fired-st['initial_shots'],hits=weapon.hits-st['initial_hits'],target_speed=hull(v).get_physics_linear_velocity().length()/100,
                distance=c.distance_to_target_m,status=c.firing_status,target=c.target.get_actor_label() if c.target else None,aim_samples=c.aim_samples)
            st['rows'].append(row);st['case_rows'].append(row);save()
        if st['phase']=='shoot' and weapon.shots_fired-st['initial_shots']>=st['case']['count']:
            c.enable_combat=False;st.update(phase='drain',at=t)
        elif st['phase']=='drain' and t-st['at']>=10:
            shots=weapon.shots_fired-st['initial_shots'];hits=weapon.hits-st['initial_hits']
            st['results'].append(dict(**st['case'],shots=shots,hits=hits,misses=shots-hits,hit_ratio=hits/shots,max_target_speed=max(r['target_speed'] for r in st['case_rows'])))
            if st['index']+1==len(cases):
                for mode in ['stationary','moving']:
                    easy=next(r for r in st['results'] if r['mode']==mode and r['skill']=='Easy');hard=next(r for r in st['results'] if r['mode']==mode and r['skill']=='Hard')
                    check(mode+' real misses',easy['misses']>=1)
                    check(mode+' Hard more accurate than Easy',hard['hit_ratio']>easy['hit_ratio'])
                    if mode=='moving':check('moving target uses actual physical velocity',easy['max_target_speed']>2 and hard['max_target_speed']>2)
                check('all launches follow animated muzzle',all(x['muzzle_launch_dot']>.999 for x in st['shots']))
                finish('ACCURACY_CHECKS_PASSED')
            else:begin(t)
        if st['phase']=='shoot' and t-st['at']>180:raise AssertionError('case did not produce required physical shots')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['checks']=[];st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
