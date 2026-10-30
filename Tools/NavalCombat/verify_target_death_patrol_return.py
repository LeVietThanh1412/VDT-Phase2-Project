"""Current shared-combat pursuit -> hostile health death -> physical patrol return."""
import builtins,json,math,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
helpers=(root/'Tools/NavalCombat/test_roles.py').read_text(encoding='utf-8').split('def tick(dt):')[0]
exec(helpers)
OUTPUT='NAVIGATION_TARGET_DEATH_RETURN.json'
st=dict(status='RUNNING',phase='init',rows=[],checks=[],last=-1,at=0,error=None)
builtins.target_death_patrol_return=st

def tick(dt):
    w=ed.get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<1:return
    try:
        if st['phase']=='init':
            fleet=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn)
            subject=next(a for a in fleet if a.get_actor_label()=='Friendly_AI_1')
            threat=next(a for a in fleet if a.get_actor_label()=='BP_ShipEnemy')
            st.update(subject=subject,threat=threat)
            for a in fleet:
                if not a.get_component_by_class(unreal.ShipWeaponComponent):continue
                a.can_be_damaged=False
                if (c:=a.get_component_by_class(unreal.ShipCombatComponent)):c.enable_combat=a==subject
                if a!=subject:
                    if (n:=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)):n.stop_navigation()
                    hull(a).set_simulate_physics(False)
                    if a!=threat:a.set_actor_location(a.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
            c=subject.get_component_by_class(unreal.ShipCombatComponent);n=subject.get_component_by_class(unreal.ShipEnemyNavigationComponent)
            c.enable_hit_and_run=False;c.engagement_radius_m=300;c.hunter_max_pursuit_seconds=120
            place(threat,1400);c.set_combat_role(unreal.ShipEnemyRole.HUNTER)
            st.update(phase='pursuit',start=metric(subject),at=t,a=n.patrol_actor_a.get_path_name(),b=n.patrol_actor_b.get_path_name(),controller=subject.get_controller().get_path_name())
        a=st['subject'];n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent);c=a.get_component_by_class(unreal.ShipCombatComponent)
        if t-st['last']>.5:
            st['last']=t;st['rows'].append(dict(t=t,en_m=metric(a),phase=st['phase'],tactic=str(c.tactical_state),tactical_override=n.has_tactical_destination(),resumed=c.patrols_resumed,diagnostics=json.loads(n.get_navigation_diagnostics())))
            save()
        if st['phase']=='pursuit' and t-st['at']>=12:
            check('real geographic pursuit moved subject',c.hunter_routes_started>0 and math.dist(st['start'],metric(a))>20)
            threat=st['threat'];threat.can_be_damaged=True;weapon=threat.get_component_by_class(unreal.ShipWeaponComponent)
            check('hostile health pipeline destroys target',weapon.receive_ship_damage(weapon.current_health+1,a) and weapon.is_destroyed)
            st.update(phase='dead_target',at=t,death_position=metric(a))
        elif st['phase']=='dead_target' and t-st['at']>=13:
            check('dead target releases combat navigation',not n.has_tactical_destination() and c.patrols_resumed>=1)
            check('original A/B preserved',n.patrol_actor_a.get_path_name()==st['a'] and n.patrol_actor_b.get_path_name()==st['b'])
            check('physical movement continues after target death',math.dist(st['death_position'],metric(a))>20)
            check('same independent AI controller',a.get_controller().get_path_name()==st['controller'] and not a.is_player_controlled())
            check('patrol route valid after target death',n.pathfinding_succeeded and not n.last_navigation_failure_reason)
            st.pop('controller',None);finish('PASSED')
        elif t>60:raise AssertionError('Target-death return fixture timeout')
    except Exception:st['error']=traceback.format_exc();st.pop('controller',None);finish('FAILED')

st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
