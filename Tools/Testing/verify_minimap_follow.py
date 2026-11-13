"""Run in the Editor Python API. Owns PIE; changes only camera/minimap input.
Checks actual BP marker slots against the rendered overlay transform.
"""
import unreal, time, json, traceback
from pathlib import Path

ed = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert ed.get_game_world() is None
root = Path(unreal.Paths.project_dir())
output = root/'Saved/DynamicMinimap/20261114/FOLLOW_PIE.json'
output.parent.mkdir(parents=True, exist_ok=True)
st = dict(status='RUNNING', phase='ready', checks={}, samples=[], started=time.monotonic())
bound = {}
lib = unreal.get_default_object(unreal.load_class(None, '/Script/Engine.SubsystemBlueprintLibrary'))

def child(widget, name):
    candidates = []
    for obj in unreal.ObjectIterator(unreal.Widget):
        if obj.get_name() != name:
            continue
        owner = obj.get_outer()
        while owner:
            if owner == widget:
                size = obj.get_desired_size()
                candidates.append((size.x*size.y, obj, [size.x,size.y]))
                break
            owner = owner.get_outer()
    if candidates:
        st.setdefault('layout_candidates', {})[name] = [dict(path=o.get_path_name(),size=s) for _,o,s in candidates]
        return max(candidates, key=lambda item:item[0])[1]
    raise AssertionError('Authored widget missing: '+name)

def finish(status):
    st['status'] = status
    output.write_text(json.dumps(st, indent=2), encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    level.editor_request_end_play()

def sample(m, pc, border, overlay):
    selected = pc.get_selected_ship()
    rows = []
    for mark, slot in zip(m.get_editor_property('ShipMarkerWidgets'), m.get_editor_property('ShipMarkerSlots')):
        ship = mark.get_editor_property('ShipActor')
        weapon = ship.get_component_by_class(unreal.ShipWeaponComponent) if ship else None
        if weapon and not weapon.is_destroyed and weapon.current_health > 0:
            p = slot.get_position()
            rows.append((ship, p.x, p.y))
    assert rows, 'No living markers'
    focus = next(((x,y) for ship,x,y in rows if ship == selected), None)
    if focus is None:
        focus = (sum(x for _,x,_ in rows)/len(rows), sum(y for _,_,y in rows)/len(rows))
    # Get the actual arranged map-space viewport as a native vector; desired
    # size can exceed a constrained viewport, and FGeometry is opaque to Python.
    view = m.get_map_viewport_size()
    size = m.get_editor_property('MinimapSize')
    z = m.get_current_zoom()
    expected = [max(-size.x*(z-1), min(0, view.x*.5-focus[0]*z)),
                max(-size.y*(z-1), min(0, view.y*.5-focus[1]*z))]
    pan = overlay.render_transform.translation
    error = ((pan.x-expected[0])**2+(pan.y-expected[1])**2)**.5
    return dict(selected=selected.get_name() if selected else None, point=focus,
                pan=[pan.x,pan.y], expected=expected, error=error, zoom=z, view_size=[view.x,view.y])

def tick(dt):
    try:
        assert time.monotonic()-st['started'] < 100, 'PIE follow test time bound'
        world = ed.get_game_world()
        if not world:
            return
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not isinstance(pc, unreal.CustomPlayerController):
            return
        if pc.setup_widget:
            pc.close_match_setup()
        field = lib.call_method('GetWorldSubsystem', args=(world, unreal.NavalBattlefieldSubsystem.static_class()))
        assert not field.initialization_error, field.initialization_error
        if not field.ready:
            return
        m = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.ShipMinimapWidget, False)[0]
        if bound.get('minimap') != m:
            bound.update(minimap=m, border=child(m, 'Border_Wrap_Minimap'), overlay=child(m, 'Overlay_Minimap'))
        border, overlay = bound['border'], bound['overlay']
        t = unreal.GameplayStatics.get_time_seconds(world)
        if st['phase'] == 'ready':
            pc.toggle_minimap_interactive_mode()
            st.update(phase='player', at=t)
            return
        row = sample(m, pc, border, overlay)
        if st['phase'] == 'player':
            assert m.is_following_ship() and row['zoom'] == 9 and row['error'] < 2, row
            st['checks']['player_centered_at_9x'] = True
            st['player'] = row
            st['capture'] = unreal.ShipCombatLibrary.capture_gameplay_hud(world, str(output.with_suffix('.png')))
            before = pc.get_selected_ship()
            for _ in range(6):
                pc.call_method('CycleShipCamera')
                if pc.get_selected_ship() != before:
                    break
            assert pc.get_selected_ship() != before, 'Camera did not select another ship'
            st.update(phase='moving', at=t)
            return
        if st['phase'] == 'moving':
            assert m.is_following_ship() and row['error'] < 2, row
            st['samples'].append(row)
            if t-st['at'] < 3:
                return
            first = st['samples'][0]['point']
            distance = ((row['point'][0]-first[0])**2+(row['point'][1]-first[1])**2)**.5
            assert distance > .001, 'Observed selected ship did not move'
            st['movement_map_units'] = distance
            st['checks']['camera_switch_and_moving_ship_followed'] = True
            m.toggle_zoom()
            m.toggle_zoom()
            assert not m.is_following_ship()
            st.update(phase='manual', at=t, manual_pan=list(sample(m, pc, border, overlay)['pan']))
            pc.call_method('CycleShipCamera')
            return
        if st['phase'] == 'manual':
            assert not m.is_following_ship()
            assert all(abs(a-b)<.001 for a,b in zip(row['pan'],st['manual_pan'])), row
            if t-st['at'] < .5:
                return
            st['checks']['manual_view_stays_put'] = True
            pc.toggle_minimap_interactive_mode()
            pc.toggle_minimap_interactive_mode()
            st.update(phase='resume', at=t)
            return
        assert m.is_following_ship() and row['zoom'] == 9 and row['error'] < 2, row
        st['checks']['reopening_resumes_current_ship_follow'] = True
        st['resumed'] = row
        pc.toggle_minimap_interactive_mode()
        assert m.get_current_zoom() == 1 and not m.is_following_ship()
        st['checks']['exit_restores_overview'] = True
        finish('PASSED')
    except Exception:
        st['error'] = traceback.format_exc()
        finish('FAILED')

handle = unreal.register_slate_post_tick_callback(tick)
output.write_text(json.dumps(st, indent=2), encoding='utf-8')
level.editor_request_begin_play()
