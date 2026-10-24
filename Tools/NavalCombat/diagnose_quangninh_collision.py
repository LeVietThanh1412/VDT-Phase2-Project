"""Two fresh PIE runs: baseline versus temporary terrain collision isolation.

Run via MCP with ISOLATE=False/True. Changes exist only in the PIE world.
All five ships keep their saved routes and original force/buoyancy Blueprints.
"""
import builtins,json,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());isolate=globals().get('ISOLATE',False)
st=dict(handle=None,last=-1,rows=[],contacts=[],initial={},error=None,isolation=isolate)
builtins.quangninh_diagnostic=st
vec=lambda p:[p.x,p.y,p.z]
def finish(status):
    st['status']=status
    unreal.unregister_slate_post_tick_callback(st['handle']);st['handle']=None
    output=root/'Docs/QuangNinhIntegration'/globals().get('OUTPUT','COLLISION_ISOLATED.json' if isolate else 'COLLISION_BASELINE.json')
    output.write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','callbacks']},indent=2))
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def hit(component,other,other_component,impulse,result):
    if len(st['contacts'])<100:
        h=result.to_tuple()
        st['contacts'].append(dict(ship=component.get_owner().get_actor_label(),other=other.get_actor_label() if other else None,component=other_component.get_path_name() if other_component else None,impulse=vec(impulse),point=vec(h[5]),normal=vec(h[7])))
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    try:
        t=unreal.GameplayStatics.get_time_seconds(w)
        terrain=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Cesium3DTileset) if a.get_actor_label()=='QuangNinh_Terrain')
        if isolate:
            for c in terrain.get_components_by_class(unreal.StaticMeshComponent):c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        ships=[]
        for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn):
            weapon=a.get_component_by_class(unreal.ShipWeaponComponent)
            if not weapon:continue
            a.can_be_damaged=False
            combat=a.get_component_by_class(unreal.ShipCombatComponent)
            if combat:combat.enable_combat=False
            n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
            if not n:continue
            hull=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
            if a.get_actor_label() not in st['initial']:
                st['initial'][a.get_actor_label()]=vec(hull.get_world_location())
                # Dynamic Python hit-delegate binding is unavailable in this
                # editor build. Record actual collision surface traces below.
            if t-st['last']<1:continue
            p=hull.get_world_location();anchor=a.get_component_by_class(unreal.CesiumGlobeAnchorComponent);geo=anchor.resolve_georeference()
            b=a.get_component_by_class(unreal.BuoyancyComponent)
            h=unreal.SystemLibrary.line_trace_single(w,p+unreal.Vector(0,0,2000),p-unreal.Vector(0,0,5000),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[a],unreal.DrawDebugTrace.NONE,True)
            ht=h.to_tuple() if h else None
            target=a.get_editor_property('DestinationActor')
            llh=geo.transform_unreal_position_to_longitude_latitude_height(p)
            roundtrip=geo.transform_longitude_latitude_height_position_to_unreal(llh)
            ships.append(dict(name=a.get_actor_label(),blueprint=a.get_class().get_name(),controller=a.get_controller().get_class().get_name() if a.get_controller() else None,actor=vec(a.get_actor_location()),body=vec(p),geographic=vec(llh),roundtrip_error_cm=(roundtrip-p).length(),anchor_count=len(a.get_components_by_class(unreal.CesiumGlobeAnchorComponent)),georeference=geo.get_path_name(),anchor_detect_changes=anchor.get_editor_property('detect_transform_changes'),anchor_teleport=anchor.teleport_when_updating_transform,speed_mps=hull.get_physics_linear_velocity().length()/100,velocity=vec(hull.get_physics_linear_velocity()),angular_velocity=vec(hull.get_physics_angular_velocity_in_degrees()),throttle=n.desired_throttle,rudder=n.desired_rudder,state=str(n.navigation_state),path=n.pathfinding_succeeded,waypoints=len(n.current_route),target=target.get_actor_label() if target else None,goal_error_m=(n.geographic_to_world(n.current_final_destination)-target.get_actor_location()).length()/100 if target and n.pathfinding_succeeded else None,in_water=b.is_in_water_body(),water_bodies=[x.get_owner().get_actor_label() for x in b.get_current_water_body_components()],trace_point=vec(ht[5]) if ht else None,trace_normal=vec(ht[7]) if ht else None,trace_component=ht[10].get_path_name() if ht and ht[10] else None,physics=hull.is_simulating_physics()))
        if t-st['last']>=1:
            filter=terrain.get_component_by_class(unreal.SeaTerrainCollisionComponent)
            stats={k:str(filter.get_editor_property(k)) for k in ['filtered_tiles','retained_land_triangles','removed_water_triangles','retained_submerged_triangles','last_error']} if filter else None
            st['last']=t;st['rows'].append(dict(t=t,slate_delta=dt,ships=ships,terrain_progress=terrain.get_editor_property('load_progress'),filter=stats))
        if t>=globals().get('DURATION',35):finish('CAPTURED')
    except Exception:st['error']=traceback.format_exc();finish('PROBE_ERROR')
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()


