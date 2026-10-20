"""Real PIE merge regression; writes separate evidence and never saves PIE changes.

Run through the live editor Python bridge. Player actions use Enhanced Input.
Camera methods are the same handlers bound to Q/F. AI routes/forces stay intact.
"""
import builtins
import json
import math
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
output = root / 'Docs/NavalCombat' / globals().get('OUTPUT', 'MERGE_INTEGRATION_PIE.json')
duration = globals().get('DURATION', 90)
state = dict(handle=None, status='RUNNING', rows=[], checks=[], done=[], error=None)
builtins.merge_integration_probe = state
ship_class = unreal.load_class(None, '/Game/Ship/BluePrint/BP_Ship.BP_Ship_C')
minimap_class = unreal.load_class(None, '/Game/Game/UI/Minimap/Widgets/WBP_MainMinimap.WBP_MainMinimap_C')
actions = {name: unreal.load_asset('/Game/Ship/Inputs/' + name) for name in
           ['IA_MoveForwardBackward', 'IA_MoveLeftRight', 'IA_LookAround', 'IA_Aim', 'IA_Fire']}
chart = json.loads((root / 'Content/SeaNavigation/halong_navigation.json').read_text())
vec = lambda p: [p.x, p.y, p.z]


def check(name, passed):
    state['checks'].append(dict(name=name, passed=bool(passed)))


def metric(actor, world):
    anchor = actor.get_component_by_class(unreal.CesiumGlobeAnchorComponent)
    geo = anchor.resolve_georeference()
    origin = unreal.GameplayStatics.get_world_origin_location(world)
    ecef = vec(geo.transform_unreal_position_to_earth_centered_earth_fixed(
        actor.get_actor_location() + unreal.Vector(origin.x, origin.y, origin.z)))
    return [sum((ecef[i] - chart['origin_ecef'][i]) * chart[axis][i] for i in range(3))
            for axis in ['east_axis', 'north_axis']]


def finish(status):
    state['status'] = status
    output.write_text(json.dumps({k: v for k, v in state.items() if k != 'handle'}, indent=2))
    unreal.unregister_slate_post_tick_callback(state['handle'])
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()


