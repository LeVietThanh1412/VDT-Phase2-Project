"""Controlled physical hull/water measurements through original player actuators.

Only the transient probe's initial pose is assigned. Existing fleet decisions
are stopped; their physics stays active. TUNING changes real profile parameters
in memory, restores them on exit, and never saves an asset. No angle/velocity
assignment, rocking animation, collision disable or artificial wave is used.
"""
import unreal,builtins,json,time,traceback,math,statistics,hashlib
from pathlib import Path
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
name=globals().get('OUTPUT','WAVE_RESPONSE.json');tuning=globals().get('TUNING',{})
classes=globals().get('CLASSES',['Small','Medium','Large']);durations=globals().get('DURATIONS',dict(idle=45,cruise=20,turn=20,reverse=20,reverse_fire=20))
p=root/'Tools/Testing/verify_authored_patrol.py'
exec(compile(p.read_text(),str(p),'exec'),dict(FULL_AI=False,HULL_CLASSES=[2]*6,REQUIRED_LEGS=99999,OUTPUT='WAVE_STARTUP.json'))
base=builtins.authored_patrol_probe
s=dict(status='RUNNING',phase='startup',started=time.monotonic(),rows=[],results=[],error=None,tuning=tuning,native_dll_sha256=base['native_dll_sha256']);builtins.wave_response_probe=s
originals={};refs={};lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
sub=lambda w,c:lib.call_method('GetWorldSubsystem',args=(w,c.static_class()))
actions={n:unreal.load_asset('/Game/Ship/Inputs/'+n) for n in ['IA_MoveForwardBackward','IA_MoveLeftRight']}
def save():
 p=root/'Saved'/name;t=p.with_suffix('.tmp');t.write_text(json.dumps(s,indent=2,default=str));t.replace(p)
