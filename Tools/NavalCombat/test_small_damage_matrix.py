"""Real original BP projectiles against compact/original hulls in isolated PIE.

Kinematic, elevated broadside targets isolate actual collision/damage from
terrain. This does not certify navigation or natural combat behavior.
"""
import builtins,json,math,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
st=dict(handle=None,status='RUNNING',phase='init',checks=[],rows=[],error=None,pair=0,case=0,at=0)
builtins.small_damage_matrix=st
pairs=[('Small','Small','BP_ShipEnemy'),('Small','Medium','BP_ShipEnemy2'),('Medium','Small','BP_ShipEnemy3'),('Small','Large','Friendly_AI_1')]
cases=[('bow',.9),('midship',.5),('stern',.1),('friendly',.5)]
def hull(a):return next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
def weapon(a):return a.get_component_by_class(unreal.ShipWeaponComponent)
def check(name,value):
    st['checks'].append(dict(name=name,passed=bool(value)));assert value,name
def save():
    (root/'Docs/NavalCombat/SMALL_DAMAGE_MATRIX.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','source','victims','round']},indent=2),encoding='utf-8')
def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=ed.get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<2:return
    try:
        if st['phase']=='init':
            fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if weapon(a)]
            source=unreal.GameplayStatics.get_player_pawn(w,0);st.update(source=source,victims={a.get_actor_label():a for a in fleet if a!=source})
            for a in fleet:
                c=a.get_component_by_class(unreal.ShipCombatComponent);n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=False;c.enable_hit_and_run=False
                if n:n.stop_navigation()
                hull(a).set_simulate_physics(False)
                a.set_actor_location(a.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
            st.update(phase='fire',at=t+1)
        source=st['source'];sw=weapon(source)
        if st['phase']=='fire' and t>=st['at']:
            attacker,victim_class,label=pairs[st['pair']];victim=st['victims'][label];vw=weapon(victim)
            check('apply '+attacker+' attacker',source.get_component_by_class(unreal.ShipMovementComponent).apply_ship_class(getattr(unreal.ShipHullClass,attacker.upper())))
            if st['case']==0:
                check('apply '+victim_class+' victim',victim.get_component_by_class(unreal.ShipMovementComponent).apply_ship_class(getattr(unreal.ShipHullClass,victim_class.upper())))
                check('victim initial HP '+victim_class,vw.current_health==(38 if victim_class=='Small' else 100))
            name,fraction=cases[st['case']];victim.get_component_by_class(unreal.ShipCombatComponent).faction=1
            sw.faction=1 if name=='friendly' else 0
            c=source.get_component_by_class(unreal.ShipCombatComponent)
            if c:c.faction=sw.faction
            mesh=source.get_component_by_class(unreal.SkeletalMeshComponent);direction=mesh.get_socket_rotation('front_gun_v_socket').get_forward_vector();start=mesh.get_socket_location('front_gun_v_socket')
            yaw=math.degrees(math.atan2(direction.y,direction.x))+90;victim.set_actor_rotation(unreal.Rotator(yaw=yaw),True)
            low,high=hull(victim).get_local_bounds();local=unreal.Vector(low.x+(high.x-low.x)*fraction,0,0)
            endpoint=start+direction*30000+unreal.Vector(0,0,-.5*980*1.5**2)
            victim.set_actor_location(victim.get_actor_location()+endpoint-hull(victim).get_world_transform().transform_location(local),False,True)
            before=vw.current_health;events=vw.accepted_damage_events
            check(name+' original firing',sw.try_fire_weapon(unreal.Vector(),None,unreal.Vector(),False))
            round=sw.last_projectile;damage=round.get_component_by_class(unreal.ShipProjectileDamageComponent)
            check('attacker damage snapshot',damage.projectile_damage==(6 if attacker=='Small' else 10))
            if st['pair']==0 and st['case']==0:
                source.get_component_by_class(unreal.ShipMovementComponent).apply_ship_class(unreal.ShipHullClass.MEDIUM)
                check('in-flight Small round keeps damage after profile change',damage.projectile_damage==6)
            st.update(phase='impact',at=t+3,before=before,events=events,round=round)
        elif st['phase']=='impact' and t>=st['at']:
            attacker,victim_class,label=pairs[st['pair']];victim=st['victims'][label];vw=weapon(victim);name,fraction=cases[st['case']]
            expected=0 if name=='friendly' else (6 if name=='midship' else 5) if attacker=='Small' else (10 if name=='midship' else 8)
            delta=st['before']-vw.current_health;st['rows'].append(dict(attacker=attacker,victim=victim_class,case=name,damage=delta,expected=expected,hp=vw.current_health,region=str(vw.last_impact_region)))
            check(attacker+' vs '+victim_class+' '+name+' measured delta',abs(delta-expected)<.001)
            check('single hit / friendly guard',vw.accepted_damage_events-st['events']==(0 if name=='friendly' else 1))
            if unreal.SystemLibrary.is_valid(st['round']):st['round'].destroy_actor()
            st['case']+=1
            if st['case']==len(cases):
                # Retire this kinematic target before positioning the next one
                # at the same firing endpoint, so it cannot intercept later rounds.
                victim.set_actor_location(victim.get_actor_location()+unreal.Vector(0,0,100000),False,True)
                st['case']=0;st['pair']+=1
            st.update(phase='fire',at=t+1);save()
            if st['pair']==len(pairs):
                victim=st['victims']['BP_ShipEnemy3'];vw=weapon(victim);sw.faction=0
                check('prepare lethal eight HP through hostile damage',vw.receive_ship_damage(vw.current_health-8,source) and vw.current_health==8)
                check('Small dies exactly once',vw.receive_ship_damage(8,source) and vw.is_destroyed and vw.destruction_events==1)
                check('dead rejects duplicate kill',not vw.receive_ship_damage(8,source) and vw.destruction_events==1)
                st.update(phase='wreck',at=t+24)
        elif st['phase']=='wreck':
            victim=st['victims']['BP_ShipEnemy3']
            try:valid=unreal.SystemLibrary.is_valid(victim)
            except TypeError:valid=False
            if valid and t-st.get('wreck_last',0)>.2:
                st['wreck_last']=t;v=weapon(victim)
                st.setdefault('wreck_rows',[]).append(dict(t=t,stage=str(v.destruction_stage),sunk_m=v.sunk_distance_m,physics=hull(victim).is_simulating_physics(),deaths=v.destruction_events))
            if t>=st['at']:
                check('Small reached full sinking distance',max(r['sunk_m'] for r in st['wreck_rows'])>=64.9)
                check('Small staged wreck cleaned up',not valid);finish('PASSED')
        if t>110:raise AssertionError('bounded Small damage fixture timeout')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick);save()
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
