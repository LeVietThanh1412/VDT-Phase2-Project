"""Real prominent-island occlusion, using the preserved map patrol anchors."""
import builtins,json,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());st=dict(handle=None,rows=[],initialized=False,error=None,last=-1)
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<1:return
    try:
        ships=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'))
        source=next(s for s in ships if s.get_actor_label()=='BP_ShipEnemy');target=next(s for s in ships if s.is_player_controlled())
        combat=source.get_component_by_class(unreal.ShipCombatComponent);weapon=source.get_component_by_class(unreal.ShipWeaponComponent)
        if not st['initialized']:
            for s in ships:
                s.can_be_damaged=False;c=s.get_component_by_class(unreal.ShipCombatComponent);n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=s==source;c.enable_hit_and_run=False
                if n:n.stop_navigation()
                h=next(x for x in s.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM');h.set_simulate_physics(False)
                if s not in [source,target]:s.set_actor_location(s.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
            n=source.get_component_by_class(unreal.ShipEnemyNavigationComponent)
            source.set_actor_location(n.patrol_actor_a.get_actor_location()+unreal.Vector(0,0,680),False,True)
            # Actual accepted water south of the island, independent of the
            # patrol B marker that the user moved to a new saved location.
            chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
            g=target.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
            ecef=unreal.Vector(*chart['origin_ecef'])+unreal.Vector(*chart['east_axis'])*-50+unreal.Vector(*chart['north_axis'])*-1950
            llh=g.transform_unreal_position_to_longitude_latitude_height(g.transform_earth_centered_earth_fixed_position_to_unreal(ecef));llh.z=14.7
            target.set_actor_location(g.transform_longitude_latitude_height_position_to_unreal(llh),False,True)
            source.set_actor_rotation(unreal.Rotator(pitch=0,yaw=70,roll=0),True)
            for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                if a.get_class().get_name().startswith('BP_ShipProjectile'):a.destroy_actor()
            st['initialized']=True;st['initial_shots']=weapon.shots_fired
        if t-st['last']>.5:
            st['last']=t;combat.scan_targets()
            st['rows'].append(dict(t=t,target=combat.target.get_actor_label() if combat.target else None,visible=combat.target_visible,
                shots=weapon.shots_fired-st['initial_shots'],status=combat.firing_status,distance_m=(source.get_actor_location()-target.get_actor_location()).length()/100))
        if t>15:
            st['passed']=all(r['shots']==0 and not r['visible'] and r['target'] is None and r['distance_m']<combat.engagement_radius_m for r in st['rows'][2:])
            (root/'Docs/NavalCombat/ISLAND_OCCLUSION_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k!='handle'},indent=2));unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    except Exception:
        st['error']=traceback.format_exc();(root/'Docs/NavalCombat/ISLAND_OCCLUSION_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k!='handle'},indent=2));unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
st['handle']=unreal.register_slate_post_tick_callback(tick);builtins.island_test=st
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
