"""Production manual-map startup and physical A/B endurance; no fixture drive/pose writes.
Damage alone is disabled so all authored routes can be observed through repeated legs.
"""
import unreal,builtins,json,time,traceback,math,hashlib
from pathlib import Path
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
full_ai=bool(globals().get('FULL_AI',False));required=int(globals().get('REQUIRED_LEGS',4))
output=globals().get('OUTPUT','AUTHORED_FIVE_AI_LOOPS.json')
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
sub=lambda w,c:lib.call_method('GetWorldSubsystem',args=(w,c.static_class()))
snapshot={}
for actor in unreal.GameplayStatics.get_all_actors_of_class(ed.get_editor_world(),unreal.Pawn):
    n=actor.get_component_by_class(unreal.ShipEnemyNavigationComponent)
    if n:snapshot[actor.get_actor_label()]=dict(object_name=actor.get_name(),position=list(actor.get_actor_location().to_tuple()),a=n.patrol_actor_a.get_actor_label(),b=n.patrol_actor_b.get_actor_label(),a_pos=list(n.patrol_actor_a.get_actor_location().to_tuple()),b_pos=list(n.patrol_actor_b.get_actor_location().to_tuple()))
player_start=unreal.GameplayStatics.get_all_actors_of_class(ed.get_editor_world(),unreal.PlayerStart)[0].get_actor_location()
s=dict(status='RUNNING',phase='menu',started=time.monotonic(),ships={},rows=[],checks={},error=None,authored_snapshot=snapshot,player_start_world=list(player_start.to_tuple()),native_dll_sha256=hashlib.sha256((root/'Binaries/Win64/UnrealEditor-VDTPhase2Project.dll').read_bytes()).hexdigest())
builtins.authored_patrol_probe=s
def save():
    p=root/'Saved'/output;tmp=p.with_suffix('.json.tmp')
    tmp.write_text(json.dumps({k:v for k,v in s.items() if k not in ['handle','old_world']},indent=2),encoding='utf-8');tmp.replace(p)
