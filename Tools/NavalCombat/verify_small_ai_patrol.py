"""Small milestone only: runtime fixture on previously certified old patrols.

Does not alter saved actors and is NOT the new randomized battlefield soak.
"""
import builtins,json,math,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);assert ed.get_game_world() is None
plan=json.loads((root/'Docs/QuangNinhIntegration/ORIGIN_PATROL_PLAN.json').read_text())
st=dict(handle=None,status='RUNNING',phase='init',rows=[],checks=[],error=None,last=-1)
builtins.small_ai_patrol=st
classes={'Friendly_AI_1':'SMALL','Friendly_AI_2':'MEDIUM','BP_ShipEnemy':'SMALL','BP_ShipEnemy2':'LARGE','BP_ShipEnemy3':'SMALL'}
def body(a):return next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
def save():
    (root/'Docs/NavalCombat/SMALL_AI_PATROL.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','ships','initial','controllers']},indent=2),encoding='utf-8')
def check(name,value):
    st['checks'].append(dict(name=name,passed=bool(value)));assert value,name
def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=ed.get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<2:return
    try:
        if st['phase']=='init':
            ships={a.get_actor_label():a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)}
            st['ships']=ships;st['initial']={};st['controllers']={}
            for a in ships.values():
                a.can_be_damaged=False;c=a.get_component_by_class(unreal.ShipCombatComponent)
                if c:c.enable_combat=False
            for spec in plan['ships']:
                a=ships[spec['label']];h=body(a);h.set_simulate_physics(False)
                m=a.get_component_by_class(unreal.ShipMovementComponent);check('profile '+spec['label'],m.apply_ship_class(getattr(unreal.ShipHullClass,classes[spec['label']])))
                anchor=a.get_component_by_class(unreal.CesiumGlobeAnchorComponent);g=anchor.resolve_georeference();anchor.move_to_longitude_latitude_height(unreal.Vector(*spec['spawn_llh']));anchor.set_east_south_up_rotation(unreal.Rotator(yaw=spec['yaw']).quaternion())
                n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                for key in ['a','b']:
                    target=n.patrol_actor_a if key=='a' else n.patrol_actor_b
                    target.set_actor_location(g.transform_longitude_latitude_height_position_to_unreal(unreal.Vector(*plan['target_llh'][spec[key]])),False,True)
                n.patrol_offset_a=unreal.Vector();n.patrol_offset_b=unreal.Vector();n.start_at_patrol_b=True;n.patrol_wait_seconds=3
                n.stop_navigation();st['initial'][spec['label']]=a.get_actor_location();st['controllers'][spec['label']]=a.get_controller().get_path_name()
            st['phase']='staged';save()
        if st['phase']=='staged' and t>5:
            for label in classes:
                a=st['ships'][label];body(a).set_simulate_physics(True);a.get_component_by_class(unreal.ShipEnemyNavigationComponent).start_patrol()
            st['phase']='run'
        if st['phase']=='run' and t-st['last']>1:
            st['last']=t;records=[]
            for label,kind in classes.items():
                a=st['ships'][label];n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent);m=a.get_component_by_class(unreal.ShipMovementComponent);h=body(a);wp=a.get_component_by_class(unreal.ShipWeaponComponent)
                records.append(dict(label=label,kind=kind,team=wp.team(),hp=wp.current_health,speed=m.signed_forward_speed(),physics=h.is_simulating_physics(),water=a.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body(),distance=(a.get_actor_location()-st['initial'][label]).length()/100,nav=str(n.navigation_state),failure=n.last_navigation_failure_reason,waypoint=n.current_waypoint_index,route=len(n.current_route),controller_same=a.get_controller().get_path_name()==st['controllers'][label],throttle=a.get_editor_property('AIThrottle'),rudder=a.get_editor_property('AIRudder')))
            st['rows'].append(dict(t=t,ships=records));save()
        if t>75:
            for label,kind in classes.items():
                rows=[s for r in st['rows'] for s in r['ships'] if s['label']==label]
                check(label+' real original-force movement',max(r['speed'] for r in rows)>10 and max(r['distance'] for r in rows)>300)
                check(label+' retained AI controller',all(r['controller_same'] and r['physics'] for r in rows))
                check(label+' no Failed nav',all('FAILED' not in r['nav'] for r in rows))
                if kind=='SMALL':check(label+' correct Small HP',all(r['hp']==38 for r in rows))
            finish('PASSED')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick);save();unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
