"""Disposable real-physics role test. Only non-subject fixture ships are placed/frozen.
The subject uses its original propulsion, rudder, patrol references and normal time.
"""
import base64,builtins,json,math,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
cells=base64.b64decode(chart['cells_base64']);size=chart['cell_size']
st=dict(handle=None,phase='init',rows=[],checks=[],last=-1,at=0,error=None)
builtins.role_test=st
def hull(s):return next(x for x in s.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM')
def metric(s):
    g=s.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
    p=g.transform_unreal_position_to_earth_centered_earth_fixed(hull(s).get_world_location());v=[p.x,p.y,p.z]
    return [sum((v[i]-chart['origin_ecef'][i])*chart[k][i] for i in range(3)) for k in ['east_axis','north_axis']]
def place(s,distance):
    p=metric(st['subject']);desired=[p[0],p[1]+distance];options=[]
    for y in range(chart['height']):
        for x in range(chart['width']):
            if cells[y*chart['width']+x]:
                q=[chart['min_east']+(x+.5)*size,chart['min_north']+(y+.5)*size]
                if abs(math.dist(p,q)-distance)<40:options.append(q)
    q=min(options,key=lambda q:math.dist(q,desired));g=s.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
    ecef=unreal.Vector(*chart['origin_ecef'])+unreal.Vector(*chart['east_axis'])*q[0]+unreal.Vector(*chart['north_axis'])*q[1]
    llh=g.transform_unreal_position_to_longitude_latitude_height(g.transform_earth_centered_earth_fixed_position_to_unreal(ecef));llh.z=14.7
    s.set_actor_location(g.transform_longitude_latitude_height_position_to_unreal(llh),False,True)
def check(name,value):
    st['checks'].append(dict(name=name,passed=bool(value)))
    assert value,name
def save():
    (root/'Docs/NavalCombat/ROLES_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','subject','threat','ally']},indent=2))
