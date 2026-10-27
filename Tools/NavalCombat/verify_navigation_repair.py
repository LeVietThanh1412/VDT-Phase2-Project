"""Real five-AI B/A/B acceptance with camera/terrain LOD changes.

MODE=runtime_reposition additionally relocates one active AI by 100 m, once,
through the explicitly permitted development operation. No drive, velocity,
arrival tolerance or physics override is used. Full samples stay in Saved.
"""
import builtins, json, hashlib, math, base64, traceback
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
mode=globals().get('MODE','saved_patrol')
raw_name='NAVIGATION_FIXED_'+mode.upper()+'_RAW.json'
required=2 if mode=='runtime_reposition' else 3
scope=dict(OUTPUT=raw_name,LIMIT=620,REQUIRED_LEGS=required,__name__='verified_navigation')
p=root/'Tools/NavalCombat/verify_patrol_loops.py'
exec(compile(p.read_text(encoding='utf-8'),str(p),'exec'),scope)
base=scope['state']
chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
cells=base64.b64decode(chart['cells_base64'])
st=dict(status='RUNNING',mode=mode,last=-1,camera_cycles=0,frames=0,ships={},
        terrain={},reposition=None,error=None,diagnostics=[])
builtins.navigation_repair_probe=st

def water(p):
    x=math.floor((p[0]-chart['min_east'])/chart['cell_size']);y=math.floor((p[1]-chart['min_north'])/chart['cell_size'])
    return 0<=x<chart['width'] and 0<=y<chart['height'] and cells[y*chart['width']+x]==1

def save():
    path=root/'Docs/NavalCombat'/('NAVIGATION_FIXED_'+mode.upper()+'.json')
    path.write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','diagnostics']},indent=2),encoding='utf-8')

def finish():
    st['loop_status']=base['status'];st['duration_seconds']=base['last'];st['arrivals']=base['arrivals']
    st['required_arrivals_per_ai']=required
    st['checks']=dict(all_five_required_arrivals=base['status']=='ALL_FIVE_LOOPS_VERIFIED' and len(base['arrivals'])>=5*required,
        exact_live_target_endpoints=not base['errors'],five_independent_controllers=len(st['ships'])==5 and all(s['controller_same'] for s in st['ships'].values()),
        continuously_physical=all(s['physical'] for s in st['ships'].values()),
        generated_sea_skirts_filtered=st['terrain'].get('removed_sea_skirt_triangles',0)>0,
        real_submerged_geometry_retained=st['terrain'].get('retained_submerged_triangles',0)>0)
    if mode=='runtime_reposition':
        st['checks']['automatic_reposition_replan']=bool(st['reposition'] and st['reposition'].get('replanned') and st['reposition'].get('targets_preserved'))
        if st['reposition']:
            rp=st['reposition'];rp.pop('replan_time',None)
            observed=[r['t'] for r in st['diagnostics'] for s in r['ships'] if s['label']==rp['ship'] and s['reposition_reinitializations']>rp['before_count'] and s['path_success']]
            rp['first_observed_replan_time']=min(observed) if observed else None
    st['status']='PASSED' if all(st['checks'].values()) and not st['error'] else 'FAILED'
    raw=root/'Docs/NavalCombat'/raw_name
    if raw.exists():
        data=raw.read_bytes();dest=root/'Saved'/raw_name;dest.write_bytes(data)
        st['raw_local_path']=str(dest);st['raw_sha256']=hashlib.sha256(data).hexdigest()
        raw.unlink() # Only this fixture's newly generated transient report.
    (root/'Saved'/('NavigationDiagnostics_'+mode+'.json')).write_text(json.dumps(st['diagnostics'],indent=2),encoding='utf-8')
    save();unreal.unregister_slate_post_tick_callback(st['handle'])

