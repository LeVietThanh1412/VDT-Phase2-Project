"""Physical closed-circuit followed by legacy A/B regression. No pose overrides.
The explicit second phase clears only Blue circuit authoring; propulsion remains native.
"""
import unreal,builtins,json,time,math,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());p=root/'Tools/Testing/verify_final_demo.py'
ctx=dict(KIND='patrol',ASSAULT=False,OBSERVE_SECONDS=99999,LIMIT_SECONDS=3000,WALL_LIMIT_SECONDS=4000,OUTPUT='NAVIGATION_SOAK_RAW.json',CAPTURE=False)
exec(compile(p.read_text(encoding='utf-8'),str(p),'exec'),ctx);base=ctx['st']
s=dict(status='RUNNING',phase='circuit',ships={},events=[],error=None,last=-1);builtins.navigation_soak_probe=s
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
def save():
 (root/'Saved/NAVIGATION_SOAK.json').write_text(json.dumps({k:v for k,v in s.items() if k!='handle'},indent=2),encoding='utf-8')
def finish(status):
 s['status']=status;save();unreal.unregister_slate_post_tick_callback(s['handle'])
 if base['status']=='RUNNING':unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_SOAK'
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
 try:
  if base['status']!='RUNNING':raise AssertionError(base['status']+': '+str(base.get('error')))
  w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  if not w or base['phase']!='observe':return
  t=unreal.GameplayStatics.get_time_seconds(w);s['age']=t-base['ready_at']
  if t-s['last']<.1:return
  s['last']=t;grid=lib.call_method('GetWorldSubsystem',args=(w,unreal.NavalSectorSubsystem.static_class()))
  fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipEnemyNavigationComponent)]
  for a in fleet:
   n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent);h=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
   pos=list(grid.world_to_chart(h.get_world_location()).to_tuple());key=a.get_name();team=a.get_component_by_class(unreal.ShipWeaponComponent).team()
   r=s['ships'].setdefault(key,dict(label=a.get_actor_label(),team=team,controller=a.get_controller().get_name(),distance_m=0,last_pos=pos,legs=0,circuit_legs=0,circuits=0,arrivals=[],failures=[],physical=True,controller_preserved=True,ab_start=None,last_goal=None))
   r['distance_m']+=math.dist(pos,r['last_pos']);r['last_pos']=pos;r['physical'] &= h.is_simulating_physics();r['controller_preserved'] &= bool(a.get_controller() and a.get_controller().get_name()==r['controller'])
   assert r['physical'] and r['controller_preserved'],r['label']
   if n.last_navigation_failure_reason and n.last_navigation_failure_reason not in r['failures']:r['failures'].append(n.last_navigation_failure_reason)
   total=n.patrol_legs_completed+n.circuit_legs_completed
   if total>r.get('total',0) and r['last_goal']:
    goal=r['last_goal'];error=math.dist(pos,goal['en']);r['arrivals'].append(dict(age=s['age'],phase=s['phase'],target=goal['name'],distance_m=error,speed_mps=h.get_physics_linear_velocity().length()/100))
    assert error<=n.arrival_radius_m+3,('Arrival outside radius',r['label'],error)
   r.update(total=total,legs=n.patrol_legs_completed,circuit_legs=n.circuit_legs_completed,circuits=n.patrol_circuits_completed)
   diag=json.loads(n.get_navigation_diagnostics());r['diagnostics']=diag
   target=next((x for x in [n.patrol_actor_a,n.patrol_actor_b,*n.patrol_circuit_actors] if x and x.get_name()==diag['active_target']),None)
   if target:r['last_goal']=dict(name=target.get_name(),en=list(grid.world_to_chart(target.get_actor_location()).to_tuple()))
  blue=[r for r in s['ships'].values() if r['team']==0]
  if s['phase']=='circuit' and len(blue)==3 and all(r['circuits']>=1 for r in blue):
   s['circuit_completed_at']=s['age'];s['phase']='ab'
   for a in fleet:
    r=s['ships'][a.get_name()];n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
    r['ab_start']=n.patrol_legs_completed;r['last_goal']=None
    if r['team']==0:n.patrol_circuit_actors=[];n.start_patrol()
   s['events'].append(dict(age=s['age'],action='Clear Blue optional circuits and restart original A/B; no hull/target movement'))
  if s['phase']=='ab' and all(r['legs']-r['ab_start']>=4 for r in s['ships'].values()):
   assert all(r['diagnostics']['navigation_state']!='EShipNavigationState::Failed' for r in s['ships'].values()),'Ship remains failed'
   s['duration']=s['age'];finish('PASSED');return
  if int(t)%5==0:save()
 except Exception:s['error']=traceback.format_exc();finish('FAILED')
s['handle']=unreal.register_slate_post_tick_callback(tick);save()
