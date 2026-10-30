"""120 actual game seconds per hull in one mixed seeded physical match.
Read-only after menu initialization; original patrol forces drive all five AI.
"""
import builtins,json,time,traceback,math,hashlib
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
name=globals().get('OUTPUT','HULL_STABILITY_120S.json')
scope=dict(SEED=20261028,HULL_CLASSES=[0,1,2,0,0,2],OBSERVE_SECONDS=99999,WALL_LIMIT_SECONDS=900,OUTPUT='STABILITY_STARTUP.json',OUTPUT_DIR='Saved',__name__='stability_start')
p=root/'Tools/NavalCombat/test_seeded_spawn.py';exec(compile(p.read_text(encoding='utf-8'),str(p),'exec'),scope)
base=scope['st'];st=dict(status='RUNNING',phase='startup',rows=[],ships={},checks={},error=None,last=-1,started=time.monotonic(),native_dll_sha256=hashlib.sha256((root/'Binaries/Win64/UnrealEditor-VDTPhase2Project.dll').read_bytes()).hexdigest())
builtins.hull_stability_probe=st

def save():
 (root/'Saved'/name).write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','fleet','ocean']},indent=2),encoding='utf-8')
def percentile(values,f):
 v=sorted(values);return v[min(len(v)-1,int((len(v)-1)*f))]
def finish(status):
 st['status']=status;save()
 (root/'Docs/QuangNinhIntegration'/name).write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','fleet','ocean','rows']},indent=2),encoding='utf-8')
 unreal.unregister_slate_post_tick_callback(st['handle'])
 if base.get('status')=='RUNNING':unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_STABILITY_FIXTURE'
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
 try:
  if base.get('status')=='FAILED':raise AssertionError(base.get('error'))
  w=ed.get_game_world()
  if not w:return
  t=unreal.GameplayStatics.get_time_seconds(w)
  if st['phase']=='startup':
   if base.get('phase')!='observe':return
   fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipMovementComponent)]
   st.update(phase='settle',ready_at=t,fleet=fleet,ocean=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.WaterBodyOcean)[0].get_water_body_component());return
  if st['phase']=='settle':
   if t-st['ready_at']<5:return
   st.update(phase='measure',at=t)
  if t-st['last']<.1:return
  elapsed=t-st['last'] if st['last']>=0 else 0;st['last']=t;samples=[]
  for a in st['fleet']:
   m=a.get_component_by_class(unreal.ShipMovementComponent);h=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM');b=a.get_component_by_class(unreal.BuoyancyComponent);n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
   pos=h.get_world_location();rot=h.get_world_rotation();v=h.get_physics_linear_velocity();omega=h.get_physics_angular_velocity_in_degrees();surface=st['ocean'].get_water_surface_info_at_location(pos,False)[0];contact=b.is_in_water_body()
   r=st['ships'].setdefault(a.get_actor_label(),dict(hull_class=str(m.hull_class),ai=bool(n),initial=list(pos.to_tuple()),initial_yaw=rot.yaw,samples=0,water_samples=0,loss_seconds=0,max_loss_seconds=0,physical_all=True,profiles={k:str(m.profile.get_editor_property(k)) for k in ['mass_kg','linear_damping','angular_damping','vertical_damping_multiplier','displacement_multiplier']},max_travel_m=0,max_yaw_change_deg=0,max_speed_mps=0,max_abs_vz_mps=0,max_angular_speed_dps=0,roll=[],pitch=[],vz=[],heave=[],patrol_arrivals=0))
   r['samples']+=1;r['water_samples']+=int(contact);r['physical_all'] &= h.is_simulating_physics();r['loss_seconds']=0 if contact else r['loss_seconds']+elapsed;r['max_loss_seconds']=max(r['max_loss_seconds'],r['loss_seconds'])
   r['max_travel_m']=max(r['max_travel_m'],math.dist(r['initial'][:2],[pos.x,pos.y])/100);r['max_yaw_change_deg']=max(r['max_yaw_change_deg'],abs((rot.yaw-r['initial_yaw']+180)%360-180));r['max_speed_mps']=max(r['max_speed_mps'],v.length()/100);r['max_abs_vz_mps']=max(r['max_abs_vz_mps'],abs(v.z)/100);r['max_angular_speed_dps']=max(r['max_angular_speed_dps'],omega.length())
   r['roll'].append(abs(rot.roll));r['pitch'].append(abs(rot.pitch));r['vz'].append(abs(v.z)/100);r['heave'].append((pos.z-surface.z)/100)
   if n:r['patrol_arrivals']=n.patrol_legs_completed
   samples.append(dict(ship=a.get_actor_label(),hull_class=str(m.hull_class),roll=rot.roll,pitch=rot.pitch,heave_m=(pos.z-surface.z)/100,vz_mps=v.z/100,speed_mps=v.length()/100,yaw=rot.yaw,angular_dps=list(omega.to_tuple()),contact=contact))
  st['rows'].append(dict(t=t,ships=samples))
  if len(st['rows'])%50==0:save()
  if t-st['at']>=120:
   for r in st['ships'].values():
    r.update(contact_fraction=r['water_samples']/r['samples'],max_roll_deg=max(r['roll']),max_pitch_deg=max(r['pitch']),p95_roll_deg=percentile(r['roll'],.95),p95_pitch_deg=percentile(r['pitch'],.95),p99_vz_mps=percentile(r['vz'],.99),heave_range_m=max(r['heave'])-min(r['heave']))
    r['checks']=dict(physical=r['physical_all'],water_contact=r['contact_fraction']>=.98 and r['max_loss_seconds']<.5,no_uncontrolled_roll_pitch=r['max_roll_deg']<35 and r['max_pitch_deg']<25 and r['p95_roll_deg']<15 and r['p95_pitch_deg']<15,vertical_response=r['max_abs_vz_mps']<6 and r['p99_vz_mps']<3.5 and r['heave_range_m']<6,responsive=(not r['ai']) or (r['max_travel_m']>100 and r['max_speed_mps']>5 and r['max_yaw_change_deg']>15))
    for k in ['roll','pitch','vz','heave']:del r[k]
   st['duration_game_seconds']=t-st['at'];st['checks']=dict(all_three_hull_classes=len(set(r['hull_class'] for r in st['ships'].values()))==3,full_120_seconds=st['duration_game_seconds']>=120,all_ships_stable_and_responsive=all(all(r['checks'].values()) for r in st['ships'].values()))
   finish('PASSED' if all(st['checks'].values()) else 'FAILED');return
  if time.monotonic()-st['started']>600:raise AssertionError('Physics fixture wall time bound')
 except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick);save()
