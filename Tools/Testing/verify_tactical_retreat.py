"""Controlled low-health/reverse-fire fixture. Subject transform/physics untouched.
Other fixture hulls are parked away; one hostile is placed in validated water.
This is not a natural match and is never counted as one.
"""
import unreal,builtins,json,time,math,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());p=root/'Tools/Testing/verify_final_demo.py'
ctx=dict(KIND='patrol',FULL_AI=False,TACTICS=False,ASSAULT=False,OBSERVE_SECONDS=99999,OUTPUT='RETREAT_STARTUP.json',CAPTURE=False)
exec(compile(p.read_text(encoding='utf-8'),str(p),'exec'),ctx);base=ctx['st']
s=dict(status='RUNNING',phase='startup',rows=[],checks={},error=None);builtins.retreat_probe=s
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
def sub(w,c):return lib.call_method('GetWorldSubsystem',args=(w,c.static_class()))
def hull(a):return next(x for x in a.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM')
def save():(root/'Saved/TACTICAL_RETREAT_REGRESSION.json').write_text(json.dumps({k:v for k,v in s.items() if k not in ['handle','subject','threat']},indent=2))
def finish(status):
 s['status']=status;save();unreal.unregister_slate_post_tick_callback(s['handle'])
 if base['status']=='RUNNING':unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_RETREAT'
 if status=='PASSED' or not globals().get('HOLD_FAILURE',False):unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
 try:
  w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  if not w:
   assert s['phase']=='startup','PIE ended before the controlled regression completed'
   return
  t=unreal.GameplayStatics.get_time_seconds(w);grid=sub(w,unreal.NavalSectorSubsystem);field=sub(w,unreal.NavalBattlefieldSubsystem)
  if s['phase']=='startup':
   if base['status']!='RUNNING':raise AssertionError(base.get('error') or base['status'])
   if base['phase']!='observe':return
   if t-s.get('last_candidate_check',-1)<1:return
   s['last_candidate_check']=t;subject=None
   fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
   for a in fleet:
    n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
    if a.get_component_by_class(unreal.ShipWeaponComponent).team()!=1 or not n or a.get_component_by_class(unreal.ShipMovementComponent).hull_class!=unreal.ShipHullClass.SMALL:continue
    arrow=next(x for x in a.get_components_by_class(unreal.SceneComponent) if x.get_name()=='Arrow')
    rear=grid.world_to_chart(hull(a).get_world_location()-arrow.get_forward_vector()*30000)
    if field.validate_development_placement(a,rear,a.get_actor_rotation().yaw) and json.loads(field.inspect_navigation_route(a,grid.world_to_chart(hull(a).get_world_location()),rear))['success']:subject=a;break
   if subject is None:
    assert t-base['ready_at']<120,'No legal reverse fixture subject after physical travel'
    return
   threat=next(a for a in fleet if a.get_component_by_class(unreal.ShipWeaponComponent).team()==0 and a.get_component_by_class(unreal.ShipCombatComponent))
   unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_RETREAT'
   for a in fleet:
    a.can_be_damaged=False
    if c:=a.get_component_by_class(unreal.ShipCombatComponent):c.enable_combat=a==subject
    if a!=subject:
     a.set_actor_tick_enabled(False)
     if n:=a.get_component_by_class(unreal.ShipEnemyNavigationComponent):n.stop_navigation()
     hull(a).set_simulate_physics(False)
     if a!=threat:a.set_actor_location(a.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
   n=subject.get_component_by_class(unreal.ShipEnemyNavigationComponent);c=subject.get_component_by_class(unreal.ShipCombatComponent)
   arrow=next(x for x in subject.get_components_by_class(unreal.SceneComponent) if x.get_name()=='Arrow');pos=grid.world_to_chart(hull(subject).get_world_location());forward=grid.world_to_chart(hull(subject).get_world_location()+arrow.get_forward_vector()*100)-pos;length=math.hypot(forward.x,forward.y);forward=forward/length
   chosen=None
   for degrees in [0,15,-15,30,-30]:
    a=math.radians(degrees);direction=unreal.Vector2D(forward.x*math.cos(a)-forward.y*math.sin(a),forward.x*math.sin(a)+forward.y*math.cos(a));q=pos+direction*650
    if field.validate_development_placement(threat,q,threat.get_actor_rotation().yaw):chosen=q;break
   assert chosen,'No safe hostile fixture placement'
   wp=grid.chart_to_world(chosen);wp.z=hull(subject).get_world_location().z;anchor=threat.get_component_by_class(unreal.CesiumGlobeAnchorComponent);geo=anchor.resolve_georeference();anchor.move_to_longitude_latitude_height(geo.transform_unreal_position_to_longitude_latitude_height(wp));anchor.sync()
   c.enable_combat=False
   s.update(subject=subject,threat=threat,phase='seek_native',at=t,anchors=[n.patrol_actor_a.get_path_name(),n.patrol_actor_b.get_path_name()],controller=subject.get_controller().get_name());save();return
  subject=s['subject'];n=subject.get_component_by_class(unreal.ShipEnemyNavigationComponent);c=subject.get_component_by_class(unreal.ShipCombatComponent);wp=subject.get_component_by_class(unreal.ShipWeaponComponent);m=subject.get_component_by_class(unreal.ShipMovementComponent)
  if s['phase']=='seek_native' and t-s.get('seek_check',-1)>=1:
   assert t-s['at']<150,'No legal native reverse opportunity during physical patrol'
   s['seek_check']=t;arrow=next(x for x in subject.get_components_by_class(unreal.SceneComponent) if x.get_name()=='Arrow')
   desired=hull(subject).get_world_location()+arrow.get_forward_vector()*65000;q=grid.world_to_chart(desired)
   if not field.validate_development_placement(s['threat'],q,s['threat'].get_actor_rotation().yaw):return
   desired=grid.chart_to_world(q);desired.z=hull(subject).get_world_location().z;anchor=s['threat'].get_component_by_class(unreal.CesiumGlobeAnchorComponent);geo=anchor.resolve_georeference();anchor.move_to_longitude_latitude_height(geo.transform_unreal_position_to_longitude_latitude_height(desired));anchor.sync()
   distance=None
   for d in [300]:
    if n.begin_backward_retreat(s['threat'],d):distance=d;n.return_to_patrol();break
   s['reverse_diagnostics']=json.loads(n.get_navigation_diagnostics())
   if not distance:return
   c.set_combat_role(unreal.ShipEnemyRole.FIGHTER);c.enable_hit_and_run=True;c.prefer_backward_retreat=True;c.backward_retreat_distance_m=distance;c.set_hostile_target(s['threat']);c.enable_combat=True
   subject.can_be_damaged=True;assert wp.receive_ship_damage(70,s['threat']);subject.can_be_damaged=False
   s.update(phase='reverse',at=t,reverse_distance_m=distance)
  if not s['rows'] or t-s['rows'][-1]['t']>.1:
   s['rows'].append(dict(t=t,phase=s['phase'],en=list(grid.world_to_chart(hull(subject).get_world_location()).to_tuple()),speed=m.signed_forward_speed(),shots=wp.shots_fired,state=str(c.tactical_state),reverse=n.backward_retreat_active,retreats=c.backward_retreats_started,resumed=c.patrols_resumed,physics=hull(subject).is_simulating_physics(),controller=subject.get_controller().get_name()))
  if s['phase']=='reverse' and t-s['at']>=35:
   rows=s['rows'];back=[r for r in rows if r['speed']<-1]
   s['checks']=dict(low_health_reverse=c.backward_retreats_started>0,physical_backward_motion=bool(back) and min(r['speed'] for r in back)<-2,real_shot_while_reversing=any(b['shots']>a['shots'] and b['speed']<-1 for a,b in zip(rows,rows[1:])))
   assert all(s['checks'].values()),s['checks'];s['threat'].get_component_by_class(unreal.ShipCombatComponent).faction=2;s.update(phase='return',at=t,return_start=rows[-1]['en'])
  if s['phase']=='return' and t-s['at']>=20:
   s['checks'].update(returned_to_patrol=c.patrols_resumed>0 and not n.has_tactical_destination(),original_targets=[n.patrol_actor_a.get_path_name(),n.patrol_actor_b.get_path_name()]==s['anchors'],physical_resume=math.dist(s['rows'][-1]['en'],s['return_start'])>20,controller_and_physics=all(r['physics'] and r['controller']==s['controller'] for r in s['rows']))
   assert all(s['checks'].values()),s['checks'];finish('PASSED');return
  if int(t)%5==0:save()
 except Exception:s['error']=traceback.format_exc();finish('FAILED')
s['handle']=unreal.register_slate_post_tick_callback(tick);save()
