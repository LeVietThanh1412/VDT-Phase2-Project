"""Reproduce a same-heading stationary-AI queue using explicit fixture placement.
Native navigation, priority leases, hull sweeps and propulsion own all later motion.
"""
import unreal,builtins,json,time,math,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());p=root/'Tools/Testing/verify_final_demo.py'
ctx=dict(KIND='patrol',FULL_AI=True,TACTICS=False,ASSAULT=False,HULL_CLASSES=[0,2,2,2,2,2],OBSERVE_SECONDS=99999,OUTPUT='TRAFFIC_STARTUP.json',CAPTURE=False)
exec(compile(p.read_text(encoding='utf-8'),str(p),'exec'),ctx);base=ctx['st']
s=dict(status='RUNNING',phase='startup',checks={},rows=[],error=None,native_dll_sha256=base['native_dll_sha256']);builtins.stationary_traffic_probe=s
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
def sub(w,c):return lib.call_method('GetWorldSubsystem',args=(w,c.static_class()))
def hull(a):return next(x for x in a.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM')
def arrow(a):return next(x for x in a.get_components_by_class(unreal.ArrowComponent) if x.get_name()=='Arrow')
def save():
 (root/'Saved/STATIONARY_TRAFFIC_REGRESSION.json').write_text(json.dumps({k:v for k,v in s.items() if k not in ['handle','subject','lead']},indent=2),encoding='utf-8')
def finish(status):
 s['status']=status;save();unreal.unregister_slate_post_tick_callback(s['handle'])
 if base['status']=='RUNNING':unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_TRAFFIC_FIXTURE'
 if status=='PASSED' or not globals().get('HOLD_FAILURE',False):unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def place(grid,a,en,direction):
 h=hull(a);target=grid.chart_to_world(unreal.Vector2D(*en));target.z=h.get_world_location().z
 offset=(arrow(a).get_world_rotation().yaw-h.get_world_rotation().yaw+180)%360-180
 forward=grid.chart_to_world(unreal.Vector2D(en[0]+direction[0],en[1]+direction[1]))-grid.chart_to_world(unreal.Vector2D(*en))
 anchor=a.get_component_by_class(unreal.CesiumGlobeAnchorComponent);geo=anchor.resolve_georeference()
 anchor.move_to_longitude_latitude_height(geo.transform_unreal_position_to_longitude_latitude_height(target));anchor.sync()
 h.set_world_rotation(unreal.Rotator(pitch=0,yaw=math.degrees(math.atan2(forward.y,forward.x))-offset,roll=0),False,True)
 h.set_physics_linear_velocity(unreal.Vector());h.set_physics_angular_velocity_in_degrees(unreal.Vector());anchor.sync()
def tick(dt):
 try:
  w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  if not w:return
  t=unreal.GameplayStatics.get_time_seconds(w);grid=sub(w,unreal.NavalSectorSubsystem);field=sub(w,unreal.NavalBattlefieldSubsystem)
  if s['phase']=='startup':
   assert base['status']=='RUNNING',base.get('error')
   if base['phase']!='observe':return
   fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
   subject=next(a for a in fleet if a.get_component_by_class(unreal.ShipMovementComponent).hull_class==unreal.ShipHullClass.SMALL)
   lead=next(a for a in fleet if a!=subject)
   # The coordinates reproduce the stopped queue observed in the natural run.
   start=[13938,4095];block=[13732,3996];goal=[13762.5,4112.5];direction=[-1,-.4]
   assert json.loads(field.inspect_navigation_route(subject,unreal.Vector2D(*start),unreal.Vector2D(*goal)))['success']
   for a in fleet:
    a.can_be_damaged=False
    if c:=a.get_component_by_class(unreal.ShipCombatComponent):c.enable_combat=False
    n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent);n.stop_navigation()
    if a not in [subject,lead]:
     a.set_actor_tick_enabled(False);hull(a).set_simulate_physics(False);a.set_actor_location(a.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
   place(grid,subject,start,direction);place(grid,lead,block,direction)
   n=subject.get_component_by_class(unreal.ShipEnemyNavigationComponent)
   s.update(phase='observe',subject=subject,lead=lead,start=start,goal=goal,at=t,controller=subject.get_controller().get_name(),minimum_gap_m=99999)
   s['checks']['same_heading']=sum(a*b for a,b in zip(arrow(subject).get_forward_vector().to_tuple(),arrow(lead).get_forward_vector().to_tuple()))>.95
   # A disabled combat component deliberately releases tactical overrides.
   # Use the public destination command for this isolated navigation fixture.
   n.set_geographic_destination(n.world_to_geographic(grid.chart_to_world(unreal.Vector2D(*goal))))
   unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_TRAFFIC_FIXTURE';save();return
  subject=s['subject'];lead=s['lead'];n=subject.get_component_by_class(unreal.ShipEnemyNavigationComponent);pos=grid.world_to_chart(hull(subject).get_world_location());gap=(hull(subject).get_world_location()-hull(lead).get_world_location()).length()/100
  distance=math.dist(pos.to_tuple(),s['goal']);s['minimum_gap_m']=min(s['minimum_gap_m'],gap)
  if not s['rows'] or t-s['rows'][-1]['t']>.1:
   s['rows'].append(dict(t=t,age=t-s['at'],distance_m=distance,gap_m=gap,priority=n.temporary_traffic_priority,yielding=n.yielding_to_ship,wait=n.traffic_wait_seconds,throttle=n.desired_throttle,speed=hull(subject).get_physics_linear_velocity().length()/100,lead_speed=hull(lead).get_physics_linear_velocity().length()/100,physics=hull(subject).is_simulating_physics(),in_water=subject.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body(),en=list(pos.to_tuple())))
  if t-s['at']>2 and distance<=n.arrival_radius_m+3:
   s['checks'].update(reached_goal=True,physical_progress=math.dist(pos.to_tuple(),s['start'])>75,stationary_lead=max(r['lead_speed'] for r in s['rows'])<1,priority_exercised=any(r['priority'] for r in s['rows']),water_and_physics=all(r['physics'] and r['in_water'] for r in s['rows']),controller_preserved=subject.get_controller().get_name()==s['controller'],physical_gap=s['minimum_gap_m']>70)
   assert all(s['checks'].values()),s['checks'];s['duration']=t-s['at'];finish('PASSED');return
  assert t-s['at']<180,'Stationary same-heading queue did not clear within bounded priority windows'
  if int(t)%5==0:save()
 except Exception:s['error']=traceback.format_exc();finish('FAILED')
s['handle']=unreal.register_slate_post_tick_callback(tick);save()
