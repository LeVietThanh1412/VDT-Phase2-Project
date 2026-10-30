"""Real seeded convoy movement; optional explicit leader-health-death stage.

No transforms, forces, velocities or direct steering are applied by this fixture.
Combat is disabled only here to separate mission movement from damage outcomes.
"""
import builtins,json,time,traceback,math,hashlib
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
follower_loss=bool(globals().get('FOLLOWER_LOSS',False));failover=bool(globals().get('FAILOVER',False)) or follower_loss;output='RED_ASSAULT_FOLLOWER_LOSS.json' if follower_loss else 'RED_ASSAULT_LEADER_LOSS.json' if failover else 'RED_ASSAULT_NAVIGATION.json'
scope=dict(SEED=20261028,ENABLE_TACTICS=True,ENABLE_ASSAULT=True,OBSERVE_SECONDS=99999,WALL_LIMIT_SECONDS=900,OUTPUT='RED_ASSAULT_STARTUP_TRANSIENT.json',OUTPUT_DIR='Saved',__name__='red_assault_startup')
p=root/'Tools/NavalCombat/test_seeded_spawn.py';exec(compile(p.read_text(encoding='utf-8'),str(p),'exec'),scope)
base=scope['st'];st=dict(status='RUNNING',scope='Combat-disabled physical convoy mission; any staged health death is explicit',failover=failover,phase='startup',rows=[],events=[],ships={},checks={},started=time.monotonic(),last=-1,error=None)
builtins.red_assault_probe=st
def vec(v):return list(v.to_tuple())
def save():
    (root/'Docs/NavalCombat'/output).write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','red','field','grid','player','dead_leader']},indent=2),encoding='utf-8')
