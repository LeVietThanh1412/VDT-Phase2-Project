"""Actual original projectiles strike bow/midship/stern on a rotated physical hull.
Supplemental self/duplicate guards are explicit pipeline-level probes.
"""
import builtins,json,math,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());st=dict(handle=None,phase='init',rows=[],checks=[],stage=0,at=0,error=None)
output=globals().get('OUTPUT','HULL_DAMAGE_PIE.json');profile=globals().get('PROFILE_CLASS')
builtins.hull_damage_test=st
cases=[('bow',.9,8),('midship',.5,10),('stern',.1,8),('rotated_midship',.5,10),('friendly',.5,0),('lethal_bow',.9,8)]
def hull(s):return next(c for c in s.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
def hp(s):return s.get_component_by_class(unreal.ShipWeaponComponent)
def guard_probes(w,source,victim,ally):
    gameplay=unreal.get_default_object(unreal.GameplayStatics.static_class())
    cls=unreal.load_class(None,'/Game/Ship/BluePrint/BP_ShipProjectileWithSpeed.BP_ShipProjectileWithSpeed_C')
    def round_from(owner):
        transform=unreal.Transform(location=source.get_actor_location()+unreal.Vector(0,0,2000000))
        round=gameplay.call_method('BeginDeferredActorSpawnFromClass',args=(w,cls,transform,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,owner,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        gameplay.call_method('FinishSpawningActor',args=(round,transform,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT));return round
    health=hp(victim);low,high=hull(victim).get_local_bounds()
    point=hull(victim).get_world_transform().transform_location(unreal.Vector((low.x+high.x)/2,0,0))
    round=round_from(source);before=health.current_health
    check('guard probe first hostile hit',health.receive_hull_impact(round,point) and health.current_health==before-10)
    check('same projectile victim claim only once',not health.receive_hull_impact(round,point) and health.current_health==before-10);round.destroy_actor()
    before=health.current_health
    for name,owner in [('self',victim),('friendly',ally),('unknown',None)]:
        round=round_from(owner);check(name+' impact guard rejects',not health.receive_hull_impact(round,point) and health.current_health==before);round.destroy_actor()
def check(name,passed):
    st['checks'].append(dict(name=name,passed=bool(passed)));assert passed,name
def save():
    (root/'Docs/NavalCombat'/output).write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','ships','source','victim','projectile','positions']},indent=2))
def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<1:return
    try:
        if st['phase']=='init':
            ships=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'))
            source=next(s for s in ships if s.get_actor_label()=='BP_ShipEnemy');victim=next(s for s in ships if s.get_actor_label()=='Friendly_AI_1')
            if profile:
                for s in [source,victim]:check(profile+' profile applied for combat',s.get_component_by_class(unreal.ShipMovementComponent).apply_ship_class(getattr(unreal.ShipHullClass,profile.upper())))
                st['combat_profile']=profile
            st.update(ships=ships,source=source,victim=victim,positions={s.get_name():s.get_actor_location() for s in ships})
            for s in ships:
                c=s.get_component_by_class(unreal.ShipCombatComponent);n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=False;c.enable_hit_and_run=False
                if n:n.stop_navigation()
                hull(s).set_simulate_physics(False)
                if s not in [source,victim]:s.set_actor_location(s.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
            guard_victim=next(s for s in ships if s.get_actor_label()=='Friendly_AI_2')
            guard_probes(w,source,guard_victim,next(s for s in ships if s.is_player_controlled()))
            hp(victim).debug_damage=True;st.update(phase='fire',at=t+1)
        source=st['source'];victim=st['victim'];weapon=hp(source);health=hp(victim)
        if st['phase']=='fire' and t>=st['at']:
            name,fraction,damage=cases[st['stage']]
            for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                if a.get_class().get_name().startswith('BP_ShipProjectile'):a.destroy_actor()
            source.get_component_by_class(unreal.ShipCombatComponent).faction=0 if name=='friendly' else 1
            if name=='rotated_midship':source.set_actor_rotation(unreal.Rotator(pitch=0,yaw=37,roll=0),True)
            if name=='lethal_bow':check('prepare lethal 8 HP',health.receive_ship_damage(health.current_health-8,source) and health.current_health==8)
            mesh=source.get_component_by_class(unreal.SkeletalMeshComponent);direction=mesh.get_socket_rotation('front_gun_v_socket').get_forward_vector();start=mesh.get_socket_location('front_gun_v_socket')
            # Broadside placement keeps projectile travel perpendicular to local X.
            yaw=math.degrees(math.atan2(direction.y,direction.x))+90
            victim.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),True)
            low,high=hull(victim).get_local_bounds();local=unreal.Vector(low.x+(high.x-low.x)*fraction,0,0)
            endpoint=start+direction*30000+unreal.Vector(0,0,.5*-980*1.5**2)
            point=hull(victim).get_world_transform().transform_location(local)
            victim.set_actor_location(victim.get_actor_location()+endpoint-point,False,True)
            before=health.current_health;events=health.accepted_damage_events
            fired=weapon.try_fire_weapon(unreal.Vector(),None,unreal.Vector(),False)
            check(name+' original Blueprint fired',fired)
            st.update(phase='impact',at=t+3,before_hp=before,before_events=events,projectile=weapon.last_projectile)
        elif st['phase']=='impact' and t>=st['at']:
            name,fraction,damage=cases[st['stage']]
            row=dict(case=name,hp=health.current_health,before_hp=st['before_hp'],damage=st['before_hp']-health.current_health,
                events=health.accepted_damage_events-st['before_events'],region=str(health.last_impact_region),normalized_x=health.last_normalized_impact_x,
                local=[health.last_impact_local.x,health.last_impact_local.y,health.last_impact_local.z],deaths=health.destruction_events)
            st['rows'].append(row);check(name+' measured health delta',abs(row['damage']-damage)<.001)
            check(name+' single impact event',row['events']==(0 if name=='friendly' else 1))
            if name!='friendly':check(name+' correct local region',('MIDSHIP' if 'midship' in name else 'STERN' if name=='stern' else 'BOW') in row['region'])
            if name=='lethal_bow':
                check('one death event',health.is_destroyed and health.destruction_events==1)
                check('dead rejects further damage',not health.receive_ship_damage(100,source) and health.destruction_events==1)
                finish('HULL_COLLISION_CHECKS_PASSED');return
            st['stage']+=1;st.update(phase='fire',at=t+2);save()
        if t>50:raise AssertionError('hull collision fixture timed out')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
