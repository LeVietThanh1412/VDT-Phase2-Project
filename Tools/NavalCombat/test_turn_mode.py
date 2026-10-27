"""One scheduler, 18 real 10-second windows, inactive gates and live physics.

Disposable actual saved-level PIE. No world pause, time dilation or teleporting.
The boundary round uses the original BP spawn path with an upward fixture launch
so it cannot hit the sea before its shooter's window closes.
"""
import builtins,json,traceback,math
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());st=dict(handle=None,checks=[],turns=[],rows=[],error=None,last=-1,phase=0,projectiles=[])
builtins.naval_turn_test=st
def hp(a):return a.get_component_by_class(unreal.ShipWeaponComponent)
def valid(a):
    try:return unreal.SystemLibrary.is_valid(a)
    except TypeError:return False
def zero(v):return v.x*v.x+v.y*v.y+v.z*v.z<1e-8
def check(name,value):st['checks'].append(dict(name=name,passed=bool(value)));assert value,name
def save():
    skip=['handle','battle','ships','controllers','rounds','player','dead','enemy','source','reds']
    (root/'Docs/NavalCombat/TURN_MODE_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in skip},indent=2),encoding='utf-8')
def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    try:
        if t<3:return
        if st['phase']==0:
            lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
            battle=lib.call_method('GetWorldSubsystem',args=(w,unreal.NavalBattleSubsystem.static_class()))
            st['initial_roster']=dict(configured=battle.match_configured,blue=battle.blue_alive,red=battle.red_alive,error=battle.configuration_error,pawns=[dict(name=a.get_name(),team=hp(a).team(),player=a.is_player_controlled()) for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if hp(a)])
            check('default saved roster configured',battle.match_configured and battle.blue_alive==3 and battle.red_alive==3)
            check('direct PIE real time default',battle.battle_mode==unreal.NavalBattleMode.REAL_TIME)
            ships=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if hp(a)]
            player=unreal.GameplayStatics.get_player_pawn(w,0)
            for a in ships:
                c=a.get_component_by_class(unreal.ShipCombatComponent)
                if c:c.enable_combat=False
            check('real time all ships permitted',all(battle.is_ship_active(a) for a in ships))
            st.update(battle=battle,ships=ships,player=player,reds=[a for a in ships if hp(a).team()==1],controllers=[a.get_controller() for a in ships if a!=player],rounds=[],positions={a.get_name():[a.get_actor_location().x,a.get_actor_location().y] for a in ships},phase=1,started=t)
            battle.set_battle_mode(unreal.NavalBattleMode.TURN_BASED)
        battle=st['battle'];active=battle.active_turn_ship
        if st['phase']==1:
            if not st['turns'] or st['turns'][-1]['number']!=battle.turn_number:
                if st['turns']:
                    previous=st['turns'][-1];elapsed=t-previous['t']
                    check('completed window 10 seconds',9.85<=elapsed<=10.2)
                    check('teams alternate',hp(active).team()!=previous['team'])
                check('exactly one active ship',sum(battle.is_ship_active(a) for a in st['ships'])==1)
                check('window starts near 10 seconds',9.7<=battle.turn_seconds_remaining()<=10)
                for a in st['ships']:
                    if a==active:continue
                    check('inactive propulsion zero',zero(unreal.ShipMovementComponent.filter_propulsion(a,unreal.Vector(700000,0,0))))
                    check('inactive rudder zero',zero(unreal.ShipMovementComponent.filter_rudder(a,unreal.Vector(0,12000,0))))
                    shots=hp(a).shots_fired
                    check('inactive shot rejected',not hp(a).try_fire_weapon(unreal.Vector(),None,unreal.Vector(),False) and hp(a).shots_fired==shots)
                st['turns'].append(dict(number=battle.turn_number,t=t,ship=active.get_name(),team=hp(active).team(),label=battle.active_turn_label))
                save()
            if battle.turn_seconds_remaining()<.3 and st.get('shot_turn')!=battle.turn_number:
                check('active original projectile accepted',hp(active).try_fire_weapon(unreal.Vector(),None,unreal.Vector(0,0,20000),True))
                st['rounds'].append((hp(active).last_projectile,active,battle.turn_number));st['shot_turn']=battle.turn_number
            for projectile,shooter,number in list(st['rounds']):
                if number!=battle.turn_number:
                    check('existing round continues after window',valid(projectile) and projectile.get_velocity().z>10000 and not battle.is_ship_active(shooter))
                    st['projectiles'].append(dict(shot_turn=number,observed_turn=battle.turn_number,speed=projectile.get_velocity().z))
                    # This upward test round has completed its observation.
                    # Do not let it fall back onto a stationary shooter 40 s
                    # later and add an unrelated hull collision to the test.
                    projectile.destroy_actor()
                    st['rounds'].remove((projectile,shooter,number))
            if t-st['last']>=.5:
                st['last']=t
                rows=[]
                for a in st['ships']:
                    check('no unplanned ship deaths during rotations',valid(a) and not hp(a).is_destroyed)
                    body=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
                    n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                    rows.append(dict(name=a.get_name(),active=battle.is_ship_active(a),physics=body.is_simulating_physics(),z=body.get_world_location().z,water=a.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body(),speed=body.get_physics_linear_velocity().length()/100,throttle=a.get_editor_property('AIThrottle') if a!=st['player'] else None,rudder=a.get_editor_property('AIRudder') if a!=st['player'] else None,nav=str(n.navigation_state) if n else None))
                st['rows'].append(dict(t=t,number=battle.turn_number,ships=rows))
                if t-st['turns'][-1]['t']>.25:
                    check('inactive AI drive commands zero',all(abs(s['throttle'])+abs(s['rudder'])<1e-6 for s in rows if not s['active'] and s['name']!=st['player'].get_name()))
            if battle.turn_number>=19 and battle.turn_seconds_remaining()<8:
                counts={a.get_name():sum(r['ship']==a.get_name() for r in st['turns'][:18]) for a in st['ships']}
                check('three complete six-ship rotations',all(v==3 for v in counts.values()))
                check('physics never paused for turns',all(s['physics'] for row in st['rows'] for s in row['ships']))
                check('all ships physically float during turns',all(any(s['name']==a.get_name() and s['water'] for row in st['rows'] for s in row['ships']) for a in st['ships']))
                check('all five AI physically move',all(math.hypot(a.get_actor_location().x-st['positions'][a.get_name()][0],a.get_actor_location().y-st['positions'][a.get_name()][1])>2000 for a in st['ships'] if a!=st['player']))
                check('five AI controller identities preserved',all(a.get_controller()==c for a,c in zip([a for a in st['ships'] if a!=st['player']],st['controllers'])))
                st['dead']=active;st['enemy']=next(a for a in st['ships'] if hp(a).team()!=hp(active).team())
                hp(active).receive_ship_damage(100,st['enemy']);st.update(phase=2,killed_at=t,death_turn=battle.turn_number)
        elif st['phase']==2 and t-st['killed_at']>=.3:
            check('dead active ship immediately skipped',battle.active_turn_ship!=st['dead'] and battle.turn_number==st['death_turn']+1 and not battle.is_ship_active(st['dead']))
            check('sinking continues outside action window',hp(st['dead']).is_destroyed)
            st['source']=next(a for a in st['ships'] if hp(a).team()==0 and not hp(a).is_destroyed)
            for a in st['ships']:
                if hp(a).team()==1:hp(a).receive_ship_damage(100,st['source'])
            st.update(phase=3,eliminated_at=t)
        elif st['phase']==3 and t-st['eliminated_at']>=.3:
            check('team elimination ends turn battle',battle.outcome=='VICTORY' and battle.active_turn_ship is None)
            check('no actions after victory',all(not battle.is_ship_active(a) for a in st['ships'] if valid(a)))
            st['phase']=4
        elif st['phase']==4 and t-st['eliminated_at']>=19:
            check('inactive wreck sinks progressively',valid(st['dead']) and hp(st['dead']).sunk_distance_m>=64.9)
            st['phase']=5
        elif st['phase']==5 and t-st['eliminated_at']>=25:
            check('wreck cleanup continues after turns end',not valid(st['dead']) and all(not valid(a) for a in st['reds']))
            finish('PASSED')
        if t-st['started']>225:raise AssertionError('turn fixture timeout')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
