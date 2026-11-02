"""PIE-only player probes at the reported shore; real input/physics, no saved edits.

Only initial probe transforms are assigned. After settling all motion uses the
original Enhanced Input/Blueprint actuators; no pose/velocity correction occurs.
"""
import unreal,builtins,json,time,math,traceback,hashlib
from pathlib import Path
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
spec=json.loads((root/'Tools/Testing/Fixtures/shoreline_probe.json').read_text())
ctx=dict(OUTPUT='SHORELINE_STARTUP.json',REQUIRED_LEGS=999999,WALL_LIMIT_SECONDS=1200)
p=root/'Tools/Testing/verify_authored_patrol.py';exec(compile(p.read_text(),str(p),'exec'),ctx);base=builtins.authored_patrol_probe
s=dict(status='RUNNING',phase='startup',started=time.monotonic(),rows=[],results=[],error=None,native_dll_sha256=base['native_dll_sha256']);builtins.shoreline_probe=s
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
sub=lambda w,c:lib.call_method('GetWorldSubsystem',args=(w,c.static_class()))
types=[unreal.ShipHullClass.MEDIUM,unreal.ShipHullClass.SMALL]
action=unreal.load_asset('/Game/Ship/Inputs/IA_MoveForwardBackward')
output=globals().get('OUTPUT','USER_SHORELINE_PHYSICS.json')
def save():
 d={k:v for k,v in s.items() if k not in ['handle','probe','camera','inputs']};p=root/'Saved'/output;t=p.with_suffix('.json.tmp');t.write_text(json.dumps(d,indent=2));t.replace(p)
