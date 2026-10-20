"""Real physics A/B/A verification; only combat is disabled in this PIE world."""
import builtins,json,math,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
out=root/'Docs/NavalCombat'/globals().get('OUTPUT','PATROL_LOOP_VERIFICATION.json')
limit=globals().get('LIMIT',1500);required=globals().get('REQUIRED_LEGS',3)
state=dict(handle=None,status='RUNNING',last=-1,rows=[],arrivals=[],errors=[],last_ships={},error=None,limit_seconds=limit,required_arrivals=required)
builtins.patrol_loop_probe=state
vec=lambda p:[p.x,p.y,p.z]
def metric(p,g,w):
    o=unreal.GameplayStatics.get_world_origin_location(w)
    q=vec(g.transform_unreal_position_to_earth_centered_earth_fixed(p+unreal.Vector(o.x,o.y,o.z)))
    return [sum((q[i]-chart['origin_ecef'][i])*chart[k][i] for i in range(3)) for k in ['east_axis','north_axis']]
def save():
    tmp=out.with_suffix('.tmp');tmp.write_text(json.dumps({k:v for k,v in state.items() if k not in ['handle','last_ships']},indent=2));tmp.replace(out)
def end(status):
    state['status']=status
    unreal.unregister_slate_post_tick_callback(state['handle']);state['handle']=None
    save();unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    try:
        pawns=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn)
        for a in pawns:
            c=a.get_component_by_class(unreal.ShipCombatComponent)
            if c:c.enable_combat=False
            if a.get_component_by_class(unreal.ShipWeaponComponent):a.can_be_damaged=False
        t=unreal.GameplayStatics.get_time_seconds(w)
        if t-state['last']<1:return
        state['last']=t;ships=[]
        for a in pawns:
            n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
            if not n:continue
            hull=next(x for x in a.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM')
            g=a.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
            active=a.get_editor_property('DestinationActor');p=metric(hull.get_world_location(),g,w)
            target=metric(active.get_actor_location(),g,w) if active else None
            velocity=hull.get_physics_linear_velocity();up=hull.get_up_vector()
            vertical=velocity.dot(up);horizontal=velocity-up*vertical
            label=a.get_actor_label()
            r=dict(name=label,controller=a.get_controller().get_class().get_name() if a.get_controller() else None,
                ai=a.is_pawn_controlled() and not a.is_player_controlled(),en_m=p,world=vec(hull.get_world_location()),heading=hull.get_world_rotation().yaw,
                speed_mps=horizontal.length()/100,signed_speed_mps=velocity.dot(a.get_actor_forward_vector())/100,
                physics=hull.is_simulating_physics(),throttle=n.desired_throttle,rudder=n.desired_rudder,state=str(n.navigation_state),
                yielding=n.yielding_to_ship,traffic_wait_s=n.traffic_wait_seconds,traffic_priority=n.temporary_traffic_priority,
                path=n.pathfinding_succeeded,failure=n.last_navigation_failure_reason,waypoint=n.current_waypoint_index,route=[vec(x) for x in n.current_route],legs=n.patrol_legs_completed,
                a=n.patrol_actor_a.get_actor_label() if n.patrol_actor_a else None,b=n.patrol_actor_b.get_actor_label() if n.patrol_actor_b else None,
                active=active.get_actor_label() if active else None,target_en_m=target,target_world=vec(active.get_actor_location()) if active else None,
                distance_to_target_m=math.dist(p,target) if target else None,arrival_radius_m=n.arrival_radius_m,
                endpoint_error_m=math.dist(vec(n.current_route[-1])[:2],target) if n.current_route and target else None,
                goal_world_error_m=(n.geographic_to_world(n.current_final_destination)-active.get_actor_location()).length()/100 if n.pathfinding_succeeded and active else None)
            previous=state['last_ships'].get(label)
            if previous and r['legs']>previous['legs']:
                arrival=dict(ship=label,t=t,leg=r['legs'],arrived_at=previous['active'],distance_m=math.dist(p,previous['target_en_m']),allowed_m=r['arrival_radius_m'],speed_mps=r['speed_mps'])
                state['arrivals'].append(arrival)
                if arrival['distance_m']>arrival['allowed_m']+3:state['errors'].append(dict(reason='Switched before reaching actual target',**arrival))
            if r['endpoint_error_m'] is not None and r['endpoint_error_m']>1:
                state['errors'].append(dict(reason='Route endpoint differs from actual target',ship=label,t=t,error_m=r['endpoint_error_m']))
            ships.append(r);state['last_ships'][label]=r
        state['rows'].append(dict(t=t,ships=ships,total_pawns=len(pawns),player_pawns=sum(p.is_player_controlled() for p in pawns)))
        if len(state['rows'])%10==0:
            try:save()
            except OSError:pass
        if len(ships)==5 and all(r['legs']>=required for r in ships):end('ALL_FIVE_LOOPS_VERIFIED' if not state['errors'] else 'ASSERTION_FAILURE')
        elif t>=state['limit_seconds']:end('TIME_LIMIT_REACHED')
    except Exception:
        state['error']=traceback.format_exc();end('PROBE_ERROR')
state['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