def finish(status):
    s['status']=status;save();unreal.unregister_slate_post_tick_callback(s['handle'])
    if not (status=='FAILED' and globals().get('HOLD_ON_FAILURE',False)):unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    try:
        assert time.monotonic()-s['started']<float(globals().get('WALL_LIMIT_SECONDS',1800)),'Authored patrol fixture timeout'
        w=ed.get_game_world()
        if not w:return
        t=unreal.GameplayStatics.get_time_seconds(w);pc=unreal.GameplayStatics.get_player_controller(w,0)
        fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
        for a in fleet:a.can_be_damaged=False
        if s['phase']=='menu' and time.monotonic()-s['started']>2:
            if not pc.setup_widget:pc.toggle_match_setup()
            ui=pc.setup_widget
            settings=lib.call_method('GetGameInstanceSubsystem',args=(unreal.GameplayStatics.get_game_instance(w),unreal.NavalMatchSettings.static_class()))
            s['checks']['fresh_defaults_or_retained_selection']=settings.explicit_selection or (settings.hull_classes[0]==unreal.ShipHullClass.MEDIUM and settings.hull_classes[3]==unreal.ShipHullClass.MEDIUM)
            ui.use_my_map();ui.control_selector.set_selected_index(1 if full_ai else 0);ui.mode_selector.set_selected_index(0)
            player_team=int(globals().get('PLAYER_TEAM',0));ui.player_team_selector.set_selected_index(player_team)
            ui.blue_count_selector.set_selected_index(2);ui.red_count_selector.set_selected_index(2)
            ui.seed_input.set_text('-1');s['checks']['invalid_seed_rejected']=not ui.apply_selection()
            ui.seed_input.set_text('0');s['checks']['zero_seed_accepted']=ui.apply_selection()
            ui.seed_input.set_text('271828')
            if 'HULL_CLASSES' in globals():
                for selector,index in zip(ui.hull_selectors,globals()['HULL_CLASSES']):selector.set_selected_index(index)
            if globals().get('PLAYER_SMALL',False):ui.hull_selectors[player_team*3].set_selected_index(0)
            s.update(phase='startup',old_world=w);ui.start_match();save();return
        if s['phase']=='startup' and w!=s['old_world']:
            f=sub(w,unreal.NavalBattlefieldSubsystem);b=sub(w,unreal.NavalBattleSubsystem)
            if globals().get('TAG_ERROR'):
                assert full_ai and t<.1,'Tag fault must be injected before roster configuration'
                markers=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.TargetPoint)
                a=next(m for m in markers if 'Naval.PlayerPatrol.A' in [str(x) for x in m.tags])
                z=next(m for m in markers if 'Naval.PlayerPatrol.B' in [str(x) for x in m.tags])
                if globals()['TAG_ERROR'] in ['missing','same_actor']:a.tags=[x for x in a.tags if str(x)!='Naval.PlayerPatrol.A']
                if globals()['TAG_ERROR'] in ['duplicate','same_actor']:z.tags=list(z.tags)+[unreal.Name('Naval.PlayerPatrol.A')]
                s.update(phase='expected_setup_error',tag_fault=globals()['TAG_ERROR']);return
            assert not f.initialization_error,f.initialization_error
            assert t<40,'Authored startup did not settle'
            if not f.ready:return
            navs=[a for a in fleet if a.get_component_by_class(unreal.ShipEnemyNavigationComponent)]
            assert len(navs)==(6 if full_ai else 5),'Wrong authored AI roster'
            if globals().get('CHECK_INVALID_CONFIG',False):
                n=navs[0].get_component_by_class(unreal.ShipEnemyNavigationComponent);saved_b=n.patrol_actor_b
                try:
                    n.patrol_actor_b=n.patrol_actor_a
                    result=n.prepare_authored_patrol();error=n.last_navigation_failure_reason
                    assert result is None and 'distinct' in error and navs[0].get_actor_label() in error,error
                    s['checks']['invalid_pair_named_and_stopped']=n.desired_throttle==0 and n.desired_rudder==0
                finally:n.patrol_actor_b=saved_b
                result=n.prepare_authored_patrol();assert result is not None,n.last_navigation_failure_reason
            s['checks']['manual_flag']=f.using_authored_layout and not f.is_enabled()
            s['checks']['seed_retained']=f.actual_seed==271828
            if not full_ai:
                p=unreal.GameplayStatics.get_player_pawn(w,0)
                s['checks']['player_team']=p.get_component_by_class(unreal.ShipWeaponComponent).team()==int(globals().get('PLAYER_TEAM',0))
                s['checks']['player_authored_spawn']=math.dist(s['player_start_world'][:2],list(p.get_actor_location().to_tuple())[:2])<100
                if globals().get('PLAYER_SMALL',False):s['checks']['small_player_selected']=p.get_component_by_class(unreal.ShipMovementComponent).hull_class==unreal.ShipHullClass.SMALL and 'BP_ShipSmall' in p.get_class().get_name()
            for a in navs:
                n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent);d=json.loads(n.get_navigation_diagnostics());label=a.get_actor_label()
                assert n.keep_authored_patrol and not n.start_at_patrol_b and not n.patrol_circuit_actors and n.patrol_offset_a.length()<.01 and n.patrol_offset_b.length()<.01,label+' incorrect authored policy'
                assert d['active_target']==n.patrol_actor_a.get_name(),label+' did not start at A'
                if label in snapshot and a.get_name()==snapshot[label]['object_name']:
                    saved=snapshot[label]
                    assert n.patrol_actor_a.get_actor_label()==saved['a'] and n.patrol_actor_b.get_actor_label()==saved['b'],label+' marker references changed'
                    assert math.dist(saved['a_pos'],list(n.patrol_actor_a.get_actor_location().to_tuple()))<1 and math.dist(saved['b_pos'],list(n.patrol_actor_b.get_actor_location().to_tuple()))<1,label+' authored markers moved'
                    assert math.dist(saved['position'][:2],list(a.get_actor_location().to_tuple())[:2])<100,label+' authored horizontal spawn moved'
                if a.get_name()=='Demo_Blue_Flagship':
                    assert math.dist(s['player_start_world'][:2],list(a.get_actor_location().to_tuple())[:2])<100,'Full AI flagship did not preserve PlayerStart'
                s['ships'][a.get_name()]=dict(label='Blue Flagship (Full AI)' if a.get_name()=='Demo_Blue_Flagship' else label,controller=a.get_controller().get_name(),a=n.patrol_actor_a.get_name(),b=n.patrol_actor_b.get_name(),legs=0,arrivals=[],max_speed=0,max_pitch=0,max_roll=0,water_samples=0,samples=0,physical=True,marker_refs=True,controller_preserved=True,tactical_override=False)
            s.update(phase='observe',ready_at=t);save();return
        if s['phase']=='expected_setup_error':
            if t<1:return
            b=sub(w,unreal.NavalBattleSubsystem)
            assert not b.match_configured and 'MY MAP / Full AI:' in b.configuration_error,b.configuration_error
            assert len(fleet)==5 and not any(a.get_name()=='Demo_Blue_Flagship' for a in fleet),'Invalid configuration spawned an extra ship'
            for a in fleet:
                m=a.get_component_by_class(unreal.ShipMovementComponent);n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                assert m.last_propulsion_force.length()<.01 and m.last_rudder_force.length()<.01 and n.desired_throttle==0 and n.desired_rudder==0,'Invalid setup issued drive force'
            s['checks']['named_setup_error_stops_fleet_without_spawn']=True;s['configuration_error']=b.configuration_error;finish('PASSED');return
        if s['phase']!='observe' or t-s.get('last',-1)<.1:return
        s['last']=t;sample=[]
        for a in fleet:
            n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
            if not n:continue
            r=s['ships'][a.get_name()];d=json.loads(n.get_navigation_diagnostics());h=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
            assert not a.get_component_by_class(unreal.ShipWeaponComponent).is_destroyed,r['label']+' died in nondamaging endurance'
            r['physical'] &= h.is_simulating_physics();r['controller_preserved'] &= a.get_controller().get_name()==r['controller'];r['marker_refs'] &= n.patrol_actor_a.get_name()==r['a'] and n.patrol_actor_b.get_name()==r['b'];r['tactical_override'] |= n.has_tactical_destination();r['max_speed']=max(r['max_speed'],h.get_physics_linear_velocity().length()/100)
            rotation=h.get_world_rotation();water=a.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body();r['samples']+=1;r['water_samples']+=int(water);r['max_pitch']=max(r['max_pitch'],abs(rotation.pitch));r['max_roll']=max(r['max_roll'],abs(rotation.roll))
            assert abs(rotation.pitch)<5 and abs(rotation.roll)<5,r['label']+' excessive hull tilt/grounding'
            if n.patrol_legs_completed>r['legs']:
                assert n.patrol_legs_completed==r['legs']+1,'Missed arrival sample'
                arrived=n.patrol_actor_a if r['legs']%2==0 else n.patrol_actor_b
                hp=h.get_world_location();tp=arrived.get_actor_location();distance=math.hypot(hp.x-tp.x,hp.y-tp.y)/100
                assert distance<=n.arrival_radius_m+3,r['label']+' did not physically reach endpoint'
                r['arrivals'].append(dict(leg=n.patrol_legs_completed,marker=arrived.get_actor_label(),distance_m=distance,age=t-s['ready_at']))
                r['legs']=n.patrol_legs_completed
            if n.navigation_state==unreal.ShipNavigationState.FAILED:
                # A bounded recovery can deliberately stop, cool down, then retry.
                # It is still required to reach every endpoint within the fixture limit.
                assert d.get('recovery_retry_at',0)>0,r['label']+': '+n.last_navigation_failure_reason
                events=r.setdefault('recovery_events',[])
                if not events or events[-1]['retry_at']!=d['recovery_retry_at']:
                    events.append(dict(age=t-s['ready_at'],retry_at=d['recovery_retry_at'],reason=n.last_navigation_failure_reason))
            arrow=next(c for c in a.get_components_by_class(unreal.ArrowComponent) if c.get_name()=='Arrow');fwd=arrow.get_forward_vector();v=h.get_physics_linear_velocity()
            sample.append(dict(ship=r['label'],legs=r['legs'],state=str(n.navigation_state),distance=d.get('distance_to_goal_m'),speed=d.get('speed_mps'),side_speed_mps=(-fwd.y*v.x+fwd.x*v.y)/100,yaw=rotation.yaw,pitch=rotation.pitch,roll=rotation.roll,water=water,en=d.get('chart_en_m'),throttle=n.desired_throttle,traffic_wait=d.get('traffic_wait_seconds'),shots=a.get_component_by_class(unreal.ShipWeaponComponent).shots_fired))
        s['rows'].append(dict(age=t-s['ready_at'],ships=sample))
        if int(t)%5==0 and int(t)!=s.get('saved'):
            s['saved']=int(t);save()
        if t-s['ready_at']>8 and not s.get('captured'):
            s['captured']=unreal.ShipCombatLibrary.capture_gameplay_hud(w,str(root/'reports/images/authored_patrol.png'))
        if all(r['legs']>=required for r in s['ships'].values()):
            s['checks']['all_physical_loops']=all(r['physical'] and r['controller_preserved'] and r['marker_refs'] and not r['tactical_override'] and r['max_speed']>3 and r['water_samples']/r['samples']>=.98 for r in s['ships'].values())
            assert all(s['checks'].values()),s['checks'];s['duration']=t-s['ready_at'];finish('PASSED')
    except Exception:s['error']=traceback.format_exc();finish('FAILED')
s['handle']=unreal.register_slate_post_tick_callback(tick);save();unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
