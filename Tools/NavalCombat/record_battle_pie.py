"""Read-only six-ship PIE telemetry with a bounded automatic stop.

Optional fixture flags must be explicitly supplied in the isolated namespace;
the default run changes neither ships, combat settings, nor simulation speed.
"""
import builtins,json,math,time
from pathlib import Path
import unreal

w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
root=Path(unreal.Paths.project_dir());chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
cls=unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C')
ships=unreal.GameplayStatics.get_all_actors_of_class(w,cls);assert len(ships)==6,len(ships)
old=getattr(builtins,'battle_probe',None)
if old and old.get('handle'):unreal.unregister_slate_post_tick_callback(old['handle'])
v=lambda p:[p.x,p.y,p.z]
rot=lambda p:[p.pitch,p.yaw,p.roll]
s=dict(handle=None,last=-1,rows=[],started=unreal.GameplayStatics.get_time_seconds(w),started_wall=time.monotonic(),
    path=root/'Docs/NavalCombat'/globals().get('OUTPUT','BATTLE_DEFAULT_PIE.json'),status='RUNNING',deadline=globals().get('DEADLINE',420),
    minimum_pair_m=1e9,error=None,fixture=dict(disable_combat=globals().get('DISABLE_COMBAT',False),prevent_destruction=globals().get('PREVENT_DESTRUCTION',False)))
builtins.battle_probe=s
if globals().get('DISABLE_COMBAT'):
    for ship in ships:
        c=ship.get_component_by_class(unreal.ShipCombatComponent)
        if c:c.enable_combat=False
if globals().get('PREVENT_DESTRUCTION'):
    for ship in ships:ship.can_be_damaged=False
def metric(ship,p):
    g=ship.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
    origin=unreal.GameplayStatics.get_world_origin_location(w)
    ecef=v(g.transform_unreal_position_to_earth_centered_earth_fixed(p+unreal.Vector(origin.x,origin.y,origin.z)))
    d=[ecef[i]-chart['origin_ecef'][i] for i in range(3)]
    return [sum(d[i]*chart[k][i] for i in range(3)) for k in ['east_axis','north_axis']]
def sample(ship):
    weapon=ship.get_component_by_class(unreal.ShipWeaponComponent);c=ship.get_component_by_class(unreal.ShipCombatComponent);n=ship.get_component_by_class(unreal.ShipEnemyNavigationComponent)
    h=next(x for x in ship.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM')
    mesh=ship.get_component_by_class(unreal.SkeletalMeshComponent);animation=mesh.get_anim_instance()
    row=dict(label=ship.get_actor_label(),name=ship.get_name(),team=weapon.team(),player=ship.is_player_controlled(),
        controller=str(ship.get_controller().get_class().get_name()) if ship.get_controller() else None,
        position=v(h.get_world_location()),en_m=metric(ship,h.get_world_location()),velocity=v(h.get_physics_linear_velocity()),
        hull_rotation=rot(h.get_world_rotation()),physics=h.is_simulating_physics(),buoyancy=bool(ship.get_component_by_class(unreal.BuoyancyComponent)),
        health=weapon.current_health,destroyed=weapon.is_destroyed,shots=weapon.shots_fired,hits=weapon.hits,
        bone_h=rot(mesh.get_socket_rotation('front_gun_h_jnt')),bone_v=rot(mesh.get_socket_rotation('front_gun_v_jnt')),
        muzzle=v(mesh.get_socket_location('front_gun_v_socket')),muzzle_rotation=rot(mesh.get_socket_rotation('front_gun_v_socket')),
        anim_yaw=animation.get_editor_property('GunYaw'),anim_pitch=animation.get_editor_property('GunPitch'))
    if c:row.update(target=c.target.get_actor_label() if c.target else None,target_team=c.target.get_component_by_class(unreal.ShipWeaponComponent).team() if c.target else None,
        distance=c.distance_to_target_m,tactic=str(c.tactical_state),retreats=c.retreats_started,resumed=c.patrols_resumed,
        yaw=c.turret_yaw,pitch=c.barrel_pitch,desired_yaw=c.desired_turret_yaw,desired_pitch=c.desired_barrel_pitch,
        status=c.firing_status,cooldown=c.cooldown_seconds_remaining,visible=c.target_visible,solution=c.has_firing_solution,
        launch=v(c.launch_velocity),intercept=v(c.predicted_intercept))
    if n:row.update(nav=str(n.navigation_state),failure=n.last_navigation_failure_reason,path=n.pathfinding_succeeded,
        route=[v(x) for x in n.current_route],legs=n.patrol_legs_completed,waypoint=n.current_waypoint_index,
        a=n.patrol_actor_a.get_actor_label(),b=n.patrol_actor_b.get_actor_label(),active=ship.get_editor_property('DestinationActor').get_actor_label() if ship.get_editor_property('DestinationActor') else None,
        tactical_goal=n.has_tactical_destination(),goal_llh=v(n.current_final_destination),
        throttle=n.desired_throttle,rudder=n.desired_rudder,bp_throttle=ship.get_editor_property('AIThrottle'),bp_rudder=ship.get_editor_property('AIRudder'),yielding=n.yielding_to_ship)
    return row
def save():
    s['path'].write_text(json.dumps({k:s[k] for k in ['status','rows','minimum_pair_m','error','fixture']},indent=2),encoding='utf8')
def finish(status):
    s['status']=status;save();unreal.unregister_slate_post_tick_callback(s['handle']);s['handle']=None
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t-s['last']<1:return
    s['last']=t
    try:
        rows=[sample(x) for x in ships]
        minimum=min(math.dist(a['en_m'],b['en_m']) for i,a in enumerate(rows) for b in rows[i+1:])
        s['minimum_pair_m']=min(minimum,s['minimum_pair_m']);s['rows'].append(dict(t=t,ships=rows,minimum_pair_m=minimum))
        if len(s['rows'])%5==0:save()
        if t-s['started']>=s['deadline']:finish('BOUNDED_RUN_COMPLETE')
        elif s['fixture']['disable_combat'] and all(x.get('legs',0)>=2 for x in rows if not x['player']):finish('ALL_FIVE_AI_A_B_A')
    except Exception as error:s['error']=str(error);finish('PROBE_ERROR')
s['handle']=unreal.register_slate_post_tick_callback(tick)
print('Six-ship telemetry started:',s['path'],s['fixture'])
