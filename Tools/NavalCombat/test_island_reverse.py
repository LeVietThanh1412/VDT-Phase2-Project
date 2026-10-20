"""Critical-health escape with actual chart land behind the initial fixture hull."""
import builtins,json,math,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
exec((root/'Tools/NavalCombat/test_roles.py').read_text().split('def tick(dt):')[0])
st=dict(handle=None,phase='init',rows=[],checks=[],last=-1,at=0,error=None)
builtins.island_reverse_test=st
fixture=json.loads((root/'Docs/NavalCombat/ISLAND_REVERSE_FIXTURE.json').read_text())
def set_en(s,p,yaw=None):
    g=s.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
    ecef=unreal.Vector(*chart['origin_ecef'])+unreal.Vector(*chart['east_axis'])*p[0]+unreal.Vector(*chart['north_axis'])*p[1]
    llh=g.transform_unreal_position_to_longitude_latitude_height(g.transform_earth_centered_earth_fixed_position_to_unreal(ecef));llh.z=14.7
    s.set_actor_location(g.transform_longitude_latitude_height_position_to_unreal(llh),False,True)
    if yaw is not None:s.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),True)
def finish(status):
    st['status']=status
    (root/'Docs/NavalCombat/ISLAND_REVERSE_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','subject','threat']},indent=2))
    unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<1:return
    try:
        if st['phase']=='init':
            ships=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'))
            s=next(s for s in ships if s.get_actor_label()=='Friendly_AI_1');threat=next(s for s in ships if s.get_actor_label()=='BP_ShipEnemy');st.update(subject=s,threat=threat)
            for other in ships:
                c=other.get_component_by_class(unreal.ShipCombatComponent);n=other.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=False
                if other!=s:
                    other.can_be_damaged=False
                    if n:n.stop_navigation()
                    hull(other).set_simulate_physics(False)
                    if other!=threat:other.set_actor_location(other.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
            set_en(s,fixture['p'],-fixture['heading_en_deg']);set_en(threat,fixture['target'])
            n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent);n.start_patrol()
            arrow=next(x for x in s.get_components_by_class(unreal.SceneComponent) if x.get_name()=='Arrow')
            st['fixture_actual']=dict(position=metric(s),actor_rotation=str(s.get_actor_rotation()),forward=str(arrow.get_forward_vector()))
            rear_ok=n.begin_backward_retreat(threat,300)
            st['fixture_actual']['reverse_goal']=str(n.current_final_destination)
            check('island blocks full rear commitment',not rear_ok)
            st['start']=metric(s);c=s.get_component_by_class(unreal.ShipCombatComponent);c.enable_combat=True
            check('critical 15 HP event',s.get_component_by_class(unreal.ShipWeaponComponent).receive_ship_damage(85,threat))
            st.update(phase='escape',at=t)
        s=st['subject'];n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent);c=s.get_component_by_class(unreal.ShipCombatComponent)
        if t-st['last']>.25:
            st['last']=t;st['rows'].append(dict(t=t,position=metric(s),speed=hull(s).get_physics_linear_velocity().length()/100,state=str(c.tactical_state),reverse=n.backward_retreat_active,
                retreats=c.retreats_started,fallbacks=c.unsafe_reverse_fallbacks,route=len(n.current_route),failure=n.last_navigation_failure_reason))
        if t-st['at']>=25:
            check('unsafe reverse uses forward escape',c.unsafe_reverse_fallbacks>=1 and c.backward_retreats_started==0)
            check('escape physically moves',math.dist(st['start'],metric(s))>30)
            check('escape route avoids blocked cells',all(cells[math.floor((r['position'][1]-chart['min_north'])/size)*chart['width']+math.floor((r['position'][0]-chart['min_east'])/size)] for r in st['rows']))
            finish('ISLAND_CRITICAL_RETREAT_PASSED')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