def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle'])
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def phase(name,t):st.update(phase=name,at=t)
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<1:return
    try:
        if st['phase']=='init':
            ships=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'))
            subject=next(s for s in ships if s.get_actor_label()=='Friendly_AI_1')
            threat=next(s for s in ships if s.get_actor_label()=='BP_ShipEnemy')
            ally=next(s for s in ships if s.is_player_controlled())
            st.update(subject=subject,threat=threat,ally=ally)
            for s in ships:
                c=s.get_component_by_class(unreal.ShipCombatComponent);n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=s==subject
                if s!=subject:
                    s.can_be_damaged=False
                    if n:n.stop_navigation()
                    hull(s).set_simulate_physics(False)
                    if s!=threat:s.set_actor_location(s.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
            for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                if a.get_class().get_name().startswith('BP_ShipProjectile'):a.destroy_actor()
            c=subject.get_component_by_class(unreal.ShipCombatComponent);n=subject.get_component_by_class(unreal.ShipEnemyNavigationComponent)
            c.enable_hit_and_run=False;c.set_combat_role(unreal.ShipEnemyRole.PASSIVE)
            st['anchors']=[n.patrol_actor_a.get_path_name(),n.patrol_actor_b.get_path_name()]
            st['start']=metric(subject);place(threat,650);phase('passive',t)
        s=st['subject'];c=s.get_component_by_class(unreal.ShipCombatComponent);n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent);weapon=s.get_component_by_class(unreal.ShipWeaponComponent)
        assert [n.patrol_actor_a.get_path_name(),n.patrol_actor_b.get_path_name()]==st['anchors'],'patrol references changed'
        if t-st['last']>=.5:
            st['last']=t;st['rows'].append(dict(t=t,phase=st['phase'],position=metric(s),speed=hull(s).get_physics_linear_velocity().length()/100,
                role=str(n.role),state=str(c.tactical_state),reason=c.last_role_transition,shots=weapon.shots_fired,hp=weapon.current_health,
                pursuit_routes=c.hunter_routes_started,resumed=c.patrols_resumed,tactical=n.has_tactical_destination(),failure=n.last_navigation_failure_reason,
                target=c.target.get_actor_label() if c.target else None,visible=c.target_visible,target_distance_m=c.distance_to_target_m))
        elapsed=t-st['at']
        if st['phase']=='passive' and elapsed>=9:
            check('passive never auto-fired',weapon.shots_fired==0)
            check('passive navigates physically',math.dist(st['start'],metric(s))>20)
            check('friendly damage rejected',not weapon.receive_ship_damage(1,st['ally']) and n.role==unreal.ShipEnemyRole.PASSIVE)
            check('hostile damage activates Fighter',weapon.receive_ship_damage(1,st['threat']) and n.role==unreal.ShipEnemyRole.FIGHTER)
            phase('fighter',t)
        elif st['phase']=='fighter' and elapsed>=9:
            check('fighter acquired hostile',c.target==st['threat'])
            c.set_combat_role(unreal.ShipEnemyRole.PASSIVE);place(st['threat'],600)
            check('unconfigured alert inert',not c.raise_threat_alert(st['threat']))
            c.promote_passive_on_threat_alert=True
            check('configured threat activates Fighter',c.raise_threat_alert(st['threat']) and n.role==unreal.ShipEnemyRole.FIGHTER)
            c.set_combat_role(unreal.ShipEnemyRole.PASSIVE);c.set_combat_role(unreal.ShipEnemyRole.FIGHTER)
            check('explicit command activates Fighter',n.role==unreal.ShipEnemyRole.FIGHTER)
            c.engagement_radius_m=300;c.hunter_max_pursuit_seconds=30;place(st['threat'],1400)
            c.set_combat_role(unreal.ShipEnemyRole.HUNTER);st['hunter_start']=metric(s);phase('hunter',t)
        elif st['phase']=='hunter' and elapsed>=12:
            check('Hunter planned geographic pursuit',c.hunter_routes_started>0)
            check('Hunter physically moved',math.dist(st['hunter_start'],metric(s))>20)
            check('Hunter still bounded active',n.role==unreal.ShipEnemyRole.HUNTER and n.has_tactical_destination())
            c.detection_radius_m=300;c.lose_target_radius_m=300
            st['resume_start']=metric(s);phase('lost',t)
        elif st['phase']=='lost' and elapsed>=8:
            check('lost Hunter becomes Fighter',n.role==unreal.ShipEnemyRole.FIGHTER and 'lost' in c.last_role_transition.lower())
            check('original patrol resumed',not n.has_tactical_destination() and c.patrols_resumed>=1)
            check('movement continues after target loss',math.dist(st['resume_start'],metric(s))>10)
            c.detection_radius_m=1800;c.lose_target_radius_m=2200
            place(st['threat'],1400);c.hunter_max_pursuit_seconds=3;c.set_combat_role(unreal.ShipEnemyRole.HUNTER);phase('timeout',t)
        elif st['phase']=='timeout' and elapsed>=5:
            check('Hunter timeout bounded',n.role==unreal.ShipEnemyRole.FIGHTER and 'timeout' in c.last_role_transition.lower())
            # The subject moved during the timeout case. Re-establish an actual
            # visible hostile outside the tested 500 m bound; an occluded actor
            # is not a valid precondition for testing a known-target range exit.
            place(st['threat'],800);c.scan_targets()
            check('range fixture has visible hostile beyond 500 m',c.target==st['threat'] and c.target_visible and math.dist(metric(s),metric(st['threat']))>500)
            c.hunter_max_pursuit_seconds=30;c.hunter_max_pursuit_distance_m=500;c.set_combat_role(unreal.ShipEnemyRole.HUNTER);phase('distance',t)
        elif st['phase']=='distance' and elapsed>=2:
            check('Hunter range bounded',n.role==unreal.ShipEnemyRole.FIGHTER and 'distance' in c.last_role_transition.lower())
            c.hunter_max_pursuit_distance_m=1700;c.set_combat_role(unreal.ShipEnemyRole.HUNTER)
            check('hostile health event accepted',weapon.receive_ship_damage(60,st['threat']))
            phase('health',t)
        elif st['phase']=='health' and elapsed>=2:
            check('low-health Hunter becomes Fighter',n.role==unreal.ShipEnemyRole.FIGHTER and 'health' in c.last_role_transition.lower())
            check('patrol references preserved',True);finish('ALL_ROLE_CHECKS_PASSED')
        elif t>100:raise AssertionError('role fixture timed out')
        if int(t)%5==0:save()
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
