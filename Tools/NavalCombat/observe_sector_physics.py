"""Read-only physics measurements during an existing PIE fixture.

This never changes mass, buoyancy, input, velocity, transforms or controllers.
Samples and contact fractions are evidence, not a stability acceptance claim.
"""
import builtins,json,time,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
st=dict(status='RUNNING',rows=[],started=time.monotonic(),last_world_seen=time.monotonic(),last=-1,error=None)
builtins.sector_physics_observer=st
def save():
    p=root/'Saved/SECTOR_PHYSICS_SAMPLES.json'
    p.write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle']},indent=2),encoding='utf-8')
def finish():
    st['status']='MEASURED' if st['rows'] and not st['error'] else 'NO_ACCEPTED_MEASUREMENT'
    summary={}
    for row in st['rows']:
        for s in row['ships']:
            a=summary.setdefault(s['label'],dict(hull_class=s['hull_class'],samples=0,water_samples=0,max_abs_roll_deg=0,max_abs_pitch_deg=0,max_vertical_speed_mps=0,min_heave_m=1e9,max_heave_m=-1e9))
            a['samples']+=1;a['water_samples']+=int(s['in_water'])
            a['max_abs_roll_deg']=max(a['max_abs_roll_deg'],abs(s['roll_deg']))
            a['max_abs_pitch_deg']=max(a['max_abs_pitch_deg'],abs(s['pitch_deg']))
            a['max_vertical_speed_mps']=max(a['max_vertical_speed_mps'],abs(s['vertical_speed_mps']))
            a['min_heave_m']=min(a['min_heave_m'],s['heave_m']);a['max_heave_m']=max(a['max_heave_m'],s['heave_m'])
    for a in summary.values():a['water_contact_fraction']=a['water_samples']/max(1,a['samples'])
    (root/'Docs/NavalCombat/SECTOR_PHYSICS_BASELINE.json').write_text(json.dumps(dict(status=st['status'],scope='Read-only current sector startup/travel; no tuning or all-class acceptance claim',ships=summary,error=st['error'],raw_path=str(root/'Saved/SECTOR_PHYSICS_SAMPLES.json')),indent=2),encoding='utf-8')
    save();unreal.unregister_slate_post_tick_callback(st['handle'])
def tick(dt):
    try:
        w=ed.get_game_world()
        if not w:
            if time.monotonic()-st['last_world_seen']>4:finish()
            return
        st['last_world_seen']=time.monotonic()
        field=lib.call_method('GetWorldSubsystem',args=(w,unreal.NavalBattlefieldSubsystem.static_class()))
        t=unreal.GameplayStatics.get_time_seconds(w)
        if not field or not field.ready or t-st['last']<.1:return
        st['last']=t
        oceans=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.WaterBodyOcean)
        ocean=oceans[0].get_water_body_component();ships=[]
        for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn):
            m=a.get_component_by_class(unreal.ShipMovementComponent)
            if not m:continue
            h=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
            pos=h.get_world_location();r=h.get_world_rotation();v=h.get_physics_linear_velocity();water=ocean.get_water_surface_info_at_location(pos,False)[0]
            ships.append(dict(label=a.get_actor_label(),hull_class=str(m.hull_class),in_water=a.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body(),physics=h.is_simulating_physics(),roll_deg=r.roll,pitch_deg=r.pitch,heave_m=(pos.z-water.z)/100,vertical_speed_mps=v.z/100,speed_mps=v.length()/100))
        st['rows'].append(dict(t=t,seed=field.actual_seed,ships=ships))
        if len(st['rows'])%50==0:save()
        if time.monotonic()-st['started']>150:finish()
    except Exception:st['error']=traceback.format_exc();finish()
st['handle']=unreal.register_slate_post_tick_callback(tick)