def tick(dt):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world:
        return
    t = unreal.GameplayStatics.get_time_seconds(world)
    if t < 2:
        return
    try:
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        ships = unreal.GameplayStatics.get_all_actors_of_class(world, ship_class)
        player = next(s for s in ships if not s.get_component_by_class(unreal.ShipEnemyNavigationComponent))
        ai = [s for s in ships if s != player]
        if 'initialized' not in state['done']:
            check('one player ship and five AI ships', len(ships) == 6 and len(ai) == 5)
            check('CustomPlayerController active', isinstance(pc, unreal.CustomPlayerController))
            check('correct manual pawn', unreal.GameplayStatics.get_player_pawn(world, 0) == player)
            state['controllers'] = {s.get_actor_label(): s.get_controller().get_path_name() for s in ai}
            state['done'].append('initialized')
        # Protect the scene during input/camera checks; original AI weapons still fire.
        if t < 35:
            for s in ships:
                s.can_be_damaged = False
        else:
            for s in ships:
                s.can_be_damaged = True
        inputs = next(x for x in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem)
                      if not x.get_name().startswith('Default__'))
        def inject(name, values):
            inputs.inject_input_vector_for_action(actions[name], unreal.Vector(*values), [], [])
        if 3 < t < 9:
            inject('IA_MoveForwardBackward', (1, 0, 0))
        if 9 < t < 12:
            inject('IA_MoveForwardBackward', (.5, 0, 0))
            inject('IA_MoveLeftRight', (1, 0, 0))
        if 12 < t < 13:
            inject('IA_LookAround', (.4, .2, 0))
        if 14 < t < 18.5:
            inject('IA_Aim', (1, 0, 0))
        if 16 < t < 17:
            inject('IA_Fire', (1, 0, 0))
        if t > 22 and 'cycle' not in state['done']:
            pc.cycle_ship_camera()
            state['done'].append('cycle')
        if t > 23 and 'cycle_check' not in state['done']:
            check('Q changes view target', pc.get_view_target() != player)
            check('Q keeps player possession', unreal.GameplayStatics.get_player_pawn(world, 0) == player)
            check('Q preserves every AI controller', all(s.get_controller().get_path_name() == state['controllers'][s.get_actor_label()] for s in ai))
            pc.set_view_target_with_blend(player, 0)
            state['done'].append('cycle_check')
        if t > 25 and 'free' not in state['done']:
            pc.toggle_free_camera()
            state['done'].append('free')
        if 26 < t < 28:
            camera = unreal.GameplayStatics.get_player_pawn(world, 0)
            if isinstance(camera, unreal.FreeCameraPawn):
                camera.add_movement_input(camera.get_actor_forward_vector(), 1)
        if t > 28 and 'free_check' not in state['done']:
            check('F activates free camera', isinstance(unreal.GameplayStatics.get_player_pawn(world, 0), unreal.FreeCameraPawn))
            check('F preserves every AI controller', all(s.get_controller().get_path_name() == state['controllers'][s.get_actor_label()] for s in ai))
            pc.toggle_free_camera()
            check('F restores original player', unreal.GameplayStatics.get_player_pawn(world, 0) == player)
            state['done'].append('free_check')
        if t > 65 and 'death' not in state['done']:
            victim = next(s for s in ai if s.get_actor_label() == 'BP_ShipEnemy3')
            victim.get_component_by_class(unreal.ShipWeaponComponent).die(unreal.ShipDeathReason.OTHER, player)
            state['done'].append('death')
        if not state['rows'] or t - state['rows'][-1]['t'] >= 1:
            row = dict(t=t, ships=[], minimaps=[])
            for s in ships:
                weapon = s.get_component_by_class(unreal.ShipWeaponComponent)
                nav = s.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                hull = next(c for c in s.get_components_by_class(unreal.PrimitiveComponent) if c.get_name() == 'ShipSM')
                r = dict(name=s.get_actor_label(), en_m=metric(s, world), controller=s.get_controller().get_class().get_name() if s.get_controller() else None,
                         physics=hull.is_simulating_physics(), water=s.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body(),
                         team=weapon.team(), hp=weapon.current_health, shots=weapon.shots_fired, hits=weapon.hits, dead=weapon.is_destroyed)
                if nav:
                    target=s.get_editor_property('DestinationActor')
                    r.update(path=nav.pathfinding_succeeded, route=len(nav.current_route), failure=nav.last_navigation_failure_reason,
                             legs=nav.patrol_legs_completed, active=target.get_actor_label() if target else None,
                             endpoint_error_m=math.dist(vec(nav.current_route[-1])[:2], metric(target, world)) if nav.current_route and target else None)
                row['ships'].append(r)
            for m in unreal.WidgetLibrary.get_all_widgets_of_class(world, minimap_class, False):
                markers=m.get_editor_property('ShipMarkerWidgets')
                slots=m.get_editor_property('ShipMarkerSlots')
                tracked=m.get_editor_property('ShipActors')
                def marker_row(s,mark,slot):
                    color=mark.get_editor_property('color_and_opacity')
                    return dict(ship=s.get_actor_label(),team=s.get_component_by_class(unreal.ShipWeaponComponent).team(),
                                position=[slot.get_position().x,slot.get_position().y],color=[color.r,color.g,color.b,color.a])
                row['minimaps'].append(dict(viewport=m.is_in_viewport(), native=isinstance(m,unreal.ShipMinimapWidget),
                                           markers=[marker_row(s,mark,slot) for s,mark,slot in zip(tracked,markers,slots)]))
            gun=next(c for c in player.get_components_by_class(unreal.CameraComponent) if c.get_name()=='GunCamera')
            row['gun_camera']=gun.is_active()
            state['rows'].append(row)
        if t >= duration:
            tracks={name:[r for row in state['rows'] for r in row['ships'] if r['name']==name] for name in state['controllers']}
            check('all five physically move', all(max(math.dist(rows[0]['en_m'],r['en_m']) for r in rows)>20 for rows in tracks.values()))
            check('all five generate A* paths', all(any(r.get('path') for r in rows) for rows in tracks.values()))
            check('patrol endpoints match actual target actors', all(r['endpoint_error_m']<1 for rows in tracks.values() for r in rows if r.get('endpoint_error_m') is not None and not r['dead']))
            check('manual original projectile fired', any(s['shots']>0 for row in state['rows'] for s in row['ships'] if s['name']==player.get_actor_label()))
            positions=[s['en_m'] for row in state['rows'] for s in row['ships'] if s['name']==player.get_actor_label()]
            check('manual input physically moves player', max(math.dist(positions[0],p) for p in positions)>20)
            check('GunCamera aiming activates', any(row['gun_camera'] for row in state['rows']))
            check('minimap has all six initial markers', any(len(m['markers'])==6 for row in state['rows'] for m in row['minimaps']))
            maps=state['rows'][-1]['minimaps']
            check('minimap remains on screen', len(maps)==1 and maps[0]['viewport'])
            check('dead ship marker removed', bool(maps) and all(mark['ship']!='BP_ShipEnemy3' for m in maps for mark in m['markers']))
            check('team marker colors match naval factions', bool(maps) and all(
                (mark['color'][2]>.9 and mark['color'][0]<.2) if mark['team']==0 else
                (mark['color'][0]>.9 and mark['color'][2]<.2)
                for m in maps for mark in m['markers']))
            six=next((m['markers'] for row in state['rows'] for m in row['minimaps'] if len(m['markers'])==6),[])
            check('markers follow moving ships', bool(maps) and any(
                math.dist(old['position'],new['position'])>.1
                for old in six for new in maps[0]['markers'] if old['ship']==new['ship']))
            finish('PASSED' if all(c['passed'] for c in state['checks']) else 'FAILED')
    except Exception:
        state['error']=traceback.format_exc()
        finish('ERROR')


state['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