def finish(status):
 for cls,values in originals.items():
  for key,value in values.items():refs['profile_'+cls].set_editor_property(key,value)
 s['status']=status;save();unreal.unregister_slate_post_tick_callback(refs['handle'])
 if base['status']=='RUNNING':unreal.unregister_slate_post_tick_callback(base['handle'])
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def spawn(w,cls,pos,rot):
 g=unreal.get_default_object(unreal.GameplayStatics.static_class());t=unreal.Transform(location=pos,rotation=rot)
 a=g.call_method('BeginDeferredActorSpawnFromClass',args=(w,cls,t,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
 g.call_method('FinishSpawningActor',args=(a,t,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT));return a
def begin_subject(w,pc,t):
 cls=classes[len(s['results'])];kind=getattr(unreal.ShipHullClass,cls.upper())
 profile=unreal.load_asset('/Game/NavalBattle/Profiles/DA_'+cls+'Ship');refs['profile_'+cls]=profile
 overrides=tuning.get(cls,{})
 originals[cls]={k:profile.get_editor_property(k) for k in overrides}
 for k,v in overrides.items():profile.set_editor_property(k,unreal.Vector(*v) if k=='inertia_scale' else v)
 grid=sub(w,unreal.NavalSectorSubsystem);start=grid.chart_to_world(unreal.Vector2D(12700,-2800));goal=grid.chart_to_world(unreal.Vector2D(12700,-4000));start.z=900
 yaw=math.degrees(math.atan2(goal.y-start.y,goal.x-start.x))
 path='/Game/ShipSmall/BluePrint/BP_ShipSmall.BP_ShipSmall_C' if cls=='Small' else '/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'
 probe=spawn(w,unreal.load_class(None,path),start,unreal.Rotator(yaw=yaw));m=probe.get_component_by_class(unreal.ShipMovementComponent);assert m.apply_ship_class(kind)
 arrow=next(c for c in probe.get_components_by_class(unreal.ArrowComponent) if c.get_name()=='Arrow');rot=probe.get_actor_rotation();rot.yaw+=(yaw-arrow.get_world_rotation().yaw+180)%360-180;probe.set_actor_rotation(rot,True);probe.get_component_by_class(unreal.CesiumGlobeAnchorComponent).sync()
 probe.can_be_damaged=False;pc.possess(probe);pc.set_control_rotation(unreal.Rotator(pitch=-10,yaw=yaw));pc.set_view_target_with_blend(probe,0)
 refs.update(probe=probe,inputs=lib.call_method('GetLocalPlayerSubSystemFromPlayerController',args=(pc,unreal.EnhancedInputLocalPlayerSubsystem.static_class())))
 s.update(cls=cls,phase='settle',at=t,next_fire=0)
 s.setdefault('profiles',{})[cls]={k:str(profile.get_editor_property(k)) for k in ['mass_kg','inertia_scale','linear_damping','angular_damping','heave_damping_per_second','roll_damping_per_second','pitch_damping_per_second','lateral_damping_per_second','forward_max_speed','reverse_max_speed','propulsion_scale','rudder_scale']}
 save()
def pct(v,q):
 v=sorted(v);return v[min(len(v)-1,int((len(v)-1)*q))]
def summarize(cls):
 result=dict(cls=cls,states={})
 for phase in durations:
  rows=[r for r in s['rows'] if r['cls']==cls and r['phase']==phase];assert len(rows)>25,(cls,phase)
  mean=lambda key:statistics.mean(r[key] for r in rows)
  roll=mean('roll');pitch=mean('pitch');r=[x['roll']-roll for x in rows];p=[x['pitch']-pitch for x in rows]
  result['states'][phase]=dict(samples=len(rows),mean_roll=roll,mean_pitch=pitch,roll_rms=math.sqrt(statistics.mean(x*x for x in r)),pitch_rms=math.sqrt(statistics.mean(x*x for x in p)),roll_p95=pct([abs(x) for x in r],.95),pitch_p95=pct([abs(x) for x in p],.95),roll_max=max(abs(x['roll']) for x in rows),pitch_max=max(abs(x['pitch']) for x in rows),water_fraction=mean('water'),mean_forward_speed=mean('speed'),p99_vertical_speed=pct([abs(x['vz']) for x in rows],.99),heave_range=max(x['heave'] for x in rows)-min(x['heave'] for x in rows),wave_height_range=max(x['surface_z'] for x in rows)-min(x['surface_z'] for x in rows),surface_roll_p95=pct([abs(x['surface_roll']) for x in rows],.95),surface_pitch_p95=pct([abs(x['surface_pitch']) for x in rows],.95),yaw_change=(rows[-1]['yaw']-rows[0]['yaw']+180)%360-180,physics=all(x['physics'] and not x['dead'] for x in rows),shots=rows[-1]['shots'])
  v=result['states'][phase]
  for axis in ['roll','pitch']:
   v[axis+'_motion_rms']=v[axis+'_rms'];v[axis+'_motion_p95']=v[axis+'_p95']
   v[axis+'_rms']=math.sqrt(mean(axis)**2+v[axis+'_motion_rms']**2)
   v[axis+'_p95']=pct([abs(x[axis]) for x in rows],.95)
 assert all(v['physics'] and v['water_fraction']>.98 and v['roll_max']<15 and v['pitch_max']<15 for v in result['states'].values()),result
 if 'cruise' in durations:assert result['states']['cruise']['mean_forward_speed']>3,result
 if 'turn' in durations:assert abs(result['states']['turn']['yaw_change'])>5,result
 if 'reverse' in durations:assert result['states']['reverse']['mean_forward_speed']<-.5,result
 if 'reverse_fire' in durations:assert result['states']['reverse_fire']['shots']>=5,result
 return result
def tick(dt):
 try:
  assert time.monotonic()-s['started']<1600,'Wave fixture wall timeout'
  assert base['status']!='FAILED',base.get('error')
  w=ed.get_game_world()
  if not w:return
  t=unreal.GameplayStatics.get_time_seconds(w);pc=unreal.GameplayStatics.get_player_controller(w,0)
  if s['phase']=='startup':
   if base['phase']!='observe':return
   unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_WAVE_FIXTURE'
   for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn):
    if a.get_component_by_class(unreal.ShipWeaponComponent):
     a.can_be_damaged=False
     if n:=a.get_component_by_class(unreal.ShipEnemyNavigationComponent):n.stop_navigation()
     if c:=a.get_component_by_class(unreal.ShipCombatComponent):c.enable_combat=False
   begin_subject(w,pc,t);return
  if s['phase']=='next':begin_subject(w,pc,t);return
  probe=refs['probe'];m=probe.get_component_by_class(unreal.ShipMovementComponent);h=next(c for c in probe.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM');age=t-s['at']
  if s['phase']=='settle':
   if age<20:return
   s.update(phase=next(iter(durations)),at=t);age=0
  phase=s['phase'];forward=1 if phase=='cruise' else .4 if phase=='turn' else -.7 if phase=='reverse' else -.25 if phase=='reverse_fire' else 0
  refs['inputs'].inject_input_vector_for_action(actions['IA_MoveForwardBackward'],unreal.Vector(forward,0,0),[],[])
  refs['inputs'].inject_input_vector_for_action(actions['IA_MoveLeftRight'],unreal.Vector(1 if phase=='turn' else 0,0,0),[],[])
  wp=probe.get_component_by_class(unreal.ShipWeaponComponent)
  if phase=='reverse_fire' and age>=s['next_fire']:
   wp.try_fire_weapon(unreal.Vector(),None,unreal.Vector(),False);s['next_fire']=age+2
  if t-s.get('last',-1)>.05:
   s['last']=t;pos=h.get_world_location();rot=h.get_world_rotation();v=h.get_physics_linear_velocity();omega=h.get_physics_angular_velocity_in_degrees()
   sampled=unreal.ShipCombatLibrary.sample_ocean_waves(w,pos);assert sampled,'Physical ocean waves unavailable'
   surface,normal=sampled
   s['rows'].append(dict(t=t,age=age,cls=s['cls'],phase=phase,roll=rot.roll,pitch=rot.pitch,yaw=rot.yaw,angular_velocity=list(omega.to_tuple()),position=list(pos.to_tuple()),vz=v.z/100,heave=(pos.z-surface.z)/100,surface_z=surface.z/100,surface_roll=math.degrees(math.atan2(normal.y,normal.z)),surface_pitch=math.degrees(math.atan2(normal.x,normal.z)),speed=m.signed_forward_speed(),physics=h.is_simulating_physics(),water=probe.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body(),dead=wp.is_destroyed,shots=wp.shots_fired,blocked=m.shoreline_blocked))
  if age>min(10,durations[phase]*.5) and phase not in s.setdefault('captures',{}).setdefault(s['cls'],{}):
   s['captures'][s['cls']][phase]=unreal.ShipCombatLibrary.capture_gameplay_hud(w,str(root/'Saved'/(Path(name).stem+'_'+s['cls']+'_'+phase+'.png')))
  if int(t)%5==0 and s.get('saved')!=int(t):s['saved']=int(t);save()
  if age>=durations[phase]:
   keys=list(durations);index=keys.index(phase)
   if index+1<len(keys):s.update(phase=keys[index+1],at=t);return
   result=summarize(s['cls']);s['results'].append(result);probe.destroy_actor();refs.pop('probe')
   for key,value in originals[s['cls']].items():refs['profile_'+s['cls']].set_editor_property(key,value)
   if len(s['results'])==len(classes):finish('PASSED')
   else:s['phase']='next';save()
 except Exception:s['error']=traceback.format_exc();finish('FAILED')
refs['handle']=unreal.register_slate_post_tick_callback(tick);save()
