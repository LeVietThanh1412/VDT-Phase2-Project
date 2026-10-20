"""Starts real PIE, records initialization and UMG state, stops automatically."""
import builtins,json
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());rows=[]
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<1:return
    ships=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'))
    hud=unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.ShipHUDWidget,False)
    v=lambda p:[p.x,p.y,p.z]
    chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
    def sample(s):
        weapon=s.get_component_by_class(unreal.ShipWeaponComponent);n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent)
        h=next(c for c in s.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
        g=s.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
        ecef=v(g.transform_unreal_position_to_earth_centered_earth_fixed(h.get_world_location()))
        en=[sum((ecef[i]-chart['origin_ecef'][i])*chart[k][i] for i in range(3)) for k in ['east_axis','north_axis']]
        return dict(label=s.get_actor_label(),team=weapon.team(),hp=weapon.current_health,player=s.is_player_controlled(),controller=s.get_controller().get_class().get_name() if s.get_controller() else None,
            world=v(h.get_world_location()),en_m=en,actor=v(s.get_actor_location()),physics=h.is_simulating_physics(),
            buoyancy=bool(s.get_component_by_class(unreal.BuoyancyComponent)),nav=str(n.navigation_state) if n else None,
            failure=n.last_navigation_failure_reason if n else None,route=len(n.current_route) if n else None,
            widget=weapon.health_widget.get_user_widget_object().get_class().get_name() if weapon.health_widget else None)
    rows.append(dict(t=t,total_pawns=len(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn)),ships=[sample(s) for s in ships],hud=[dict(score=x.score_text,hp=x.health_text,viewport=x.is_in_viewport()) for x in hud]))
    (root/'Docs/NavalCombat/INITIALIZATION_NEW.json').write_text(json.dumps(rows,indent=2))
    if t>4:
        unreal.unregister_slate_post_tick_callback(builtins.initial_test_handle)
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
builtins.initial_test_handle=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
