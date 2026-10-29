"""Staged physical incoming threats exercise bounded Blue interception/return.

All ships retain physics and controllers. Red uses the normal native geographic
destination API; this fixture never positions or drives a ship directly. Health
damage/death stages are explicit fixture actions, not natural battle evidence.
"""
import builtins,json,time,traceback,math
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
scope=dict(SEED=20261028,ENABLE_TACTICS=True,COMBAT_TEAMS=[0],OBSERVE_SECONDS=99999,WALL_LIMIT_SECONDS=750,OUTPUT='BLUE_DEFENSE_STARTUP_TRANSIENT.json',__name__='blue_defense_startup')
p=root/'Tools/NavalCombat/test_seeded_spawn.py';exec(compile(p.read_text(encoding='utf-8'),str(p),'exec'),scope)
base=scope['st'];st=dict(status='RUNNING',phase='startup',rows=[],checks={},started=time.monotonic(),error=None,last=-1)
builtins.blue_defense_probe=st
def sub(w,cls):return scope['sub'](w,cls)
def vec(v):return list(v.to_tuple())
def save():
    (root/'Docs/NavalCombat/BLUE_DEFENSE_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','blue','red','field','grid']},indent=2),encoding='utf-8')
def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle'])
    if base.get('status')=='RUNNING':unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_DEFENSE_FIXTURE'
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    try:
        if base.get('status')=='FAILED':raise AssertionError('Seed startup failed: '+str(base.get('error')))
        w=ed.get_game_world()
        if not w:return
        t=unreal.GameplayStatics.get_time_seconds(w)
        if st['phase']=='startup':
            if base.get('phase')!='observe':return
            field=sub(w,unreal.NavalBattlefieldSubsystem);grid=sub(w,unreal.NavalSectorSubsystem)
            fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
            blue=[a for a in fleet if a.get_component_by_class(unreal.ShipEnemyNavigationComponent) and a.get_component_by_class(unreal.ShipWeaponComponent).team()==0]
            red=[a for a in fleet if a.get_component_by_class(unreal.ShipWeaponComponent).team()==1]
            red.sort(key=lambda a:0 if a.get_component_by_class(unreal.ShipMovementComponent).hull_class==unreal.ShipHullClass.SMALL else 1)
            st.update(blue=blue,red=red,field=field,grid=grid,at=t,phase='incoming')
            st['controllers']={a.get_actor_label():a.get_controller().get_path_name() for a in fleet}
            st['patrol_refs']={a.get_actor_label():[a.get_component_by_class(unreal.ShipEnemyNavigationComponent).patrol_actor_a.get_path_name(),a.get_component_by_class(unreal.ShipEnemyNavigationComponent).patrol_actor_b.get_path_name()] for a in blue}
            goals=[unreal.Vector2D(14600,4300),unreal.Vector2D(14150,3270)]
            st['incoming_goals_en_m']={}
            for enemy,goal in zip(red,goals):
                n=enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                candidates=sorted([sample for sector in grid.sectors if sector.navigable for sample in sector.samples_en],key=lambda p:(p-goal).length())
                accepted=None
                for candidate in candidates[:120]:
                    llh=n.world_to_geographic(grid.chart_to_world(candidate))
                    if n.begin_tactical_move_to(llh):
                        n.return_to_patrol();n.set_geographic_destination(llh);accepted=candidate;break
                assert accepted is not None,'No verified physical incoming route for '+enemy.get_actor_label()
                st['incoming_goals_en_m'][enemy.get_actor_label()]=vec(accepted)
            save();return
        field=st['field'];grid=st['grid'];blue=st['blue'];red=st['red']
        if t-st['last']>=.5:
            st['last']=t;rows=[]
            for a in blue:
                c=a.get_component_by_class(unreal.ShipCombatComponent);n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent);weapon=a.get_component_by_class(unreal.ShipWeaponComponent)
                assignment=next(x for x in field.assignments if x.ship==a)
                rows.append(dict(label=a.get_actor_label(),en_m=vec(grid.world_to_chart(a.get_actor_location())),mission=assignment.mission_state,interceptions=assignment.interceptions_started,returns=assignment.defensive_returns,defense_distance_m=(grid.world_to_chart(a.get_actor_location())-assignment.defensive_anchor_en).length(),tactic=str(c.tactical_state),retreats=c.retreats_started,shots=weapon.shots_fired,health=weapon.current_health,target_distance_m=c.distance_to_target_m,target=c.target.get_actor_label() if c.target else None,navigation=json.loads(n.get_navigation_diagnostics())))
                assert a.get_controller().get_path_name()==st['controllers'][a.get_actor_label()]
                assert [n.patrol_actor_a.get_path_name(),n.patrol_actor_b.get_path_name()]==st['patrol_refs'][a.get_actor_label()]
                assert next(h for h in a.get_components_by_class(unreal.PrimitiveComponent) if h.get_name()=='ShipSM').is_simulating_physics()
            st['rows'].append(dict(t=t,phase=st['phase'],ships=rows,field=json.loads(field.layout_report())));save()
        if st['phase']=='incoming':
            assignments=[x for x in field.assignments if x.team==0 and x.slot>0]
            if all(a.interceptions_started>0 for a in assignments):
                st['checks']['both_defenders_intercept']=True;st['phase']='damage';st['at']=t
                subject=blue[0];weapon=subject.get_component_by_class(unreal.ShipWeaponComponent);subject.can_be_damaged=True
                st['retreats_before_staged_damage']=subject.get_component_by_class(unreal.ShipCombatComponent).retreats_started
                st['checks']['staged_hostile_health_damage_accepted']=weapon.receive_ship_damage(weapon.current_health-weapon.max_health*.3,red[0]);subject.can_be_damaged=False
        elif st['phase']=='damage':
            if blue[0].get_component_by_class(unreal.ShipCombatComponent).retreats_started>st['retreats_before_staged_damage']:
                st['checks']['shared_low_health_retreat']=True
                for enemy in red[:2]:
                    enemy.can_be_damaged=True;enemy.get_component_by_class(unreal.ShipWeaponComponent).receive_ship_damage(1000,blue[1]);enemy.can_be_damaged=False
                st['checks']['staged_target_health_deaths']=all(a.get_component_by_class(unreal.ShipWeaponComponent).is_destroyed for a in red[:2])
                st['phase']='return';st['at']=t;st['return_positions']={a.get_actor_label():vec(grid.world_to_chart(a.get_actor_location())) for a in blue}
        elif st['phase']=='return' and t-st['at']>45:
            assignments=[x for x in field.assignments if x.team==0 and x.slot>0]
            returned=all(not a.get_component_by_class(unreal.ShipEnemyNavigationComponent).has_tactical_destination() and a.get_component_by_class(unreal.ShipEnemyNavigationComponent).pathfinding_succeeded for a in blue)
            moved=all(math.dist(st['return_positions'][a.get_actor_label()],vec(grid.world_to_chart(a.get_actor_location())))>25 for a in blue)
            if t-st['at']<120 and not (returned and moved):return
            st['checks'].update(original_controllers_and_patrol_refs=True,continuously_physical=True,
                no_extended_red_staging_pursuit=all(s['en_m'][0]<14850 and s['defense_distance_m']<=field.defensive_leash_m+250 for row in st['rows'] for s in row['ships'] if 'Retreat' not in s['tactic'] and 'Escape' not in s['tactic']),
                both_resume_original_patrol=returned,
                physical_travel_after_disengagement=moved,
                shared_gun_fired=any(a.get_component_by_class(unreal.ShipWeaponComponent).shots_fired>0 for a in blue))
            finish('PASSED' if all(st['checks'].values()) else 'FAILED');return
        if time.monotonic()-st['started']>500:raise AssertionError('Blue defense physical fixture bound')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick);save()
