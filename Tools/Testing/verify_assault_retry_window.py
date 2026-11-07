"""Inject an exhausted strategic retry budget, then observe native cooldown/motion.
Only the retry fault and other ships' stationary fixture state are injected.
No subject pose, drive command, destination or damage is written after startup.
"""
import unreal,builtins,json,time,traceback,math
from pathlib import Path
root=Path(unreal.Paths.project_dir());p=root/'Tools/Testing/verify_final_demo.py'
ctx=dict(KIND='patrol',FULL_AI=True,TACTICS=True,ASSAULT=True,OBSERVE_SECONDS=99999,OUTPUT='ASSAULT_RETRY_STARTUP.json',CAPTURE=False)
exec(compile(p.read_text(encoding='utf-8'),str(p),'exec'),ctx);base=ctx['st']
s=dict(status='RUNNING',phase='startup',rows=[],checks={},error=None,native_dll_sha256=base['native_dll_sha256']);builtins.assault_retry_probe=s
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
def sub(w,c):return lib.call_method('GetWorldSubsystem',args=(w,c.static_class()))
def save():(root/'Saved/ASSAULT_RETRY_WINDOW.json').write_text(json.dumps({k:v for k,v in s.items() if k not in ['handle','subject']},indent=2),encoding='utf-8')
def finish(status):
 s['status']=status;save();unreal.unregister_slate_post_tick_callback(s['handle'])
 if base['status']=='RUNNING':unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_RETRY_FIXTURE'
 if status=='PASSED' or not globals().get('HOLD_FAILURE',False):unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
 try:
  w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  if not w:return
  t=unreal.GameplayStatics.get_time_seconds(w);field=sub(w,unreal.NavalBattlefieldSubsystem);grid=sub(w,unreal.NavalSectorSubsystem)
  if s['phase']=='startup':
   assert base['status']=='RUNNING',base.get('error')
   if base['phase']!='observe':return
   fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
   subject=next(a for a in fleet if a.get_component_by_class(unreal.ShipMovementComponent).hull_class==unreal.ShipHullClass.LARGE)
   for a in fleet:
    a.can_be_damaged=False;a.get_component_by_class(unreal.ShipCombatComponent).enable_combat=False
    if a!=subject:
     a.get_component_by_class(unreal.ShipEnemyNavigationComponent).stop_navigation();a.set_actor_tick_enabled(False)
   n=subject.get_component_by_class(unreal.ShipEnemyNavigationComponent);n.stop_navigation()
   # BlueprintReadOnly status is changed only by this explicit editor fault.
   assignments=list(field.get_editor_property('assignments'))
   a=next(a for a in assignments if a.ship==subject)
   a.set_editor_property('mission_route_retries',3);a.set_editor_property('assault_released',True)
   field.set_editor_property('assignments',assignments)
   assert next(a for a in field.assignments if a.ship==subject).mission_route_retries==3
   unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_RETRY_FIXTURE'
   s.update(phase='observe',subject=subject,start=list(grid.world_to_chart(subject.get_actor_location()).to_tuple()),at=t,prior_goals=a.assault_goals_reached,controller=subject.get_controller().get_name(),cooldown=field.lost_contact_search_delay_seconds);save();return
  if s['rows'] and t-s['rows'][-1]['t']<.1:return
  subject=s['subject'];a=next(a for a in field.assignments if a.ship==subject);n=subject.get_component_by_class(unreal.ShipEnemyNavigationComponent);h=next(c for c in subject.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
  row=dict(t=t,goals=a.assault_goals_reached,retries=a.mission_route_retries,mission=a.mission_state,throttle=n.desired_throttle,physics=h.is_simulating_physics(),water=subject.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body(),en=list(grid.world_to_chart(h.get_world_location()).to_tuple()))
  s['rows'].append(row)
  if t<s['cooldown']-.5:assert a.assault_goals_reached==s['prior_goals'] and abs(n.desired_throttle)<.01,'Retry bypassed the cooldown'
  if a.assault_goals_reached>s['prior_goals'] and 'resumed_at' not in s:s['resumed_at']=t;save()
  if 'resumed_at' in s and math.dist(row['en'],s['start'])>50:
   s['checks']=dict(cooldown_observed=s['resumed_at']>=s['cooldown']-.1,fresh_goal=a.assault_goals_reached>s['prior_goals'],budget_reopened=a.mission_route_retries<3,physical_resume=True,controller=subject.get_controller().get_name()==s['controller'],physics_and_water=all(r['physics'] and r['water'] for r in s['rows']))
   assert all(s['checks'].values()),s['checks'];s['duration']=t-s['at'];finish('PASSED');return
  assert t-s['at']<180,'Exhausted escort failed to resume native movement'
  if int(t)%5==0:save()
 except Exception:s['error']=traceback.format_exc();finish('FAILED')
s['handle']=unreal.register_slate_post_tick_callback(tick);save()