def tick(dt):
    w=ed.get_game_world()
    try:
        if not w:
            if base['status']!='RUNNING':finish()
            return
        t=unreal.GameplayStatics.get_time_seconds(w)
        if t<1:return
        st['frames']+=1
        fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipEnemyNavigationComponent)]
        for a in fleet:
            n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
            h=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
            label=a.get_actor_label();controller=a.get_controller().get_path_name() if a.get_controller() else None
            s=st['ships'].setdefault(label,dict(controller=controller,controller_same=True,physical=True,max_legs=0,
                max_repositions=0,max_speed_mps=0,contact_loss_s=0,max_contact_loss_s=0))
            s['controller_same'] &= controller==s['controller'] and not a.is_player_controlled()
            s['physical'] &= h.is_simulating_physics();s['max_legs']=max(s['max_legs'],n.patrol_legs_completed)
            s['max_speed_mps']=max(s['max_speed_mps'],h.get_physics_linear_velocity().length()/100)
            s['max_repositions']=max(s['max_repositions'],n.reposition_reinitializations)
            if t>12:
                s['contact_loss_s']=0 if a.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body() else s['contact_loss_s']+dt
                s['max_contact_loss_s']=max(s['max_contact_loss_s'],s['contact_loss_s'])
            recorded_leg=base['last_ships'].get(label,{}).get('legs',0)
            if mode=='runtime_reposition' and label=='BP_ShipEnemy2' and n.patrol_legs_completed>=1 and recorded_leg==n.patrol_legs_completed and not st['reposition']:
                anchor=a.get_component_by_class(unreal.CesiumGlobeAnchorComponent);g=anchor.resolve_georeference()
                pos=h.get_world_location();ecef=g.transform_unreal_position_to_earth_centered_earth_fixed(pos)
                en=[sum(([ecef.x,ecef.y,ecef.z][i]-chart['origin_ecef'][i])*chart[k][i] for i in range(3)) for k in ['east_axis','north_axis']]
                for offset in [100,-100,200,-200]:
                    goal=[en[0]+offset,en[1]]
                    if not water(goal):continue
                    target=pos+unreal.Vector(offset*100,0,0)
                    q=g.transform_unreal_position_to_earth_centered_earth_fixed(target)
                    actual_en=[sum(([q.x,q.y,q.z][i]-chart['origin_ecef'][i])*chart[k][i] for i in range(3)) for k in ['east_axis','north_axis']]
                    if not water(actual_en):continue
                    st['reposition']=dict(ship=label,time=t,before_count=n.reposition_reinitializations,
                        en_before=en,requested_offset_m=offset,a=n.patrol_actor_a.get_path_name(),b=n.patrol_actor_b.get_path_name())
                    a.set_actor_location(target,False,True);anchor.sync();break
            if st['reposition'] and label==st['reposition']['ship']:
                rp=st['reposition']
                if n.reposition_reinitializations>rp['before_count'] and n.pathfinding_succeeded:
                    rp['replanned']=True
                    rp['targets_preserved']=n.patrol_actor_a.get_path_name()==rp['a'] and n.patrol_actor_b.get_path_name()==rp['b']
        if int(t)//60>st['camera_cycles']:
            unreal.GameplayStatics.get_player_controller(w,0).cycle_ship_camera();st['camera_cycles']=int(t)//60
        if t-st['last']>=2:
            st['last']=t;st['diagnostics'].append(dict(t=t,ships=[json.loads(a.get_component_by_class(unreal.ShipEnemyNavigationComponent).get_navigation_diagnostics()) for a in fleet]))
            terrain=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Cesium3DTileset) if a.get_actor_label()=='QuangNinh_Terrain')
            f=terrain.get_component_by_class(unreal.SeaTerrainCollisionComponent)
            st['terrain']={k:f.get_editor_property(k) for k in ['filtered_tiles','removed_water_triangles','retained_land_triangles','retained_submerged_triangles','removed_sea_skirt_triangles','skirt_metadata_tiles','last_error']}
            save()
    except Exception:
        st['error']=traceback.format_exc();st['status']='FAILED';save()
        unreal.unregister_slate_post_tick_callback(st['handle'])
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()

st['handle']=unreal.register_slate_post_tick_callback(tick);save()
