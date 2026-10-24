"""Drive the actual possessed player hull at a real island using original input.

No velocity override or saved actor change. Other boats are parked in PIE only
to isolate land collision from ship traffic. Physics/buoyancy stay on the player.
"""
import builtins,json,math,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());plan=json.loads((root/'Docs/QuangNinhIntegration/SHIP_SHORE_PROBE_PLAN.json').read_text())
duration=globals().get('DURATION',85)
chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
st=dict(handle=None,last=-1,rows=[],checks=[],initialized=False,error=None)
builtins.ship_shore_probe=st
vec=lambda p:[p.x,p.y,p.z]
def finish(status):
    st['status']=status;unreal.unregister_slate_post_tick_callback(st['handle'])
    (root/'Docs/QuangNinhIntegration/SHIP_SHORE_COLLISION_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','player','inputs']},indent=2))
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    try:
        t=unreal.GameplayStatics.get_time_seconds(w)
        if t<1:return
        pc=unreal.GameplayStatics.get_player_controller(w,0);p=unreal.GameplayStatics.get_player_pawn(w,0)
        h=next(x for x in p.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM')
        geo=p.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
        if not st['initialized']:
            def world(en):
                ecef=unreal.Vector(*chart['origin_ecef'])+unreal.Vector(*chart['east_axis'])*en[0]+unreal.Vector(*chart['north_axis'])*en[1]
                llh=geo.transform_unreal_position_to_longitude_latitude_height(geo.transform_earth_centered_earth_fixed_position_to_unreal(ecef));llh.z=14.7
                return geo.transform_longitude_latitude_height_position_to_unreal(llh)
            for other in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn):
                if not other.get_component_by_class(unreal.ShipWeaponComponent):continue
                other.can_be_damaged=False;c=other.get_component_by_class(unreal.ShipCombatComponent);n=other.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=False
                if n:n.stop_navigation()
                if other!=p:
                    body=next(x for x in other.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM');body.set_simulate_physics(False)
                    other.set_actor_location(other.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
            start=world(plan['start_en_m']);end=world(plan['blocked_end_en_m']);rotation=unreal.MathLibrary.find_look_at_rotation(start,end);rotation.pitch=0;rotation.roll=0
            p.set_actor_location(start,False,True);p.set_actor_rotation(rotation,True)
            st.update(initialized=True,player=p,initial=vec(start),inputs=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary')).call_method('GetLocalPlayerSubSystemFromPlayerController',args=(unreal.GameplayStatics.get_player_controller(w,0),unreal.EnhancedInputLocalPlayerSubsystem.static_class())))
        movement=p.get_component_by_class(unreal.ShipMovementComponent);speed=movement.signed_forward_speed()
        thrust=.2 if t>6 and speed<9 else 0
        action=unreal.load_asset('/Game/Ship/Inputs/IA_MoveForwardBackward')
        st['inputs'].inject_input_vector_for_action(action,unreal.Vector(thrust,0,0),[],[])
        if t-st['last']>=.5:
            st['last']=t;position=h.get_world_location();local_lo,local_hi=h.get_local_bounds();transform=h.get_world_transform()
            center=transform.transform_location((local_lo+local_hi)*.5);extent=(local_hi-local_lo)*.5
            ignore=[p]+[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor) if a.get_class().get_name().startswith('WaterBody') or a.get_class().get_name()=='WaterZone']
            hit=unreal.SystemLibrary.box_trace_single(w,center,center+p.get_actor_forward_vector()*5000,extent,h.get_world_rotation(),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignore,unreal.DrawDebugTrace.NONE,True)
            ht=hit.to_tuple() if hit else None
            ecef=vec(geo.transform_unreal_position_to_earth_centered_earth_fixed(position))
            en=[sum((ecef[i]-chart['origin_ecef'][i])*chart[k][i] for i in range(3)) for k in ['east_axis','north_axis']]
            st['rows'].append(dict(t=t,world=vec(position),en_m=en,input=thrust,speed_mps=speed,velocity=vec(h.get_physics_linear_velocity()),physics=h.is_simulating_physics(),dead=p.get_component_by_class(unreal.ShipWeaponComponent).is_destroyed,in_water=p.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body(),terrain_contact=bool(ht and isinstance(ht[9],unreal.Cesium3DTileset)),contact_component=ht[10].get_path_name() if ht and ht[10] else None,contact_point=vec(ht[5]) if ht else None,contact_normal=vec(ht[7]) if ht else None))
        if t>duration:
            rows=st['rows'];last=rows[-1];travel=math.dist(plan['start_en_m'],last['en_m'])
            def check(name,value):st['checks'].append(dict(name=name,passed=bool(value)))
            check('Original player controls accelerate physical hull',max(r['speed_mps'] for r in rows)>7 and travel>100)
            check('Actual hull collision query reaches retained island geometry',any(r['terrain_contact'] for r in rows))
            blocked=[r for r in rows if r['t']>duration-10]
            check('Positive propulsion cannot drive through land',bool(blocked) and all(r['terrain_contact'] and abs(r['speed_mps'])<1 and r['input']>0 for r in blocked))
            check('Ship stays on water side of geographic shoreline',travel<plan['coast_distance_m'] and not last['dead'])
            finish('SHIP_SHORE_COLLISION_VERIFIED' if all(c['passed'] for c in st['checks']) else 'ASSERTION_FAILURE')
    except Exception:st['error']=traceback.format_exc();finish('PROBE_ERROR')
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
