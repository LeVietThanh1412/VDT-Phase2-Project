"""Controlled idle/firing comparison in the real seeded map; hull physics stays enabled.

Only AI decisions are stopped. Shared Blueprint fire, ocean waves and buoyancy remain
active. No hull pose, velocity, physics or buoyancy is overridden by this fixture.
"""
import builtins, json, math, time, traceback, hashlib, statistics
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
ed = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
name = globals().get('OUTPUT', 'STATIONARY_BUOYANCY.json')
duration = float(globals().get('PHASE_SECONDS', 40))
scope = dict(KIND='patrol', FULL_AI=True, TACTICS=False, ASSAULT=False,
             HULL_CLASSES=[0, 0, 0, 0, 1, 2], CAPTURE=False,
             OBSERVE_SECONDS=99999, OUTPUT='BUOYANCY_STARTUP.json')
source = root/'Tools/Testing/verify_final_demo.py'
exec(compile(source.read_text(encoding='utf-8'), str(source), 'exec'), scope)
base = scope['st']
st = dict(status='RUNNING', phase='startup', started=time.monotonic(), rows=[], error=None,
          native_dll_sha256=hashlib.sha256((root/'Binaries/Win64/UnrealEditor-VDTPhase2Project.dll').read_bytes()).hexdigest())
builtins.stationary_buoyancy_probe = st
fleet = []

def save():
    (root/'Saved'/name).write_text(json.dumps({k:v for k,v in st.items() if k != 'handle'}, indent=2), encoding='utf-8')

def percentile(values, fraction):
    values = sorted(values)
    return values[min(len(values)-1, int((len(values)-1)*fraction))]

def finish(status):
    st['status'] = status
    save()
    unreal.unregister_slate_post_tick_callback(st['handle'])
    if base['status'] == 'RUNNING':
        unreal.unregister_slate_post_tick_callback(base['handle'])
        base['status'] = 'OWNED_BY_BUOYANCY_FIXTURE'
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()

