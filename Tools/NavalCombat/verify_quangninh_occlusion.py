"""Actual QuangNinh island blocks both the terrain ray and automatic fire."""
import builtins,json,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
st=dict(handle=None,rows=[],initialized=False,error=None,last=-1)
builtins.quangninh_occlusion=st

def finish():
    unreal.unregister_slate_post_tick_callback(st['handle'])
    (root/'Docs/NavalCombat/QUANGNINH_OCCLUSION_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','source','target']},indent=2))
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()

def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<1:return
    try:
        ships=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
        if not st['initialized']:
            source=next(a for a in ships if a.get_actor_label()=='BP_ShipEnemy')
            target=next(a for a in ships if a.is_player_controlled())
            for a in ships:
                a.can_be_damaged=False;a.set_actor_tick_enabled(False)
                c=a.get_component_by_class(unreal.ShipCombatComponent);n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=a==source;c.enable_hit_and_run=False
                if n:n.stop_navigation()
                next(x for x in a.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM').set_simulate_physics(False)
                if a not in [source,target]:a.set_actor_location(a.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
            geo=source.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
            for a,en in [(source,[13661.551,-3101.517]),(target,[14661.551,-3101.517])]:
                ecef=unreal.Vector(*chart['origin_ecef'])+unreal.Vector(*chart['east_axis'])*en[0]+unreal.Vector(*chart['north_axis'])*en[1]
                llh=geo.transform_unreal_position_to_longitude_latitude_height(geo.transform_earth_centered_earth_fixed_position_to_unreal(ecef));llh.z=14.7
                a.set_actor_location(geo.transform_longitude_latitude_height_position_to_unreal(llh),False,True)
                a.set_actor_rotation(unreal.Rotator(),True)
            for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                if a.get_class().get_name().startswith('BP_ShipProjectile'):a.destroy_actor()
            st.update(initialized=True,source=source,target=target,initial_shots=source.get_component_by_class(unreal.ShipWeaponComponent).shots_fired)
        if t-st['last']>=.5:
            st['last']=t;source=st['source'];target=st['target'];combat=source.get_component_by_class(unreal.ShipCombatComponent)
            weapon=source.get_component_by_class(unreal.ShipWeaponComponent);combat.scan_targets()
            muzzle=source.get_component_by_class(unreal.SkeletalMeshComponent).get_socket_location('front_gun_v_socket')
            ignore=ships+[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor) if a.get_class().get_name().startswith('WaterBody') or a.get_class().get_name()=='WaterZone']
            h=unreal.SystemLibrary.line_trace_single(w,muzzle,target.get_actor_location()+unreal.Vector(0,0,1000),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignore,unreal.DrawDebugTrace.NONE,True)
            ht=h.to_tuple() if h else None
            st['rows'].append(dict(t=t,shots=weapon.shots_fired-st['initial_shots'],visible=combat.target_visible,target=combat.target.get_actor_label() if combat.target else None,distance_m=(source.get_actor_location()-target.get_actor_location()).length()/100,terrain_blocks_ray=bool(ht and isinstance(ht[9],unreal.Cesium3DTileset)),collision_component=ht[10].get_class().get_name() if ht and ht[10] else None))
        if t>=18:
            rows=[r for r in st['rows'] if r['t']>8]
            st['checks']=dict(actual_island_blocks_physical_ray=bool(rows) and all(r['terrain_blocks_ray'] for r in rows),hostile_within_engagement_range=all(r['distance_m']<1200 for r in rows),no_acquisition_or_shot_through_island=all(r['shots']==0 and not r['visible'] and r['target'] is None for r in rows))
            st['passed']=all(st['checks'].values());finish()
    except Exception:st['error']=traceback.format_exc();st['passed']=False;finish()

st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
