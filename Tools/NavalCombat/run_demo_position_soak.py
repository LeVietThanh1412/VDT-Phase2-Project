"""Fresh saved-layout B/A/B validation plus frame contact/controller monitoring.

Only this disposable PIE fixture disables combat. No movement force, speed,
position, arrival threshold or active controller is overridden during the run.
"""
import builtins,json,time,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
st=dict(handle=None,status='RUNNING',frames=0,ships={},error=None,last_save=-1,started=time.monotonic(),camera_cycles=0)
builtins.demo_position_soak=st
scope=dict(OUTPUT='DEMO_SAVED_PATROL_LOOPS.json',LIMIT=600,REQUIRED_LEGS=3,__name__='demo_saved_loops')
p=root/'Tools/NavalCombat/verify_patrol_loops.py';exec(compile(p.read_text(),str(p),'exec'),scope)
base=scope['state'];base['scope_note']='New saved-position check with real Small/Medium default fleet; not the future randomized island-field soak.'
scope['save']()
def save():
    (root/'Docs/NavalCombat/DEMO_SAVED_POSITION_FRAME_QA.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','controllers']},indent=2),encoding='utf-8')
def finish(status):
    st['status']=status;st['loop_status']=base['status'];st['arrivals']=len(base['arrivals']);st['elapsed_wall_s']=time.monotonic()-st['started']
    save();unreal.unregister_slate_post_tick_callback(st['handle'])
def tick(dt):
    w=ed.get_game_world()
    try:
        if not w:
            if base['status']!='RUNNING':
                st['checks']=dict(all_five_complete_B_A_B=base['status']=='ALL_FIVE_LOOPS_VERIFIED' and len(base['arrivals'])>=15,
                    exact_live_target_endpoints=not base['errors'],five_controllers_preserved=len(st['ships'])==5 and all(r['controller_same'] for r in st['ships'].values()),
                    all_remain_physical=all(r['physical_all'] for r in st['ships'].values()),
                    no_sustained_wave_contact_loss=all(r['max_contact_loss_s']<.5 for r in st['ships'].values()))
                finish('PASSED' if all(st['checks'].values()) else 'FAILED')
            elif st['frames']:
                base['status']='INTERRUPTED';scope['save']();finish('INTERRUPTED')
            return
        t=unreal.GameplayStatics.get_time_seconds(w)
        if t<3:return
        fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipEnemyNavigationComponent)]
        if 'controllers' not in st:
            st['controllers']={a.get_actor_label():a.get_controller().get_path_name() for a in fleet}
        st['frames']+=1
        for a in fleet:
            label=a.get_actor_label();n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent);h=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
            r=st['ships'].setdefault(label,dict(controller_same=True,physical_all=True,frames=0,contact_frames=0,loss=0,max_contact_loss_s=0,last_t=t,min_root_z_m=1e9,max_root_z_m=-1e9,max_speed_mps=0,max_legs=0,max_stationary_eligible_s=0,stationary_s=0))
            elapsed=t-r['last_t'];r['last_t']=t
            r['controller_same'] &= bool(a.get_controller() and a.get_controller().get_path_name()==st['controllers'][label] and not a.is_player_controlled())
            r['physical_all'] &= h.is_simulating_physics()
            if t<12:continue
            contact=a.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body();speed=h.get_physics_linear_velocity().length()/100
            r['frames']+=1;r['contact_frames']+=int(contact);r['loss']=0 if contact else r['loss']+elapsed;r['max_contact_loss_s']=max(r['max_contact_loss_s'],r['loss'])
            r['min_root_z_m']=min(r['min_root_z_m'],h.get_world_location().z/100);r['max_root_z_m']=max(r['max_root_z_m'],h.get_world_location().z/100)
            r['max_speed_mps']=max(r['max_speed_mps'],speed);r['max_legs']=max(r['max_legs'],n.patrol_legs_completed)
            yaw=abs(unreal.MathLibrary.dot_vector_vector(h.get_physics_angular_velocity_in_degrees(),h.get_up_vector()))
            eligible=str(n.navigation_state) not in ['<ShipNavigationState.ARRIVED: 9>','<ShipNavigationState.IDLE: 0>'] and (abs(n.desired_throttle)>.08 or abs(n.desired_rudder)>.2)
            r['stationary_s']=r['stationary_s']+elapsed if eligible and speed<.5 and yaw<.1 else 0
            r['max_stationary_eligible_s']=max(r['max_stationary_eligible_s'],r['stationary_s'])
        if int(t)//60>st['camera_cycles']:
            unreal.GameplayStatics.get_player_controller(w,0).cycle_ship_camera();st['camera_cycles']=int(t)//60
        if t-st['last_save']>10:st['last_save']=t;save()
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick);save()
