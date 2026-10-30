"""Real menu/travel/physical initialization for one sector battlefield seed.

Only combat is disabled in the fixture. Placement/validation belongs entirely
to the native authority; this script never positions or drives a ship.
"""
import builtins,json,time,math,traceback,hashlib
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
seed=globals().get('SEED',20261028);name=globals().get('OUTPUT','SEEDED_SPAWN_'+str(seed)+'.json')
st=dict(status='RUNNING',seed=seed,phase='menu',samples=[],error=None,started=time.monotonic())
builtins.seeded_spawn_probe=st
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))

def sub(w,cls):return lib.call_method('GetWorldSubsystem',args=(w,cls.static_class()))
def save():
    (root/globals().get('OUTPUT_DIR','Docs/QuangNinhIntegration')/name).write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','old_world']},indent=2),encoding='utf-8')
def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle'])
    if not (status=='FAILED' and globals().get('HOLD_FAILURE',False)):
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()

def tick(dt):
    w=ed.get_game_world()
    if not w:return
    try:
        t=unreal.GameplayStatics.get_time_seconds(w)
        for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn):
            if (c:=a.get_component_by_class(unreal.ShipCombatComponent)):
                weapon=a.get_component_by_class(unreal.ShipWeaponComponent)
                c.enable_combat=bool(weapon and not weapon.is_destroyed and weapon.team() in globals().get('COMBAT_TEAMS',[]))
            if (weapon:=a.get_component_by_class(unreal.ShipWeaponComponent)):a.can_be_damaged=bool(globals().get('ALLOW_DAMAGE',False)) and not weapon.is_destroyed
            if globals().get('DEBUG',False) and (n:=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)):n.draw_debug=True
        if st['phase']=='menu' and time.monotonic()-st['started']>2:
            gi=unreal.GameplayStatics.get_game_instance(w)
            settings=lib.call_method('GetGameInstanceSubsystem',args=(gi,unreal.NavalMatchSettings.static_class()))
            if globals().get('VERIFY_DEFAULT_FLAGS',False):
                assert settings.use_sector_battlefield and settings.enable_sector_tactics and settings.enable_red_assault,'Production scenario flags are disabled'
                st['default_scenario_flags_verified']=True
            settings.use_sector_battlefield=True
            settings.enable_sector_tactics=bool(globals().get('ENABLE_TACTICS',False))
            settings.enable_red_assault=bool(globals().get('ENABLE_ASSAULT',False))
            pc=unreal.GameplayStatics.get_player_controller(w,0)
            if not pc.setup_widget:pc.toggle_match_setup()
            ui=pc.setup_widget
            ui.player_team_selector.set_selected_index(int(globals().get('PLAYER_TEAM',0)))
            ui.seed_input.set_text(str(seed));ui.mode_selector.set_selected_index(0)
            if 'HULL_CLASSES' in globals():
                assert len(HULL_CLASSES)==6
                for selector,index in zip(ui.hull_selectors,HULL_CLASSES):selector.set_selected_index(index)
            st.update(old_world=w,phase='initialization');ui.start_match();save();return
        if st['phase']=='initialization' and w!=st['old_world']:
            field=sub(w,unreal.NavalBattlefieldSubsystem);grid=sub(w,unreal.NavalSectorSubsystem)
            fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
            if 'controllers' not in st and len(fleet)==6:
                st['controllers']={a.get_actor_label():a.get_controller().get_path_name() if a.get_controller() else None for a in fleet}
            if int(t)!=st.get('last',-1):
                st['last']=int(t);st['samples'].append(dict(t=t,status=field.status,ready=field.ready,error=field.initialization_error));save()
            assert not field.initialization_error,field.initialization_error
            if field.ready:
                st.update(phase='observe',ready_at=t,layout=json.loads(field.layout_report()),initial_positions={a.get_actor_label():list(grid.world_to_chart(a.get_actor_location()).to_tuple()) for a in fleet})
                save()
        if st['phase']=='observe':
            field=sub(w,unreal.NavalBattlefieldSubsystem);grid=sub(w,unreal.NavalSectorSubsystem)
            required_legs=int(globals().get('REQUIRED_LEGS',0))
            if required_legs:
                records=st.setdefault('patrol_tracking',{})
                for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn):
                    n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                    if not n:continue
                    label=a.get_actor_label();r=records.setdefault(label,dict(last_legs=0,arrivals=[],physical=True,controller_same=True))
                    h=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
                    r['physical'] &= h.is_simulating_physics()
                    r['controller_same'] &= bool(a.get_controller() and a.get_controller().get_path_name()==st['controllers'][label])
                    if n.patrol_legs_completed>r['last_legs']:
                        target=n.patrol_actor_b if r['last_legs']%2==0 else n.patrol_actor_a
                        error=(grid.world_to_chart(h.get_world_location())-grid.world_to_chart(target.get_actor_location())).length()
                        r['arrivals'].append(dict(t=t,endpoint='B' if r['last_legs']%2==0 else 'A',distance_m=error,speed_mps=h.get_physics_linear_velocity().length()/100,within_arrival_radius=error<=n.arrival_radius_m+2))
                        r['last_legs']=n.patrol_legs_completed
                if t-st['ready_at']<float(globals().get('LOOP_LIMIT_SECONDS',600)) and not (len(records)==5 and all(r['last_legs']>=required_legs for r in records.values())):return
                if all(r['last_legs']>=required_legs for r in records.values()):
                    # A native arrival intentionally clears its finished route.
                    # Observe the following real planning ticks before checking
                    # that every navigator has a valid continuing route.
                    if 'all_loops_reached_at' not in st:
                        st['all_loops_reached_at']=t;save();return
                    if t-st['all_loops_reached_at']<3:return
                st['patrol_duration_seconds']=t-st['ready_at']
            if globals().get('DEBUG',False) and int(t)!=st.get('debug_second',-1):
                st['debug_second']=int(t)
                st.setdefault('debug',[]).append(dict(t=t,ships=[json.loads(a.get_component_by_class(unreal.ShipEnemyNavigationComponent).get_navigation_diagnostics()) for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipEnemyNavigationComponent)]));save()
            if globals().get('DEBUG',False) and t>12 and 'hull_capture' not in st:
                rows=[]
                fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
                ignore=fleet+list(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.WaterBody))
                ocean=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.WaterBodyOcean)[0].get_water_body_component()
                for a in fleet:
                    h=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM');p=h.get_world_location();g=a.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
                    hit=unreal.SystemLibrary.line_trace_single(w,p+unreal.Vector(0,0,4000),p-unreal.Vector(0,0,8000),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignore,unreal.DrawDebugTrace.NONE,True)
                    row=dict(ship=a.get_actor_label(),root=list(p.to_tuple()),llh=list(g.transform_unreal_position_to_longitude_latitude_height(p).to_tuple()),rotation=list(h.get_world_rotation().to_tuple()),bounds=[list(v.to_tuple()) for v in h.get_local_bounds()],velocity=list(h.get_physics_linear_velocity().to_tuple()),water=str(ocean.get_water_surface_info_at_location(p,False)))
                    if hit:
                        ht=hit.to_tuple();c=ht[10];row['trace']=dict(point=list(ht[5].to_tuple()),llh=list(g.transform_unreal_position_to_longitude_latitude_height(ht[5]).to_tuple()),component=c.get_path_name() if c else None)
                        if isinstance(c,unreal.ProceduralMeshComponent):
                            verts,tris,*_=unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(c,0);tr=c.get_world_transform();llh=[g.transform_unreal_position_to_longitude_latitude_height(tr.transform_location(v)) for v in verts]
                            (root/'Saved'/('SeedCollision_'+a.get_actor_label()+'.json')).write_text(json.dumps(dict(vertices_llh=[list(v.to_tuple()) for v in llh],triangles=list(tris))))
                    rows.append(row)
                st['hull_capture']=rows;save()
            if t-st['ready_at']<float(globals().get('OBSERVE_SECONDS',35)):return
            fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
            # A wave can briefly uncover a pontoon; capture an actual all-wet
            # sample within a bounded2s window, never force contact or retry indefinitely.
            if not all(a.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body() for a in fleet):
                if 'contact_sample_deadline' not in st:st['contact_sample_deadline']=t+2
                if t<st['contact_sample_deadline']:return
            st['contact_sample_time']=t
            ai=[a for a in fleet if a.get_component_by_class(unreal.ShipEnemyNavigationComponent)]
            assignments=st['layout']['ships'];positions=[a['spawn_en_m'] for a in assignments]
            canonical=[{k:v for k,v in a.items() if k in ['team','slot','sector','spawn_en_m','target_en_m','heading']} for a in assignments]
            st['layout_sha256']=hashlib.sha256(json.dumps(canonical,sort_keys=True,separators=(',',':')).encode()).hexdigest()
            st['minimum_spawn_separation_m']=min(math.dist(a,b) for i,a in enumerate(positions) for b in positions[i+1:])
            st['checks']=dict(seed_from_menu=field.actual_seed==seed,exact_six_ship_roster=len(fleet)==6 and len(ai)==5,
                one_player=sum(a.is_player_controlled() for a in fleet)==1,
                controllers_preserved=all(a.get_controller() and a.get_controller().get_path_name()==st['controllers'][a.get_actor_label()] for a in fleet),
                blue_left_red_right=all((a['spawn_en_m'][0]<14500 if a['team']==0 else a['spawn_en_m'][0]>14500) for a in assignments),
                red_upper_right=all(a['spawn_en_m'][0]>=14800 and a['spawn_en_m'][1]>=4000 for a in assignments if a['team']==1),
                actual_physical_clearance=all(a['physical_clearance'] for a in assignments),
                safe_initial_separation=st['minimum_spawn_separation_m']>=220,
                all_in_water=all(a.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body() for a in fleet),
                all_physical=all(next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM').is_simulating_physics() for a in fleet),
                five_native_initial_routes=all(a.get_component_by_class(unreal.ShipEnemyNavigationComponent).pathfinding_succeeded for a in ai),
                five_correct_controllers=all(a.get_controller() and not a.is_player_controlled() for a in ai),
                all_within_field=all(grid.is_inside_field(grid.world_to_chart(a.get_actor_location()),0) for a in fleet))
            st['checks']['all_five_use_physical_navigation_overlay']=all(a.get_component_by_class(unreal.ShipEnemyNavigationComponent).using_operational_chart for a in ai)
            st['checks']['all_five_actual_goals_match_live_patrol_markers']=all(min((n.geographic_to_world(n.current_final_destination)-p.get_actor_location()).length() for p in [n.patrol_actor_a,n.patrol_actor_b])<.1 for a in ai for n in [a.get_component_by_class(unreal.ShipEnemyNavigationComponent)])
            st['navigation_roles']={a.get_actor_label():str(a.get_component_by_class(unreal.ShipEnemyNavigationComponent).role) for a in ai}
            st['hull_classes']={a.get_actor_label():str(a.get_component_by_class(unreal.ShipMovementComponent).hull_class) for a in fleet}
            st['ai_diagnostics']=[json.loads(a.get_component_by_class(unreal.ShipEnemyNavigationComponent).get_navigation_diagnostics()) for a in ai]
            st['travel_m']={a.get_actor_label():math.dist(st['initial_positions'][a.get_actor_label()],grid.world_to_chart(a.get_actor_location()).to_tuple()) for a in ai}
            st['checks']['all_five_physically_moved']=all(d>35 for d in st['travel_m'].values())
            player_labels={a.get_actor_label() for a in fleet if a.is_player_controlled()}
            blue_goals=[a['target_en_m'] for a in assignments if a['team']==0 and a['ship'] not in player_labels]
            st['checks']['distinct_blue_patrol_destinations']=all(math.dist(a,b)>=200 for i,a in enumerate(blue_goals) for b in blue_goals[i+1:])
            st['checks']['selected_player_team_preserved']=next(a for a in fleet if a.is_player_controlled()).get_component_by_class(unreal.ShipWeaponComponent).team()==int(globals().get('PLAYER_TEAM',0))
            if required_legs:
                records=st['patrol_tracking']
                st['checks']['five_repeated_physical_patrols']=len(records)==5 and all(r['last_legs']>=required_legs and r['physical'] and r['controller_same'] and all(e['within_arrival_radius'] for e in r['arrivals']) for r in records.values())
            finish('PASSED' if all(st['checks'].values()) else 'FAILED')
        if time.monotonic()-st['started']>float(globals().get('WALL_LIMIT_SECONDS',120)):raise AssertionError('Seed fixture wall-time bound')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')

st['handle']=unreal.register_slate_post_tick_callback(tick);save()
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
