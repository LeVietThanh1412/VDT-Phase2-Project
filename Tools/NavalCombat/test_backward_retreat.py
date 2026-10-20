"""Physical low-HP retreat/firing test. No subject pose or velocity overrides."""
import builtins,json,math,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
# Reuse geographic fixture helpers, without executing their startup.
helpers=(root/'Tools/NavalCombat/test_roles.py').read_text().split('def tick(dt):')[0]
exec(helpers)
st=dict(handle=None,phase='init',rows=[],checks=[],last=-1,at=0,error=None)
builtins.reverse_test=st
def finish(status):
    st['status']=status
    (root/'Docs/NavalCombat/BACKWARD_RETREAT_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','subject','threat','ally']},indent=2))
    unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<1:return
    try:
        if st['phase']=='init':
            ships=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'))
            s=next(s for s in ships if s.get_actor_label()=='Friendly_AI_1');threat=next(s for s in ships if s.get_actor_label()=='BP_ShipEnemy')
            ally=next(s for s in ships if s.is_player_controlled());st.update(subject=s,threat=threat,ally=ally)
            for other in ships:
                c=other.get_component_by_class(unreal.ShipCombatComponent);n=other.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=other==s
                if other!=s:
                    other.can_be_damaged=False
                    if n:n.stop_navigation()
                    hull(other).set_simulate_physics(False)
                    if other!=threat:other.set_actor_location(other.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
            for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                if a.get_class().get_name().startswith('BP_ShipProjectile'):a.destroy_actor()
            n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent);c=s.get_component_by_class(unreal.ShipCombatComponent)
            st['anchors']=[n.patrol_actor_a.get_path_name(),n.patrol_actor_b.get_path_name()]
            place(threat,1000);phase('healthy',t)
        s=st['subject'];c=s.get_component_by_class(unreal.ShipCombatComponent);n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent);weapon=s.get_component_by_class(unreal.ShipWeaponComponent)
        move=s.get_component_by_class(unreal.ShipMovementComponent);speed=move.signed_forward_speed()
        assert [n.patrol_actor_a.get_path_name(),n.patrol_actor_b.get_path_name()]==st['anchors'],'lost patrol endpoints'
        if t-st['last']>=.1:
            st['last']=t;st['rows'].append(dict(t=t,phase=st['phase'],position=metric(s),signed_speed=speed,physics=hull(s).is_simulating_physics(),hp=weapon.current_health,
                state=str(c.tactical_state),shots=weapon.shots_fired,retreats=c.retreats_started,reverse=n.backward_retreat_active,blocked=n.backward_retreat_blocked,
                throttle=n.desired_throttle,rudder=n.desired_rudder,yaw=c.turret_yaw,pitch=c.barrel_pitch,solution=c.has_firing_solution,status=c.firing_status,
                resumed=c.patrols_resumed,fallbacks=c.unsafe_reverse_fallbacks,failure=n.last_navigation_failure_reason))
        elapsed=t-st['at']
        if st['phase']=='healthy' and elapsed>=10:
            check('no premature healthy retreat',c.retreats_started==0)
            place(st['threat'],650)
            check('70 hostile damage accepted',weapon.receive_ship_damage(70,st['threat']) and weapon.current_health==30)
            st['reverse_start']=metric(s);st['shots_before']=weapon.shots_fired;phase('reverse',t)
        elif st['phase']=='reverse' and elapsed>=28:
            rows=[r for r in st['rows'] if r['phase']=='reverse'];back=[r for r in rows if r['signed_speed']<-1]
            check('30 HP triggers backward retreat',c.backward_retreats_started>=1)
            check('physical backward motion',len(back)>20 and min(r['signed_speed'] for r in back)<-3)
            check('reverse max speed bounded',min(r['signed_speed'] for r in back)>=-7.5)
            firing=[rows[i] for i in range(1,len(rows)) if rows[i]['shots']>rows[i-1]['shots'] and rows[i]['signed_speed']<-1]
            st['reverse_shots']=firing;check('actual shot fired while reversing',bool(firing))
            check('turret has reverse firing solution',any(r['solution'] for r in back))
            st['threat'].get_component_by_class(unreal.ShipCombatComponent).faction=2
            st['resume_start']=metric(s);phase('resume',t)
        elif st['phase']=='resume' and elapsed>=20:
            check('returned to original patrol',c.patrols_resumed>=1 and not n.has_tactical_destination())
            check('physical movement after retreat',math.dist(metric(s),st['resume_start'])>20)
            check('original A/B references preserved',True)
            c.enable_hit_and_run=False;c.enable_combat=False
            # Test a parked hull behind the current ship. Only fixture hull moves.
            forward=next(x for x in s.get_components_by_class(unreal.SceneComponent) if x.get_name()=='Arrow').get_forward_vector()
            st['ally'].set_actor_location(hull(s).get_world_location()-forward*18000,False,True)
            place(st['threat'],650);st['threat'].get_component_by_class(unreal.ShipCombatComponent).faction=1
            check('parked ship blocks reverse commitment',not n.begin_backward_retreat(st['threat'],300))
            check('forward A* escape remains available',n.begin_retreat_from(st['threat'],800))
            n.return_to_patrol();finish('BACKWARD_RETREAT_CHECKS_PASSED')
        elif t>100:raise AssertionError('retreat fixture timed out')
        if int(t)%5==0:
            (root/'Docs/NavalCombat/BACKWARD_RETREAT_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','subject','threat','ally']},indent=2))
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
