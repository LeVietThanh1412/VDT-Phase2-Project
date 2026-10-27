"""Reproduce return-leg stalls with camera LOD changes; never override drive.

Combat is disabled only in this disposable PIE. Geometry snapshots remain in
Saved; the compact acceptance report is separate from raw diagnostic data.
"""
import builtins, json, traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
ed = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
scope = dict(OUTPUT='NAVIGATION_RETURN_LOD_REPRO.json', LIMIT=450,
             REQUIRED_LEGS=3, __name__='navigation_return_repro')
p = root/'Tools/NavalCombat/verify_patrol_loops.py'
exec(compile(p.read_text(encoding='utf-8'), str(p), 'exec'), scope)
st = dict(last=-1, camera=0, rows=[], geometry={}, error=None)
builtins.return_patrol_diagnostic = st

def save():
    (root/'Saved/NavigationReturnCollision.json').write_text(
        json.dumps({k:v for k,v in st.items() if k != 'handle'}, indent=2), encoding='utf-8')

def tick(dt):
    w = ed.get_game_world()
    if not w:
        if st['rows']:
            save(); unreal.unregister_slate_post_tick_callback(st['handle'])
        return
    try:
        t = unreal.GameplayStatics.get_time_seconds(w)
        if int(t)//60 > st['camera']:
            unreal.GameplayStatics.get_player_controller(w,0).cycle_ship_camera()
            st['camera'] = int(t)//60
        if t-st['last'] < 2: return
        st['last'] = t
        ships=[]
        for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn):
            n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
            if not n: continue
            n.draw_debug=True
            h=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
            speed=h.get_physics_linear_velocity().length()/100
            row=dict(ship=a.get_actor_label(),state=str(n.navigation_state),speed=speed,
                desired_speed=n.desired_speed,throttle=n.desired_throttle,rudder=n.desired_rudder,
                yaw_rate=h.get_physics_angular_velocity_in_degrees().dot(h.get_up_vector()),
                sensors=list(a.get_editor_property('ScanDistances')),legs=n.patrol_legs_completed,
                recoveries=n.recovery_attempt_count)
            if t>130 and speed<3:
                point=h.get_world_location()
                water_actors=list(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.WaterBody))
                hit=unreal.SystemLibrary.line_trace_single(w,point+unreal.Vector(0,0,3000),point-unreal.Vector(0,0,6000),
                    unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[a]+water_actors,unreal.DrawDebugTrace.NONE,True)
                if hit:
                    ht=hit.to_tuple();c=ht[10]
                    row['vertical_hit']=dict(component=c.get_path_name() if c else None,point=[ht[5].x,ht[5].y,ht[5].z])
                    if isinstance(c,unreal.ProceduralMeshComponent) and c.get_path_name() not in st['geometry']:
                        verts,tris,*_=unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(c,0)
                        g=a.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
                        tr=c.get_world_transform()
                        llh=[g.transform_unreal_position_to_longitude_latitude_height(tr.transform_location(v)) for v in verts]
                        st['geometry'][c.get_path_name()]=dict(vertices_llh=[[v.x,v.y,v.z] for v in llh],triangles=list(tris))
            ships.append(row)
        st['rows'].append(dict(t=t,ships=ships));save()
    except Exception:
        st['error']=traceback.format_exc();save()
        unreal.unregister_slate_post_tick_callback(st['handle'])

st['handle']=unreal.register_slate_post_tick_callback(tick)