def finish(status):
 s['status']=status;save();unreal.unregister_slate_post_tick_callback(s['handle'])
 if base['status']=='RUNNING':unreal.unregister_slate_post_tick_callback(base['handle'])
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def spawn(w,cls,pos,rot):
 g=unreal.get_default_object(unreal.GameplayStatics.static_class());t=unreal.Transform(location=pos,rotation=rot)
 a=g.call_method('BeginDeferredActorSpawnFromClass',args=(w,cls,t,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
 g.call_method('FinishSpawningActor',args=(a,t,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT));return a
def create_probe(w,pc,grid,t):
 cls=types[len(s['results'])];small=cls==unreal.ShipHullClass.SMALL
 asset='/Game/ShipSmall/BluePrint/BP_ShipSmall.BP_ShipSmall_C' if small else '/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'
 start=grid.chart_to_world(unreal.Vector2D(*spec['start_en']));target=grid.chart_to_world(unreal.Vector2D(*spec['coast_en']));start.z=900
 yaw=math.degrees(math.atan2(target.y-start.y,target.x-start.x));probe=spawn(w,unreal.load_class(None,asset),start,unreal.Rotator(yaw=yaw))
 m=probe.get_component_by_class(unreal.ShipMovementComponent);assert m.apply_ship_class(cls)
 arrow=next(c for c in probe.get_components_by_class(unreal.ArrowComponent) if c.get_name()=='Arrow');rot=probe.get_actor_rotation();rot.yaw+=(yaw-arrow.get_world_rotation().yaw+180)%360-180;probe.set_actor_rotation(rot,True)
 probe.get_component_by_class(unreal.CesiumGlobeAnchorComponent).sync();probe.can_be_damaged=False
 pc.possess(probe)
 for _ in range(10):
  if pc.get_selected_ship()==probe:break
  pc.call_method('CycleShipCamera')
 pc.set_control_rotation(unreal.Rotator(pitch=-15,yaw=yaw));pc.set_view_target_with_blend(probe,0)
 s.update(probe=probe,phase='settle',at=t,cls=str(cls),forward=[math.cos(math.radians(yaw)),math.sin(math.radians(yaw))])
 s['inputs']=lib.call_method('GetLocalPlayerSubSystemFromPlayerController',args=(pc,unreal.EnhancedInputLocalPlayerSubsystem.static_class()))
 save()
def tick(dt):
 try:
  assert time.monotonic()-s['started']<1200,'Shoreline fixture timeout'
  assert base['status']!='FAILED',base.get('error')
  w=ed.get_game_world()
  if not w:return
  t=unreal.GameplayStatics.get_time_seconds(w);pc=unreal.GameplayStatics.get_player_controller(w,0);grid=sub(w,unreal.NavalSectorSubsystem)
  if s['phase']=='startup':
   if base['phase']!='observe':return
   unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_SHORELINE'
   for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn):
    if a.get_component_by_class(unreal.ShipWeaponComponent):
     a.can_be_damaged=False
     if n:=a.get_component_by_class(unreal.ShipEnemyNavigationComponent):n.stop_navigation()
     if c:=a.get_component_by_class(unreal.ShipCombatComponent):c.enable_combat=False
   focus=grid.chart_to_world(unreal.Vector2D(*spec['screenshot_en']));eye=focus+unreal.Vector(0,0,230000)
   s['camera']=spawn(w,unreal.CameraActor.static_class(),eye,unreal.MathLibrary.find_look_at_rotation(eye,focus));pc.set_view_target_with_blend(s['camera'],0);s.update(phase='stream',at=t);return
  if s['phase']=='stream':
   assert t-s['at']<40,'Reported coast did not stream'
   if t-s['at']<8:return
   if any(a.get_editor_property('load_progress')<99.9 for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Cesium3DTileset)):return
   create_probe(w,pc,grid,t);return
  if s['phase']=='next':create_probe(w,pc,grid,t);return
  probe=s['probe'];m=probe.get_component_by_class(unreal.ShipMovementComponent);h=next(c for c in probe.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM');age=t-s['at']
  if s['phase']=='settle':
   if age<20:return
   s.update(phase='drive',at=t);age=0
  if s['phase']=='drive':s['inputs'].inject_input_vector_for_action(action,unreal.Vector(1,0,0),[],[])
  elif s['phase']=='reverse':s['inputs'].inject_input_vector_for_action(action,unreal.Vector(-1,0,0),[],[])
  if t-s.get('last',-1)>.1:
   s['last']=t;pos=h.get_world_location();rot=h.get_world_rotation();buoy=probe.get_component_by_class(unreal.BuoyancyComponent)
   s['rows'].append(dict(cls=s['cls'],phase=s['phase'],age=age,t=t,pos=list(pos.to_tuple()),pitch=rot.pitch,roll=rot.roll,speed=m.signed_forward_speed(),blocked=m.shoreline_blocked,water=buoy.is_in_water_body(),physics=h.is_simulating_physics(),dead=probe.get_component_by_class(unreal.ShipWeaponComponent).is_destroyed))
  if s['phase']=='drive' and age>=45:
   rows=[r for r in s['rows'] if r['cls']==s['cls'] and r['phase']=='drive'];assert any(r['blocked'] for r in rows),'Shoreline thrust was never stopped'
   assert max(abs(r['speed']) for r in rows if r['age']>40)<1.5,'Held thrust did not settle before the coast'
   s['reverse_start']=list(h.get_world_location().to_tuple());s.update(phase='reverse',at=t)
   unreal.ShipCombatLibrary.capture_gameplay_hud(w,str(root/'Saved'/('SHORELINE_'+s['cls'].split(':')[0].split('.')[-1]+'.png')))
  elif s['phase']=='reverse' and age>=15:
   rows=[r for r in s['rows'] if r['cls']==s['cls']];reverse=[r for r in rows if r['phase']=='reverse'];now=list(h.get_world_location().to_tuple());delta=[now[i]-s['reverse_start'][i] for i in range(2)];away=-sum(a*b for a,b in zip(delta,s['forward']))/100
   result=dict(cls=s['cls'],max_pitch=max(abs(r['pitch']) for r in rows),max_roll=max(abs(r['roll']) for r in rows),water_fraction=sum(r['water'] for r in rows)/len(rows),reverse_travel_m=away,min_reverse_speed=min(r['speed'] for r in reverse),physical=all(r['physics'] and not r['dead'] for r in rows))
   assert result['max_pitch']<3 and result['max_roll']<3 and result['water_fraction']>.98 and result['physical'],result
   assert away>10 and result['min_reverse_speed']<-.5,result
   s['results'].append(result);probe.destroy_actor();s.pop('probe');s.pop('inputs');save()
   if len(s['results'])==len(types):finish('PASSED')
   else:s['phase']='next'
  if int(t)%5==0 and s.get('saved')!=int(t):s['saved']=int(t);save()
 except Exception:s['error']=traceback.format_exc();finish('FAILED')
s['handle']=unreal.register_slate_post_tick_callback(tick);save()