def tick(dt):
    try:
        assert time.monotonic()-st['started'] < 500, 'Fixture timeout'
        assert base['status'] != 'FAILED', base.get('error')
        world = ed.get_game_world()
        if not world:
            return
        t = unreal.GameplayStatics.get_time_seconds(world)
        if st['phase'] == 'startup':
            if base['phase'] != 'observe':
                return
            unreal.unregister_slate_post_tick_callback(base['handle'])
            base['status'] = 'OWNED_BY_BUOYANCY_FIXTURE'
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
                movement = actor.get_component_by_class(unreal.ShipMovementComponent)
                if not movement:
                    continue
                nav = actor.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                nav.stop_navigation()
                nav.set_component_tick_enabled(False)
                actor.get_component_by_class(unreal.ShipCombatComponent).enable_combat = False
                actor.can_be_damaged = False
                hull = next(c for c in actor.get_components_by_class(unreal.PrimitiveComponent) if c.get_name() == 'ShipSM')
                fleet.append((actor, movement, hull))
            st.update(phase='settle', at=t, profiles={a.get_name():dict(hull_class=str(m.hull_class), diagnostics=json.loads(m.get_buoyancy_diagnostics()), **{k:str(m.profile.get_editor_property(k)) for k in ['mass_kg','linear_damping','angular_damping','inertia_scale','vertical_damping_multiplier','pontoon_radius_multiplier','displacement_multiplier','heave_damping_per_second','roll_damping_per_second','pitch_damping_per_second']}) for a,m,h in fleet})
            save()
            return
        age = t-st['at']
        if st['phase'] == 'settle':
            if age < 15:
                return
            st.update(phase='idle', at=t)
            age = 0
        if st['phase'] == 'idle' and age >= duration:
            st.update(phase='firing', at=t, next_fire=0, fire_attempts=0)
            age = 0
        if st['phase'] == 'firing' and age >= st['next_fire']:
            for actor,movement,hull in fleet:
                weapon = actor.get_component_by_class(unreal.ShipWeaponComponent)
                weapon.try_fire_weapon(unreal.Vector(), None, unreal.Vector(), False)
            st['next_fire'] += 4
            st['fire_attempts'] += 1
        if st['phase'] == 'firing' and age > 7 and not st.get('camera_selected'):
            unreal.GameplayStatics.get_player_controller(world,0).call_method('CycleShipCamera')
            st['camera_selected'] = True
        elif st['phase'] == 'firing' and age > 9 and not st.get('capture'):
            st['capture'] = unreal.ShipCombatLibrary.capture_gameplay_hud(world,str(root/'Saved'/name.replace('.json','.png')))
        if t-st.get('last', -1) >= .05:
            st['last'] = t
            ocean = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.WaterBodyOcean)[0].get_water_body_component()
            for actor,movement,hull in fleet:
                pos = hull.get_world_location()
                rot = hull.get_world_rotation()
                velocity = hull.get_physics_linear_velocity()
                angular = hull.get_physics_angular_velocity_in_degrees()
                surface = ocean.get_water_surface_info_at_location(pos, False)[0]
                st['rows'].append(dict(t=t, phase=st['phase'], ship=actor.get_name(), cls=str(movement.hull_class), roll=rot.roll, pitch=rot.pitch, vz=velocity.z/100, root_z=pos.z/100, heave=(pos.z-surface.z)/100, angular=list(angular.to_tuple()), horizontal_speed=math.hypot(velocity.x,velocity.y)/100, physical=hull.is_simulating_physics(), water=actor.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body(), thrust=movement.last_propulsion_force.length(), rudder=movement.last_rudder_force.length(), shots=actor.get_component_by_class(unreal.ShipWeaponComponent).shots_fired))
        if st['phase'] == 'firing' and age >= duration:
            st['summary'] = {}
            for actor,movement,hull in fleet:
                result = st['summary'].setdefault(actor.get_name(), {})
                for phase in ['idle','firing']:
                    rows = [r for r in st['rows'] if r['ship'] == actor.get_name() and r['phase'] == phase]
                    result[phase] = dict(cls=str(movement.hull_class), samples=len(rows), p95_roll=percentile([abs(r['roll']) for r in rows],.95), p95_pitch=percentile([abs(r['pitch']) for r in rows],.95), max_roll=max(abs(r['roll']) for r in rows), max_pitch=max(abs(r['pitch']) for r in rows), p99_vz=percentile([abs(r['vz']) for r in rows],.99), heave_range=max(r['heave'] for r in rows)-min(r['heave'] for r in rows), root_z_range=max(r['root_z'] for r in rows)-min(r['root_z'] for r in rows), water_fraction=sum(r['water'] for r in rows)/len(rows), physical=all(r['physical'] for r in rows), no_drive=all(r['thrust'] < .01 and r['rudder'] < .01 for r in rows), shots=max(r['shots'] for r in rows))
                    trim=statistics.mean(r['pitch'] for r in rows)
                    result[phase].update(mean_pitch_trim=trim,p95_pitch_oscillation=percentile([abs(r['pitch']-trim) for r in rows],.95))
            assert all(v['firing']['shots'] >= 8 for v in st['summary'].values()), 'Shared weapon did not fire enough shots'
            assert all(v[p]['physical'] and v[p]['no_drive'] and v[p]['water_fraction'] >= .98 for v in st['summary'].values() for p in ['idle','firing']), 'Invalid stationary physical fixture'
            assert all(not v['diagnostics']['async'] and abs(v['diagnostics']['weight_sum']-1)<.001 for v in st['profiles'].values()), 'Profile buoyancy not authoritative/normalized'
            if globals().get('ASSERT_COMFORT',True):
                # Authored bow/stern support heights give the original hull about
                # one degree of steady trim. Limit oscillation separately from trim.
                assert all(v[p]['p95_roll']<2 and v[p]['p95_pitch_oscillation']<1 and v[p]['p99_vz']<1.2 and v[p]['max_roll']<3 and v[p]['max_pitch']<3 for v in st['summary'].values() for p in ['idle','firing']), 'Stationary comfort limits exceeded'
            finish('PASSED')
            return
        if int(t)%5 == 0 and int(t) != st.get('saved_second'):
            st['saved_second'] = int(t)
            save()
    except Exception:
        st['error'] = traceback.format_exc()
        finish('FAILED')

st['handle'] = unreal.register_slate_post_tick_callback(tick)
save()
