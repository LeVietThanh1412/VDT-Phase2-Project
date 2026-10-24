"""Measure wave contact loss at frame resolution, with original hull physics.

Waves can briefly lift pontoons out of water. Measure the duration rather than
rounding a one-second sample ratio up to 99 percent. No movement override.
"""
import builtins, json, traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
output = globals().get('OUTPUT','FLOTATION_FINAL_FILTER.json')
st = dict(handle=None, rows=[], ships={}, error=None, last=-1)
builtins.flotation_probe = st

def finish(status):
    st['status'] = status
    unreal.unregister_slate_post_tick_callback(st['handle'])
    (root/'Docs/QuangNinhIntegration'/output).write_text(json.dumps({k:v for k,v in st.items() if k!='handle'},indent=2))
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()

def tick(dt):
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    try:
        t = unreal.GameplayStatics.get_time_seconds(w)
        if t<1:return
        for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn):
            n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
            c=a.get_component_by_class(unreal.ShipCombatComponent)
            if c:c.enable_combat=False
            a.can_be_damaged=False
            if not n:continue
            h=next(x for x in a.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM')
            b=a.get_component_by_class(unreal.BuoyancyComponent)
            name=a.get_actor_label()
            r=st['ships'].setdefault(name,dict(last_t=t,frames=0,contact_frames=0,loss_seconds=0,max_loss_seconds=0,max_abs_vertical_speed_mps=0,max_speed_mps=0,min_root_z_m=1e10,max_root_z_m=-1e10,physical_all=True,ai_all=True,initial=[h.get_world_location().x,h.get_world_location().y],last=[]))
            elapsed=t-r['last_t'];r['last_t']=t
            if t<12:continue
            contact=b.is_in_water_body();v=h.get_physics_linear_velocity();p=h.get_world_location()
            r['frames']+=1;r['contact_frames']+=int(contact)
            r['loss_seconds']=0 if contact else r['loss_seconds']+elapsed
            r['max_loss_seconds']=max(r['max_loss_seconds'],r['loss_seconds'])
            r['max_abs_vertical_speed_mps']=max(r['max_abs_vertical_speed_mps'],abs(v.z)/100)
            r['max_speed_mps']=max(r['max_speed_mps'],v.length()/100)
            r['min_root_z_m']=min(r['min_root_z_m'],p.z/100);r['max_root_z_m']=max(r['max_root_z_m'],p.z/100)
            r['physical_all'] &= h.is_simulating_physics();r['ai_all'] &= bool(a.get_controller() and not a.is_player_controlled())
            r['last']=[p.x,p.y]
        if t-st['last']>=1:
            st['last']=t;st['rows'].append(dict(t=t,ships={k:dict(v) for k,v in st['ships'].items()}))
        if t>=65:
            records=list(st['ships'].values())
            # Root must remain at the authored sea surface, not supported above
            # it or falling below it; no half-second sustained pontoon loss.
            st['checks']={
                'five_physical_AI':len(records)==5 and all(r['physical_all'] and r['ai_all'] for r in records),
                'all_move_under_original_propulsion':all(r['max_speed_mps']>10 for r in records),
                'no_sustained_pontoon_contact_loss':all(r['frames']>500 and r['max_loss_seconds']<.5 for r in records),
                'hulls_remain_near_authored_ocean':all(-1<r['min_root_z_m']<r['max_root_z_m']<7 for r in records),
            }
            for r in records:r['contact_ratio']=r['contact_frames']/max(1,r['frames'])
            finish('FLOTATION_VERIFIED' if all(st['checks'].values()) else 'ASSERTION_FAILURE')
    except Exception:st['error']=traceback.format_exc();finish('PROBE_ERROR')

st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
