"""PIE development relocation regression, using the real menu, survey and A*.
Only explicit fixture placements teleport; subsequent travel is the original BP physics.
"""
import unreal,json,builtins,math,time,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());script=root/'Tools/NavalCombat/test_seeded_spawn.py'
exec(compile(script.read_text(encoding='utf-8'),str(script),'exec'),dict(SEED=20261028,OUTPUT='FINAL_DYNAMIC_PATROL.json',OUTPUT_DIR='Saved',REQUIRED_LEGS=3,LOOP_LIMIT_SECONDS=1000,WALL_LIMIT_SECONDS=1400))
obs=dict(last=-1,events=[],ships_moved=False,targets_moved=False,charts_exported=False,error=None)
builtins.final_dynamic_observer=obs
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
def sub(w,c):return lib.call_method('GetWorldSubsystem',args=(w,c.static_class()))
def save():
 (root/'Saved/FINAL_DYNAMIC_RELOCATIONS.json').write_text(json.dumps({k:v for k,v in obs.items() if k!='handle'},indent=2),encoding='utf-8')
def route(field,ship,start,end):
 return json.loads(field.inspect_navigation_route(ship,unreal.Vector2D(*start),unreal.Vector2D(*end)))['success']
def observe(dt):
 st=builtins.seeded_spawn_probe;w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
 if st['status']!='RUNNING':
  obs['status']=st['status'];save();unreal.unregister_slate_post_tick_callback(obs['handle']);return
 if not w or st['phase']!='observe':return
 try:
  t=unreal.GameplayStatics.get_time_seconds(w)
  if t-obs['last']<1:return
  obs['last']=t;grid=sub(w,unreal.NavalSectorSubsystem);field=sub(w,unreal.NavalBattlefieldSubsystem)
  fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
  ai=[a for a in fleet if a.get_component_by_class(unreal.ShipEnemyNavigationComponent)]
  if not obs['charts_exported']:
   for a in ai:
    cls=str(a.get_component_by_class(unreal.ShipMovementComponent).hull_class)
    (root/'Saved'/('FINAL_OPERATIONAL_CHART_'+cls.split('.')[1].split(':')[0]+'.json')).write_text(field.inspect_navigation_chart(a),encoding='utf-8')
   obs['charts_exported']=True
  age=t-st['ready_at']
  if age>=12 and not obs['ships_moved']:
   for a in ai:
    n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent);h=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
    old=list(grid.world_to_chart(h.get_world_location()).to_tuple());goal=list(grid.world_to_chart(n.geographic_to_world(n.current_final_destination)).to_tuple())
    chosen=None
    for radius in [100,125,150]:
     for angle in range(0,360,30):
      q=[old[0]+radius*math.cos(math.radians(angle)),old[1]+radius*math.sin(math.radians(angle))]
      if any(math.dist(q,grid.world_to_chart(o.get_actor_location()).to_tuple())<180 for o in fleet if o!=a):continue
      if route(field,a,q,goal) and field.validate_development_placement(a,unreal.Vector2D(*q),a.get_actor_rotation().yaw):chosen=q;break
     if chosen:break
    assert chosen,'No physically safe relocation for '+a.get_actor_label()
    wp=grid.chart_to_world(unreal.Vector2D(*chosen));wp.z=h.get_world_location().z
    anchor=a.get_component_by_class(unreal.CesiumGlobeAnchorComponent);g=anchor.resolve_georeference()
    before=n.reposition_reinitializations
    anchor.move_to_longitude_latitude_height(g.transform_unreal_position_to_longitude_latitude_height(wp));anchor.sync()
    h.set_physics_linear_velocity(unreal.Vector());h.set_physics_angular_velocity_in_degrees(unreal.Vector())
    obs['events'].append(dict(ship=a.get_actor_label(),kind='ship_relocation',t=t,old_en=old,new_en=chosen,prior_reinitializations=before))
   obs['ships_moved']=True;save()
  if age>=35 and obs['ships_moved'] and not obs['targets_moved']:
   for a in ai:
    n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent);d=json.loads(n.get_navigation_diagnostics())
    e=next(e for e in obs['events'] if e['ship']==a.get_actor_label() and e['kind']=='ship_relocation')
    e['auto_reinitialized']=n.reposition_reinitializations>e['prior_reinitializations'];assert e['auto_reinitialized'],e
    marker=next(p for p in [n.patrol_actor_a,n.patrol_actor_b] if p.get_name()==d['active_target'])
    other=n.patrol_actor_b if marker==n.patrol_actor_a else n.patrol_actor_a
    old=list(grid.world_to_chart(marker.get_actor_location()).to_tuple());start=list(grid.world_to_chart(a.get_actor_location()).to_tuple());chosen=None
    for radius in [100,125,150]:
     for angle in range(0,360,30):
      q=[old[0]+radius*math.cos(math.radians(angle)),old[1]+radius*math.sin(math.radians(angle))]
      if math.dist(q,grid.world_to_chart(other.get_actor_location()).to_tuple())<=2*n.arrival_radius_m+20:continue
      if route(field,a,start,q) and field.validate_development_placement(a,unreal.Vector2D(*q),a.get_actor_rotation().yaw):chosen=q;break
     if chosen:break
    assert chosen,'No safe moved goal for '+a.get_actor_label()
    wp=grid.chart_to_world(unreal.Vector2D(*chosen));wp.z=marker.get_actor_location().z
    marker.set_actor_location(wp,False,False)
    if anchor:=marker.get_component_by_class(unreal.CesiumGlobeAnchorComponent):anchor.sync()
    obs['events'].append(dict(ship=a.get_actor_label(),kind='live_target_relocation',marker=marker.get_name(),t=t,old_en=old,new_en=chosen))
   obs['targets_moved']=True;save()
  if age>=40 and obs['targets_moved']:
   for a in ai:
    n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
    for e in obs['events']:
     if e['ship']==a.get_actor_label() and e['kind']=='live_target_relocation' and 'new_goal_verified' not in e:
      d=json.loads(n.get_navigation_diagnostics())
      if d['active_target']==e['marker'] and n.current_route:
       e['new_goal_error_m']=math.dist(n.current_route[-1].to_tuple()[:2],e['new_en']);e['new_goal_verified']=e['new_goal_error_m']<1;assert e['new_goal_verified'],e
   save()
 except Exception:
  obs['error']=traceback.format_exc();save();unreal.unregister_slate_post_tick_callback(obs['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
obs['handle']=unreal.register_slate_post_tick_callback(observe)
