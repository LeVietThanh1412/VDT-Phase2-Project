"""Fresh physical Small A/B/A traversal around a real candidate-1 island.

Only this PIE fixture relocates one existing ship and its two markers before
starting the route. No normal-game movement, velocity or force is overridden.
"""
import builtins,json,math,traceback,hashlib
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
plan=json.loads((root/'Docs/NavalCombat/NAVIGATION_ISLAND_FIXTURE.json').read_text())['selected']
chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
st=dict(status='RUNNING',phase='setup',last=-1,rows=[],arrivals=[],paths=[],error=None)
builtins.island_navigation_probe=st
vec=lambda v:[v.x,v.y,v.z]

def metric(g,p):
    q=vec(g.transform_unreal_position_to_earth_centered_earth_fixed(p))
    return [sum((q[i]-chart['origin_ecef'][i])*chart[k][i] for i in range(3)) for k in ['east_axis','north_axis']]

def spawn_camera(w,pos,rot):
    lib=unreal.get_default_object(unreal.GameplayStatics.static_class());tr=unreal.Transform(location=pos,rotation=rot)
    a=lib.call_method('BeginDeferredActorSpawnFromClass',args=(w,unreal.CameraActor.static_class(),tr,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    lib.call_method('FinishSpawningActor',args=(a,tr,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT));return a

def save():
    data={k:v for k,v in st.items() if k not in ['handle','ship','camera','controller']}
    (root/'Saved/NavigationIslandTrajectory.json').write_text(json.dumps(data,indent=2),encoding='utf-8')

def finish(status):
    st['status']=status;save()
    payload=(root/'Saved/NavigationIslandTrajectory.json').read_bytes()
    summary={k:v for k,v in st.items() if k not in ['handle','ship','camera','controller','rows']}
    summary.update(raw_local_path=str(root/'Saved/NavigationIslandTrajectory.json'),raw_sha256=hashlib.sha256(payload).hexdigest())
    (root/'Docs/NavalCombat/NAVIGATION_ISLAND_PIE.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()

def tick(dt):
    w=ed.get_game_world()
    if not w:return
    try:
        t=unreal.GameplayStatics.get_time_seconds(w)
        if t<1:return
        for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn):
            c=a.get_component_by_class(unreal.ShipCombatComponent)
            if c:c.enable_combat=False
            if a.get_component_by_class(unreal.ShipWeaponComponent):a.can_be_damaged=False
        if st['phase']=='setup':
            a=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_actor_label()=='BP_ShipEnemy2')
            n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent);n.stop_navigation()
            assert a.get_component_by_class(unreal.ShipMovementComponent).hull_class==unreal.ShipHullClass.SMALL
            anchor=a.get_component_by_class(unreal.CesiumGlobeAnchorComponent);g=anchor.resolve_georeference()
            ocean=next(iter(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.WaterBodyOcean)))
            water=ocean.get_component_by_class(unreal.WaterBodyOceanComponent)
            positions=[]
            for ll in [plan['a_ll'],plan['b_ll']]:
                p=g.transform_longitude_latitude_height_position_to_unreal(unreal.Vector(*ll,0))
                surface,normal,velocity,depth=water.get_water_surface_info_at_location(p,False)
                positions.append(unreal.Vector(p.x,p.y,surface.z))
            for marker,p in zip([n.patrol_actor_a,n.patrol_actor_b],positions):
                marker.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
                marker.set_actor_location(p,False,True)
                if (ma:=marker.get_component_by_class(unreal.CesiumGlobeAnchorComponent)):ma.sync()
            anchor.move_to_longitude_latitude_height(g.transform_unreal_position_to_longitude_latitude_height(positions[0]+unreal.Vector(0,0,600)))
            a.set_actor_rotation(unreal.Rotator(yaw=-90),True);anchor.sync()
            n.start_at_patrol_b=True;n.start_patrol()
            eye=positions[0]+unreal.Vector(-60000,0,180000)
            camera=spawn_camera(w,eye,unreal.MathLibrary.find_look_at_rotation(eye,positions[0]))
            unreal.GameplayStatics.get_player_controller(w,0).set_view_target_with_blend(camera,0)
            st.update(phase='traverse',ship=a,camera=camera,controller=a.get_controller().get_path_name(),
                controller_same=True,physical_all=True,start_en_m=metric(g,a.get_actor_location()),
                assigned_a=metric(g,n.patrol_actor_a.get_actor_location()),assigned_b=metric(g,n.patrol_actor_b.get_actor_location()),
                shore_clearance_m=plan['start_shore_clearance_m'],direct_intersects_real_island=plan['direct_intersects_real_island'],
                path_max_waypoints=0,prior_legs=0,distance_traveled_m=0,endpoint_errors=[],min_native_search_expansions=None)
        a=st['ship'];n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
        h=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
        g=a.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
        st['controller_same'] &= bool(a.get_controller() and a.get_controller().get_path_name()==st['controller'] and not a.is_player_controlled())
        st['physical_all'] &= h.is_simulating_physics()
        if t-st['last']<.5:return
        st['last']=t;p=metric(g,h.get_world_location());active=a.get_editor_property('DestinationActor')
        if st['rows']:st['distance_traveled_m']+=math.dist(p,st['rows'][-1]['en_m'])
        st['path_max_waypoints']=max(st['path_max_waypoints'],len(n.current_route))
        row=dict(t=t,en_m=p,speed_mps=h.get_physics_linear_velocity().length()/100,legs=n.patrol_legs_completed,
            state=str(n.navigation_state),failure=n.last_navigation_failure_reason,
            active=active.get_actor_label() if active else None,target_en_m=metric(g,active.get_actor_location()) if active else None,
            native_diagnostics=json.loads(n.get_navigation_diagnostics()),water=a.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body())
        if n.pathfinding_succeeded and n.current_route:
            route=[vec(v) for v in n.current_route]
            if not st['paths'] or route!=st['paths'][-1]['points']:st['paths'].append(dict(t=t,expansions=n.last_search_expansions,points=route))
            if active and math.dist(route[-1][:2],metric(g,active.get_actor_location()))>1:st['endpoint_errors'].append(t)
        if n.patrol_legs_completed>st['prior_legs']:
            previous=st['rows'][-1]
            st['arrivals'].append(dict(t=t,leg=n.patrol_legs_completed,target=previous['active'],distance_m=math.dist(p,previous['target_en_m'])))
            st['prior_legs']=n.patrol_legs_completed
        st['rows'].append(row)
        if len(st['rows'])%10==0:save()
        if n.patrol_legs_completed>=2:
            st['checks']=dict(real_island_detour=st['direct_intersects_real_island'] and st['path_max_waypoints']>2,
                actual_B_A_arrivals=len(st['arrivals'])>=2 and all(x['distance_m']<=n.arrival_radius_m+3 for x in st['arrivals']),
                physical_traversal=st['distance_traveled_m']>1200,controller_preserved=st['controller_same'],
                continuously_physical=st['physical_all'],exact_endpoints=not st['endpoint_errors'])
            finish('PASSED' if all(st['checks'].values()) else 'FAILED')
        elif t>620:finish('TIME_LIMIT_REACHED')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')

st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
