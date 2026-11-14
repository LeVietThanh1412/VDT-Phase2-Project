"""Owned PIE checks for passive M zoom and explicit Alt mouse interaction.
HOLD=True leaves normal PIE ready for a real M + mouse-fire smoke check.
No movement, health, damage, recoil or projectile settings are overridden.
"""
import unreal, time, json, traceback
from pathlib import Path

ed = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert ed.get_game_world() is None
root = Path(unreal.Paths.project_dir())
output = root/'Saved/MinimapInput/20261114/PIE_INPUT.json'
output.parent.mkdir(parents=True, exist_ok=True)
st = dict(status='RUNNING', phase='ready', started=time.monotonic(), checks={})
lib = unreal.get_default_object(unreal.load_class(None, '/Script/Engine.SubsystemBlueprintLibrary'))
actions = {n: unreal.load_asset('/Game/Ship/Inputs/'+n)
           for n in ['IA_Aim', 'IA_Fire', 'IA_LookAround']}

def save():
    output.write_text(json.dumps(st, indent=2), encoding='utf-8')

def finish(status):
    st['status'] = status
    save()
    unreal.unregister_slate_post_tick_callback(handle)
    if status != 'API_PASSED_READY_FOR_PHYSICAL':
        level.editor_request_end_play()

def passive(m, pc):
    return (not pc.show_mouse_cursor and not m.is_mouse_interaction_enabled()
            and m.get_visibility() == unreal.SlateVisibility.HIT_TEST_INVISIBLE
            and not pc.is_look_input_ignored() and not pc.is_move_input_ignored())

def tick(dt):
    try:
        assert time.monotonic()-st['started'] < 90, 'Gameplay input test time bound'
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
        m = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.ShipMinimapWidget, True)[0]
        if not m.is_visible():
            return
        checks = st['checks']
        if st['phase'] == 'ready':
            pc.toggle_minimap_interactive_mode()
            assert m.get_current_zoom() == 9 and m.is_following_ship() and passive(m,pc)
            checks['zoom_preserves_gameplay_mouse'] = True
            pc.begin_minimap_mouse_interaction()
            assert pc.show_mouse_cursor and m.is_mouse_interaction_enabled()
            checks['explicit_mouse_mode'] = True
            pc.end_minimap_mouse_interaction()
            assert passive(m,pc) and m.get_current_zoom() == 9 and m.is_following_ship()
            checks['release_preserves_zoom_and_follow'] = True
            pc.begin_minimap_mouse_interaction()
            pc.toggle_match_setup()
            assert pc.setup_widget and pc.show_mouse_cursor and not m.is_mouse_interaction_enabled()
            pc.end_minimap_mouse_interaction()
            assert pc.show_mouse_cursor and pc.setup_widget
            checks['late_alt_release_preserves_menu'] = True
            pc.toggle_minimap_interactive_mode()
            assert m.get_current_zoom() == 1 and pc.show_mouse_cursor
            checks['menu_blocks_zoom_input_override'] = True
            pc.close_match_setup()
            assert passive(m,pc)
            pc.toggle_minimap_interactive_mode()
            pc.begin_minimap_mouse_interaction()
            st['phase'] = 'release_recovery'
            return
        assert passive(m,pc) and m.get_current_zoom() == 9 and m.is_following_ship()
        checks['missing_alt_release_recovers_gameplay'] = True
        player = unreal.GameplayStatics.get_player_pawn(world,0)
        weapon = player.get_component_by_class(unreal.ShipWeaponComponent)
        inputs = lib.call_method('GetLocalPlayerSubSystemFromPlayerController',
                                 args=(pc, unreal.EnhancedInputLocalPlayerSubsystem.static_class()))
        def inject(name, value):
            inputs.inject_input_vector_for_action(actions[name], unreal.Vector(*value), [], [])
        if st['phase'] == 'release_recovery':
            st.update(phase='aim', aim_started=time.monotonic(), shots_before=weapon.shots_fired)
        age = time.monotonic()-st['aim_started']
        if age < 3:
            arrow = next(c for c in player.get_components_by_class(unreal.ArrowComponent)
                         if c.get_name() == 'Arrow')
            yaw = (arrow.get_world_rotation().yaw-pc.get_control_rotation().yaw+180)%360-180
            pitch = (pc.get_control_rotation().pitch+180)%360-180
            inject('IA_LookAround', (max(-1,min(1,yaw/4)), max(-1,min(1,(pitch-5)/4)), 0))
            inject('IA_Aim', (1,0,0))
            return
        if age < 4:
            inject('IA_Aim', (1,0,0))
            inject('IA_Fire', (1,0,0))
            return
        inject('IA_Aim', (0,0,0))
        inject('IA_Fire', (0,0,0))
        assert weapon.shots_fired > st['shots_before'], weapon.rejection_reason
        checks['enhanced_input_blueprint_fire_after_zoom'] = True
        st.update(player=player.get_name(), shots_after_input=weapon.shots_fired,
                  rounds_after_input=weapon.rounds_remaining, can_be_damaged=player.can_be_damaged)
        pc.toggle_minimap_interactive_mode()
        assert m.get_current_zoom() == 1 and passive(m,pc)
        checks['exit_restores_passive_overview'] = True
        finish('API_PASSED_READY_FOR_PHYSICAL' if globals().get('HOLD',False) else 'PASSED')
    except Exception:
        st['error'] = traceback.format_exc()
        finish('FAILED')

handle = unreal.register_slate_post_tick_callback(tick)
save()
level.editor_request_begin_play()
