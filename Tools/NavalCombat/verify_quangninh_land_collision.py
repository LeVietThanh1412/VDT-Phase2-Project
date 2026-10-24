"""Real PIE physics probes on geographic islands, across view/LOD changes.

Only temporary probes/cameras are spawned. Saved ships/targets are unchanged.
"""
import builtins,json,traceback,math
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());points=json.loads((root/'Docs/QuangNinhIntegration/ISLAND_PROBE_POINTS.json').read_text())
st=dict(handle=None,last=-1,rows=[],checks=[],probes=[],error=None,stage=0)
builtins.land_collision_probe=st
vec=lambda p:[p.x,p.y,p.z]
def check(name,result):st['checks'].append(dict(name=name,passed=bool(result)))
def spawn(w,cls,position,rotation=unreal.Rotator()):
    g=unreal.get_default_object(unreal.GameplayStatics.static_class());t=unreal.Transform(location=position,rotation=rotation)
    a=g.call_method('BeginDeferredActorSpawnFromClass',args=(w,cls,t,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    g.call_method('FinishSpawningActor',args=(a,t,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT));return a
def trace(w,p,ignore):
    h=unreal.SystemLibrary.line_trace_single(w,p+unreal.Vector(0,0,150000),p-unreal.Vector(0,0,10000),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignore,unreal.DrawDebugTrace.NONE,True)
    return h.to_tuple() if h else None
def finish(status):
    st['status']=status;unreal.unregister_slate_post_tick_callback(st['handle'])
    for p in st['probes']:p.pop('actor',None)
    (root/'Docs/QuangNinhIntegration/LAND_COLLISION_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','camera']},indent=2))
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    try:
        t=unreal.GameplayStatics.get_time_seconds(w)
        terrain=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Cesium3DTileset) if a.get_actor_label()=='QuangNinh_Terrain')
        geo=next(iter(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CesiumGeoreference)))
        ships=[]
        for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn):
            if not a.get_component_by_class(unreal.ShipWeaponComponent):continue
            ships.append(a);a.can_be_damaged=False
            c=a.get_component_by_class(unreal.ShipCombatComponent)
            if c:c.enable_combat=False
        if st['stage']==0 and t>4:
            # This camera sees the operating bay, then faces away from it.
            eye=unreal.Vector(-150000,-150000,240000);focus=unreal.Vector(-150000,-150000,0)
            camera=spawn(w,unreal.CameraActor.static_class(),eye,unreal.MathLibrary.find_look_at_rotation(eye,focus));st['camera']=camera
            unreal.GameplayStatics.get_player_controller(w,0).set_view_target_with_blend(camera,0)
            st['stage']=1
        if st['stage']==1 and t>10:
            for i,spec in enumerate(points):
                p=geo.transform_longitude_latitude_height_position_to_unreal(unreal.Vector(*spec['llh']));h=trace(w,p,ships)
                if not h or h[9]!=terrain:continue
                body=spawn(w,unreal.StaticMeshActor.static_class(),h[5]+unreal.Vector(0,0,1000))
                mesh=body.static_mesh_component;mesh.set_mobility(unreal.ComponentMobility.MOVABLE)
                mesh.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));mesh.set_collision_profile_name('BlockAll')
                # A vertical drop measures support, not sliding off a steep
                # island into deliberately empty ocean collision.
                instance=mesh.get_editor_property('body_instance')
                for key in ['lock_x_translation','lock_y_translation','lock_x_rotation','lock_y_rotation','lock_z_rotation']:instance.set_editor_property(key,True)
                mesh.set_editor_property('body_instance',instance)
                mesh.set_constraint_mode(unreal.DOFMode.SIX_DOF)
                mesh.set_simulate_physics(True);mesh.set_mass_override_in_kg('',10,True)
                st['probes'].append(dict(actor=body,index=i,llh=spec['llh'],surface=vec(h[5]),source=h[10].get_class().get_name(),initial=vec(body.get_actor_location())))
            check('At least six actual island collision surfaces loaded',len(st['probes'])>=6)
            check('Retained coastal proxy collision is exercised',any(p['source']=='ProceduralMeshComponent' for p in st['probes']))
            st['stage']=2
        if st['stage']==2 and t>20:
            st['camera'].set_actor_rotation(unreal.Rotator(pitch=10,yaw=90,roll=0),False);st['stage']=3
        if st['stage']==3 and t>30:
            # A near-ground view forces a different tile/LOD selection.
            st['camera'].set_actor_location(unreal.Vector(-170000,-100000,8000),False,True)
            st['camera'].set_actor_rotation(unreal.Rotator(pitch=-15,yaw=-90,roll=0),False);st['stage']=4
        if t-st['last']>=1:
            st['last']=t;samples=[]
            for p in st['probes']:
                body=p['actor'];mesh=body.static_mesh_component
                h=trace(w,body.get_actor_location(),ships+[q['actor'] for q in st['probes']])
                samples.append(dict(index=p['index'],world=vec(body.get_actor_location()),speed_mps=mesh.get_physics_linear_velocity().length()/100,terrain_hit=bool(h and h[9]==terrain),surface=vec(h[5]) if h else None,source=h[10].get_class().get_name() if h and h[10] else None))
            f=terrain.get_component_by_class(unreal.SeaTerrainCollisionComponent)
            st['rows'].append(dict(t=t,stage=st['stage'],probes=samples,tiles=f.filtered_tiles,land_triangles=f.retained_land_triangles,water_triangles=f.removed_water_triangles))
        if t>=42:
            final=st['rows'][-1]['probes']
            check('All physical probes remain supported after view and LOD changes',bool(final) and all(p['terrain_hit'] and p['world'][2]>=p['surface'][2]+10 and p['speed_mps']<.5 for p in final))
            check('Retained collision prevents falling through islands',all(p['world'][2]>next(q for q in st['probes'] if q['index']==p['index'])['surface'][2]-100 for p in final))
            check('Five AI controllers preserved',sum(bool(a.get_component_by_class(unreal.ShipEnemyNavigationComponent) and a.get_controller() and not a.is_player_controlled()) for a in ships)==5)
            finish('LAND_COLLISION_VERIFIED' if all(c['passed'] for c in st['checks']) else 'ASSERTION_FAILURE')
    except Exception:st['error']=traceback.format_exc();finish('PROBE_ERROR')
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