def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle'])
    if base.get('status')=='RUNNING':unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_ASSAULT_FIXTURE'
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    try:
        if base.get('status')=='FAILED':raise AssertionError('Seed startup failed: '+str(base.get('error')))
        w=ed.get_game_world()
        if not w:return
        t=unreal.GameplayStatics.get_time_seconds(w)
        if st['phase']=='startup':
            if base.get('phase')!='observe':return
            field=scope['sub'](w,unreal.NavalBattlefieldSubsystem);grid=scope['sub'](w,unreal.NavalSectorSubsystem)
            red=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if (weapon:=a.get_component_by_class(unreal.ShipWeaponComponent)) and weapon.team()==1]
            st.update(red=red,field=field,grid=grid,player=unreal.GameplayStatics.get_player_pawn(w,0),at=t,phase='assault')
            for a in red:
                n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                st['ships'][a.get_actor_label()]=dict(controller=a.get_controller().get_path_name(),start=vec(grid.world_to_chart(a.get_actor_location())),departure_at=None,max_displacement_m=0,min_east_m=1e9,max_phase=0,physical=True,controller_same=True,patrol_refs=[n.patrol_actor_a.get_path_name(),n.patrol_actor_b.get_path_name()],goal_arrivals=[])
            save();return
        field=st['field'];grid=st['grid'];red=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_actor_label() in st['ships']];layout=json.loads(field.layout_report())
        if t-st['last']<.5:return
        st['last']=t;sample=[]
        for a in red:
            label=a.get_actor_label()
            if not label or label not in st['ships']:continue
            r=st['ships'][label]
            weapon=a.get_component_by_class(unreal.ShipWeaponComponent)
            if weapon.is_destroyed:continue
            n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent);h=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
            assignment=next(x for x in field.assignments if x.ship==a);pos=grid.world_to_chart(a.get_actor_location());distance=math.dist(r['start'],vec(pos))
            r['physical'] &= h.is_simulating_physics();r['controller_same'] &= bool(a.get_controller() and a.get_controller().get_path_name()==r['controller'])
            assert [n.patrol_actor_a.get_path_name(),n.patrol_actor_b.get_path_name()]==r['patrol_refs'],'AI patrol actor identities changed'
            r['max_displacement_m']=max(r['max_displacement_m'],distance);r['min_east_m']=min(r['min_east_m'],pos.x);r['max_phase']=max(r['max_phase'],assignment.assault_phase)
            r['last_position']=vec(pos);r['slot']=assignment.slot;r['released_at']=assignment.assault_released_at
            if r['departure_at'] is None and distance>20:r['departure_at']=t
            prior=r.get('previous_goal')
            if prior and assignment.assault_goals_reached>r.get('last_goals',0):
                error=math.dist(vec(pos),prior);r['goal_arrivals'].append(dict(t=t,prior_goal_en_m=prior,distance_m=error,accepted_radius_m=n.arrival_radius_m+3,valid=error<=n.arrival_radius_m+3));st['events'].append(dict(t=t,ship=label,phase=assignment.assault_phase,event='physical_goal_arrival',distance_m=error))
            r['previous_goal']=vec(assignment.mission_goal_en);r['last_goals']=assignment.assault_goals_reached
            sample.append(dict(label=label,en_m=vec(pos),phase=assignment.assault_phase,goals_reached=assignment.assault_goals_reached,mission=assignment.mission_state,formation_distance_m=assignment.formation_distance_m,released=assignment.assault_released,cruise_mps=n.cruise_speed_mps,navigation=json.loads(n.get_navigation_diagnostics())))
        st['rows'].append(dict(t=t,leader=layout['assault_leader'],leader_changes=layout['leader_changes'],ships=sample));save()
        if 'initial_leader' not in st and layout['assault_leader']!='none':
            st['initial_leader']=layout['assault_leader'];leader=next(a for a in red if a.get_actor_label()==layout['assault_leader'])
            st['checks']['initial_scout_is_small']=leader.get_component_by_class(unreal.ShipMovementComponent).hull_class==unreal.ShipHullClass.SMALL
        if failover and st['phase']=='assault':
            leader=next((a for a in red if a.get_actor_label()==layout['assault_leader']),None)
            if leader and next(x for x in field.assignments if x.ship==leader).assault_phase>=1:
                st['leader_changes_before_death']=layout['leader_changes']
                if follower_loss:leader=next(a for a in red if next(x for x in field.assignments if x.ship==a).slot==1)
                st['dead_leader_label']=leader.get_actor_label();st['dead_leader']=leader
                st['survivor_positions']={a.get_actor_label():vec(grid.world_to_chart(a.get_actor_location())) for a in red if a!=leader}
                leader.can_be_damaged=True;st['checks']['staged_hostile_health_death_accepted']=leader.get_component_by_class(unreal.ShipWeaponComponent).receive_ship_damage(1000,st['player']);leader.can_be_damaged=False
                st.update(phase='leader_loss',death_at=t);st['events'].append(dict(t=t,event='explicit_leader_health_death',ship=leader.get_actor_label()));save()
        if failover and st['phase']=='leader_loss' and t-st['death_at']>90:
            survivors=[r for label,r in st['ships'].items() if label!=st['dead_leader_label']]
            progressed=all(math.dist(st['survivor_positions'][label],r.get('last_position',r['start']))>100 for label,r in st['ships'].items() if label!=st['dead_leader_label'])
            leader_ok=(layout['assault_leader']==st['initial_leader'] and layout['leader_changes']==st['leader_changes_before_death']) if follower_loss else layout['leader_changes']>st['leader_changes_before_death']
            if progressed and leader_ok and layout['assault_leader'] not in ['none',st['dead_leader_label']]:
                st['checks']['live_leader_retained_after_follower_death' if follower_loss else 'new_live_leader_selected']=True
                st['checks'].update(both_survivors_physically_continue=True,original_survivor_controllers=all(r['controller_same'] and r['physical'] for r in survivors))
                finish('PASSED' if all(st['checks'].values()) else 'FAILED');return
        if not failover and all(r['max_phase']>=3 for r in st['ships'].values()):
            if 'all_contact_at' not in st:st['all_contact_at']=t
            if t-st['all_contact_at']>3:
                lead=st['ships'][st['initial_leader']];followers=sorted([r for k,r in st['ships'].items() if k!=st['initial_leader']],key=lambda r:r['slot'])
                st['checks']['sequential_release_intervals']=all(b['released_at']-a['released_at']>=field.formation_departure_interval_seconds-.05 for a,b in zip([lead]+followers,followers))
                st['checks']['physical_departures_sequential']=all(b['departure_at'] and a['departure_at'] and b['departure_at']>a['departure_at'] for a,b in zip([lead]+followers,followers))
                st['checks'].update(scout_departs_before_followers=lead['departure_at'] is not None and all(r['departure_at'] and r['departure_at']>lead['departure_at'] for r in followers),all_three_reach_contact_and_maneuver=True,
                    all_three_enter_blue_contact_water=all(r['min_east_m']<=14600 for r in st['ships'].values()),all_three_physically_travel=all(r['max_displacement_m']>300 for r in st['ships'].values()),
                    physical_arrival_before_each_phase=all(len(r['goal_arrivals'])>=3 and all(e['valid'] for e in r['goal_arrivals']) for r in st['ships'].values()),
                    independent_controllers_and_physics=all(r['controller_same'] and r['physical'] for r in st['ships'].values()),
                    final_routes_not_permanently_idle=all(s['navigation']['navigation_state'] not in ['EShipNavigationState::Idle','EShipNavigationState::Failed'] for s in sample))
                finish('PASSED' if all(st['checks'].values()) else 'FAILED');return
        if t-st['at']>650 or time.monotonic()-st['started']>800:raise AssertionError('Convoy mission/recovery acceptance time bound')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick);save()
