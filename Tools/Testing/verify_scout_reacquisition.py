"""Controlled survival/search regression, not a natural 3v3 match.
Four guarded damage transitions isolate a Small scout and Blue flagship.
The two live hulls remain physical; damage is withheld until real reacquisition.
"""
import unreal,builtins,json,time,traceback
from pathlib import Path
r=Path(unreal.Paths.project_dir());p=r/'Tools/Testing/verify_final_demo.py'
ctx=dict(KIND='lethal',OUTPUT='SCOUT_REACQUISITION_CONTROLLED.json',CAPTURE=False,LIMIT_SECONDS=1200)
exec(compile(p.read_text(encoding='utf-8'),str(p),'exec'),ctx);base=ctx['st']
s=dict(status='RUNNING',phase='wait',forced_deaths=[],samples=[],first_seen=None,lost=None,search_started=None,reacquired=None,error=None,last=-1);builtins.scout_search_probe=s
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
def sub(w,c):return lib.call_method('GetWorldSubsystem',args=(w,c.static_class()))
def save():
 (r/'Saved/SCOUT_SEARCH_REGRESSION.json').write_text(json.dumps({k:v for k,v in s.items() if k not in ['handle','blue','red']},indent=2),encoding='utf-8')
def tick(dt):
 try:
  if base['status']!='RUNNING':
   s['status']='PASSED' if base['status']=='PASSED' and s['search_started'] is not None and s['reacquired'] is not None else 'FAILED';s['base_status']=base['status'];save();unreal.unregister_slate_post_tick_callback(s['handle']);return
  w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  if not w or base['phase']!='observe':return
  t=unreal.GameplayStatics.get_time_seconds(w);age=t-base['ready_at'];f=sub(w,unreal.NavalBattlefieldSubsystem);b=sub(w,unreal.NavalBattleSubsystem)
  fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
  if s['phase']=='wait':
   s['blue']=next(a for a in fleet if a.get_name()=='Demo_Blue_Flagship')
   s['red']=next(a for a in fleet if a.get_component_by_class(unreal.ShipWeaponComponent).team()==1 and a.get_component_by_class(unreal.ShipMovementComponent).hull_class==unreal.ShipHullClass.SMALL)
   for a in fleet:
    if a not in [s['blue'],s['red']]:
     weap=a.get_component_by_class(unreal.ShipWeaponComponent);src=s['red'] if weap.team()==0 else s['blue'];assert weap.receive_ship_damage(1000,src)
     before=(b.blue_alive,b.red_alive);assert not weap.receive_ship_damage(1000,src);assert before==(b.blue_alive,b.red_alive)
     s['forced_deaths'].append(dict(actor=a.get_name(),team=weap.team(),reason=str(weap.death_reason)))
   assert b.blue_alive==1 and b.red_alive==1,'Counters did not decrement once per death'
   s['phase']='seeking';save()
  blue=s['blue'];red=s['red'];c=red.get_component_by_class(unreal.ShipCombatComponent)
  a=next(a for a in f.assignments if a.ship==red)
  if s['reacquired'] is None:
   blue.can_be_damaged=False;red.can_be_damaged=False
  if age-s['last']<.5:return
  s['last']=age;visible=bool(c.target==blue and c.target_visible)
  if visible and s['first_seen'] is None:s['first_seen']=age
  if not visible and s['first_seen'] is not None and s['lost'] is None:s['lost']=age
  if a.searching_circuit and s['search_started'] is None:s['search_started']=age
  if visible and c.has_firing_solution and s['search_started'] is not None and age>s['search_started']+1 and s['reacquired'] is None:s['reacquired']=age
  s['samples'].append(dict(age=age,visible=visible,range=c.distance_to_target_m,search=a.searching_circuit,search_goals=a.search_goals_reached,mission=a.mission_state,nav=str(red.get_component_by_class(unreal.ShipEnemyNavigationComponent).navigation_state)))
  if int(age)%5==0:save()
 except Exception:
  s['error']=traceback.format_exc();s['status']='FAILED';save();unreal.unregister_slate_post_tick_callback(s['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
s['handle']=unreal.register_slate_post_tick_callback(tick);save()
