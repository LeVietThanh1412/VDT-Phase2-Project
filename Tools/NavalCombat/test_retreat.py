"""Controlled encounter in real water: patrol, moving fire, proximity retreat, return.
Only the stationary threat is placed for the fixture. The subject AI moves solely
through its saved A*, propulsion and rudder implementation, with normal time.
"""
import builtins,json,math,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
import base64
cells=base64.b64decode(chart['cells_base64']);size=chart['cell_size']
st=dict(handle=None,phase='initialize',rows=[],last=-1,at=1,error=None)
builtins.retreat_test=st
def vec(p):return [p.x,p.y,p.z]
def metric(s):
    g=s.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference();ecef=vec(g.transform_unreal_position_to_earth_centered_earth_fixed(s.get_actor_location()))
    return [sum((ecef[i]-chart['origin_ecef'][i])*chart[k][i] for i in range(3)) for k in ['east_axis','north_axis']]
def place_threat(subject,threat,distance,behind):
    p=metric(subject);forward=subject.get_actor_forward_vector();g=subject.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
    # Chart east/north follow world X/-Y in this inspected local georeference.
    sign=-1 if behind else 1;desired=[p[0]+sign*forward.x*distance,p[1]-sign*forward.y*distance]
    options=[]
    for y in range(chart['height']):
        for x in range(chart['width']):
            if not cells[y*chart['width']+x]:continue
            q=[chart['min_east']+(x+.5)*size,chart['min_north']+(y+.5)*size]
            d=math.dist(p,q)
            if abs(d-distance)<60:options.append(q)
    q=min(options,key=lambda q:math.dist(q,desired));ecef=unreal.Vector(*chart['origin_ecef'])+unreal.Vector(*chart['east_axis'])*q[0]+unreal.Vector(*chart['north_axis'])*q[1]
    llh=g.transform_unreal_position_to_longitude_latitude_height(g.transform_earth_centered_earth_fixed_position_to_unreal(ecef));llh.z=6.8
    threat.set_actor_location(g.transform_longitude_latitude_height_position_to_unreal(llh),False,True)
def save():
    (root/'Docs/NavalCombat/RETREAT_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','subject','threat']},indent=2))
def finish(w,status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<1:return
    try:
        if st['phase']=='initialize':
            ships=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'))
            subject=next(s for s in ships if s.get_actor_label()=='Friendly_AI_1');threat=next(s for s in ships if s.get_actor_label()=='BP_ShipEnemy')
            for s in ships:
                s.can_be_damaged=False;c=s.get_component_by_class(unreal.ShipCombatComponent);n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=s==subject
                if s!=subject:
                    if n:n.stop_navigation()
                    h=next(x for x in s.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM');h.set_simulate_physics(False)
                    if s!=threat:s.set_actor_location(s.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
            for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                if a.get_class().get_name().startswith('BP_ShipProjectile'):a.destroy_actor()
            place_threat(subject,threat,650,False);st.update(subject=subject,threat=threat,phase='engage',at=t)
        subject=st['subject'];threat=st['threat'];c=subject.get_component_by_class(unreal.ShipCombatComponent);n=subject.get_component_by_class(unreal.ShipEnemyNavigationComponent);weapon=subject.get_component_by_class(unreal.ShipWeaponComponent)
        if t-st['last']>.25:
            st['last']=t;st['rows'].append(dict(t=t,position=metric(subject),distance=math.dist(metric(subject),metric(threat)),tactic=str(c.tactical_state),
                shots=weapon.shots_fired,retreats=c.retreats_started,resumed=c.patrols_resumed,status=c.firing_status,
                speed=subject.get_velocity().length()/100,throttle=n.desired_throttle,rudder=n.desired_rudder,
                tactical_goal=n.has_tactical_destination(),path=n.pathfinding_succeeded,route=[vec(q) for q in n.current_route],
                a=n.patrol_actor_a.get_actor_label(),b=n.patrol_actor_b.get_actor_label()))
        if st['phase']=='engage' and t-st['at']>10:
            place_threat(subject,threat,350,True);st.update(phase='retreat',at=t)
        if st['phase']=='retreat' and c.patrols_resumed>=1 and not n.has_tactical_destination():
            st['phase']='returned';st['at']=t
        if st['phase']=='returned' and t-st['at']>4:finish(w,'RETURNED_TO_ORIGINAL_PATROL')
        elif t>140:finish(w,'BOUNDED_RUN_COMPLETE')
        elif len(st['rows'])%10==0:save()
    except Exception:st['error']=traceback.format_exc();finish(w,'ERROR')
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
