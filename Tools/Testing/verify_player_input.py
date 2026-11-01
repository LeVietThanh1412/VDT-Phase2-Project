"""Current menu plus real Enhanced Input, with damage/combat disabled by fixture."""
import unreal,builtins,json,time,traceback,math
from pathlib import Path
root=Path(unreal.Paths.project_dir());p=root/'Tools/Testing/verify_final_demo.py'
ctx=dict(KIND='patrol',FULL_AI=False,TACTICS=False,ASSAULT=False,OBSERVE_SECONDS=99999,OUTPUT='PLAYER_INPUT_STARTUP.json',CAPTURE=False)
if 'HULL_CLASSES' in globals():ctx['HULL_CLASSES']=HULL_CLASSES
ctx['PLAYER_TEAM']=int(globals().get('PLAYER_TEAM',0))
output=globals().get('OUTPUT','PLAYER_INPUT_REGRESSION.json')
exec(compile(p.read_text(encoding='utf-8'),str(p),'exec'),ctx);base=ctx['st']
s=dict(status='RUNNING',rows=[],checks={},error=None);builtins.player_input_probe=s
actions={n:unreal.load_asset('/Game/Ship/Inputs/'+n) for n in ['IA_MoveForwardBackward','IA_MoveLeftRight','IA_LookAround','IA_Aim','IA_Fire','IA_Zoom']}
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
def save():(root/'Saved'/output).write_text(json.dumps({k:v for k,v in s.items() if k!='handle'},indent=2))
def finish(status):
 s['status']=status;save();unreal.unregister_slate_post_tick_callback(s['handle'])
 if base['status']=='RUNNING':unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_INPUT'
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
 try:
  if base['status']!='RUNNING':raise AssertionError(base.get('error') or base['status'])
  w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  if not w or base['phase']!='observe':return
  age=unreal.GameplayStatics.get_time_seconds(w)-base['ready_at'];pc=unreal.GameplayStatics.get_player_controller(w,0);ship=unreal.GameplayStatics.get_player_pawn(w,0)
  inputs=lib.call_method('GetLocalPlayerSubSystemFromPlayerController',args=(pc,unreal.EnhancedInputLocalPlayerSubsystem.static_class()))
  def inject(n,v):inputs.inject_input_vector_for_action(actions[n],unreal.Vector(*v),[],[])
  if 1<age<6:inject('IA_MoveForwardBackward',(1,0,0))
  if 6<age<14:inject('IA_MoveForwardBackward',(.65,0,0));inject('IA_MoveLeftRight',(1,0,0))
  if 14<age<16:inject('IA_MoveForwardBackward',(.65,0,0))
  if 16<age<24:inject('IA_MoveForwardBackward',(.65,0,0));inject('IA_MoveLeftRight',(-1,0,0))
  if 24<age<25:inject('IA_LookAround',(.4,.2,0))
  if 26<age<31.5:inject('IA_Aim',(1,0,0))
  if 28<age<29:inject('IA_Fire',(1,0,0))
  if 30<age<30.2:inject('IA_Zoom',(1,0,0))
  if not s['rows'] or age-s['rows'][-1]['age']>.1:
   h=next(c for c in ship.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM');pos=h.get_world_location();look=pc.get_control_rotation();wp=ship.get_component_by_class(unreal.ShipWeaponComponent)
   gun=next(c for c in ship.get_components_by_class(unreal.CameraComponent) if c.get_name()=='GunCamera')
   m=ship.get_component_by_class(unreal.ShipMovementComponent)
   s['rows'].append(dict(age=age,pos=list(pos.to_tuple()),yaw=h.get_world_rotation().yaw,roll=h.get_world_rotation().roll,pitch=h.get_world_rotation().pitch,rudder_force=list(m.last_rudder_force.to_tuple()),inertia=list(h.get_inertia_tensor().to_tuple()),look=[look.pitch,look.yaw],shots=wp.shots_fired,gun=gun.is_active(),fov=gun.field_of_view,physics=h.is_simulating_physics(),player=ship.is_player_controlled()))
  if age>=34:
   rows=s['rows'];first=rows[0];last=rows[-1]
   m=ship.get_component_by_class(unreal.ShipMovementComponent)
   def yaw_at(t):return min(rows,key=lambda r:abs(r['age']-t))['yaw']
   delta=lambda a,b:(yaw_at(b)-yaw_at(a)+180)%360-180
   right=delta(6,14);left=delta(17,24)
   s.update(right_yaw_degrees=right,left_yaw_degrees=left,hull_class=str(m.hull_class),profile_inertia_scale=list(m.profile.inertia_scale.to_tuple()))
   s['checks']=dict(forward_physical_travel=math.dist(first['pos'],last['pos'])>3000,right_rudder_yaw=abs(right)>5,left_rudder_yaw=abs(left)>5,opposite_turn_directions=right*left<0,mouse_look=any(math.dist(r['look'],first['look'])>5 for r in rows),original_projectile_fired=last['shots']>0,aim_camera=any(r['gun'] for r in rows if 27<r['age']<31),released_aim_camera=not last['gun'],physics_and_possession=all(r['physics'] and r['player'] for r in rows))
   assert all(s['checks'].values()),s['checks'];finish('PASSED')
 except Exception:s['error']=traceback.format_exc();finish('FAILED')
s['handle']=unreal.register_slate_post_tick_callback(tick);save()
