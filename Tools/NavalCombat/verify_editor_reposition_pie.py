"""Validate a saved editor relocation after a clean editor restart."""
import builtins,json,math,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
expected=json.loads((root/'Docs/NavalCombat/NAVIGATION_EDITOR_REPOSITION.json').read_text())
chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
st=dict(status='RUNNING',last=-1,rows=[],error=None)
builtins.editor_reposition_probe=st

def metric(a):
    g=a.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
    p=g.transform_unreal_position_to_earth_centered_earth_fixed(a.get_actor_location());q=[p.x,p.y,p.z]
    return [sum((q[i]-chart['origin_ecef'][i])*chart[k][i] for i in range(3)) for k in ['east_axis','north_axis']]

def finish(status):
    st['status']=status
    (root/'Docs/NavalCombat/NAVIGATION_EDITOR_REPOSITION_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k!='handle'},indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()

def tick(dt):
    w=ed.get_game_world()
    if not w:return
    try:
        t=unreal.GameplayStatics.get_time_seconds(w);fleet=[]
        for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn):
            if (c:=a.get_component_by_class(unreal.ShipCombatComponent)):c.enable_combat=False
            if a.get_component_by_class(unreal.ShipEnemyNavigationComponent):fleet.append(a)
        if t-st['last']<1:return
        st['last']=t
        subject=next(a for a in fleet if a.get_actor_label()==expected['ship']);n=subject.get_component_by_class(unreal.ShipEnemyNavigationComponent)
        en=metric(subject)
        if not st['rows']:st['initial_en_m']=en
        st['rows'].append(dict(t=t,en_m=en,diagnostics=json.loads(n.get_navigation_diagnostics()),
            five_ai_controllers=len(fleet)==5 and all(a.get_controller() and not a.is_player_controlled() for a in fleet)))
        if t>=25:
            st['checks']=dict(saved_position_persisted=math.dist(st['initial_en_m'],expected['after_en_m'])<1,
                real_displacement=math.dist(st['initial_en_m'],en)>100,
                correct_current_route=n.pathfinding_succeeded and len(n.current_route)>=2,
                no_spurious_runtime_resets=n.reposition_reinitializations==0,
                five_ai_controllers=all(r['five_ai_controllers'] for r in st['rows']),
                endpoints_preserved=n.patrol_actor_a.get_actor_label()==expected['a'] and n.patrol_actor_b.get_actor_label()==expected['b'])
            finish('PASSED' if all(st['checks'].values()) else 'FAILED')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')

st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
