"""Bounded PIE fixtures. Changes are disposable and never saved to editor assets.

The separate sustained patrol run is movement evidence. Here navigation is
explicitly stopped to isolate mechanical aiming, roles, and firing rejection.
"""
import builtins,json,math
from pathlib import Path
import unreal

w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
e=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,"/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy_C"))[0]
p=unreal.GameplayStatics.get_player_pawn(w,0);assert p
c=e.get_component_by_class(unreal.ShipCombatComponent);n=e.get_component_by_class(unreal.ShipEnemyNavigationComponent)
weapon=e.get_component_by_class(unreal.ShipWeaponComponent);pw=p.get_component_by_class(unreal.ShipWeaponComponent)
mesh=e.get_component_by_class(unreal.SkeletalMeshComponent)
ph=next(x for x in p.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=="ShipSM")
eh=next(x for x in e.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=="ShipSM")
n.stop_navigation();ph.set_simulate_physics(False)
root=Path(unreal.Paths.project_dir());vec=lambda x:[x.x,x.y,x.z]
old=getattr(builtins,"combat_cases",None)
if old and old.get("handle"):unreal.unregister_slate_post_tick_callback(old["handle"])
st=dict(handle=None,index=-1,started=0,last=-1,rows=[],results=[],blocker=None,case=None,initial_shots=0)
builtins.combat_cases=st
cases=[
 dict(name="outside_detection",distance=1100,yaw=0,duration=3),
 dict(name="detected_outside_engagement",distance=650,yaw=0,duration=4),
 dict(name="left_tracking",distance=450,yaw=-45,duration=9,no_fire=True),
 dict(name="right_tracking",distance=450,yaw=45,duration=9,no_fire=True),
 dict(name="barrel_elevation",distance=450,yaw=30,height=100,duration=8,no_fire=True),
 dict(name="barrel_lowering",distance=450,yaw=30,height=0,duration=7,no_fire=True),
 dict(name="inside_engagement",distance=450,yaw=0,duration=13),
 dict(name="friendly_rejected",distance=450,yaw=0,duration=4,friendly=True),
 dict(name="invalid_rejected",distance=450,yaw=0,duration=4,dead=True),
 dict(name="island_like_blocker",distance=450,yaw=0,duration=5,block=True),
 dict(name="mechanical_yaw_limit",distance=450,yaw=170,duration=8),
 dict(name="mechanical_pitch_limit",distance=250,yaw=0,height=450,duration=8),
 dict(name="passive",distance=450,yaw=0,duration=5,role=unreal.ShipEnemyRole.PASSIVE),
 dict(name="coward",distance=450,yaw=0,duration=6,role=unreal.ShipEnemyRole.COWARD),
 dict(name="hunter_fire",distance=450,yaw=0,duration=13,role=unreal.ShipEnemyRole.HUNTER),
]
if globals().get('CASES_FILTER'):cases=[case for case in cases if case['name'] in CASES_FILTER]
def place(case):
    q=mesh.get_world_transform().transform_location(unreal.Vector(math.cos(math.radians(case['yaw']))*case['distance']*100,math.sin(math.radians(case['yaw']))*case['distance']*100,case.get('height',0)*100))
    p.set_actor_location(q,False,True)
    # Move the physical hull too only if Cesium/root updates left it behind.
    assert (ph.get_world_location()-q).length()<1000
    return q
def begin(index):
    st['index']=index;case=cases[index];st['case']=case;st['started']=unreal.GameplayStatics.get_time_seconds(w);st['rows']=[];st['initial_shots']=weapon.shots_fired
    if st['blocker']:st['blocker'].destroy_actor();st['blocker']=None
    p.tags=[t for t in p.tags if str(t)!='Dead']
    if case.get('dead'):p.tags=list(p.tags)+[unreal.Name('Dead')]
    pw.faction=1 if case.get('friendly') else 0
    n.role=case.get('role',unreal.ShipEnemyRole.FIGHTER)
    c.detection_radius_m=700;c.engagement_radius_m=0 if case.get('no_fire') else 500;c.lose_target_radius_m=800;c.require_line_of_sight=not case.get('no_fire',False)
    q=place(case)
    if case.get('block'):
        pos=(q+mesh.get_socket_location('front_gun_v_socket'))*.5
        transform=unreal.Transform(location=pos)
        gameplay=unreal.get_default_object(unreal.GameplayStatics.static_class())
        b=gameplay.call_method('BeginDeferredActorSpawnFromClass',args=(w,unreal.StaticMeshActor.static_class(),transform,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        assert b,"Disposable PIE blocker spawn failed"
        b.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
        b.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        b.set_actor_scale3d(unreal.Vector(80,80,200));b.static_mesh_component.set_collision_profile_name('BlockAll')
        gameplay.call_method('FinishSpawningActor',args=(b,transform,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT));b.set_actor_scale3d(unreal.Vector(80,80,200))
        b.set_actor_enable_collision(True)
        b.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
        b.static_mesh_component.set_collision_profile_name('BlockAll')
        b.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        b.static_mesh_component.set_collision_response_to_all_channels(unreal.CollisionResponse.BLOCK)
        st['blocker']=b
        print('BLOCKER',b.get_path_name(),b.get_actor_location(),b.get_actor_scale3d(),b.get_actor_bounds(False),b.static_mesh_component.get_collision_enabled())
    c.scan_targets()
    print('COMBAT CASE',case['name'])
def finish_case():
    case=st['case'];rows=st['rows'];last=rows[-1];shots=weapon.shots_fired-st['initial_shots'];name=case['name'];passed=False
    if name in ['outside_detection','friendly_rejected','invalid_rejected','passive']:passed=not last['target'] and shots==0
    elif name in ['detected_outside_engagement','coward']:passed=bool(last['target']) and shots==0
    elif name in ['left_tracking','right_tracking','barrel_elevation','barrel_lowering']:
        passed=bool(last['target']) and abs(last['yaw']-last['desired_yaw'])<2 and abs(last['pitch']-last['desired_pitch'])<2 and abs(last['anim_yaw']-last['yaw'])<2 and abs(last['anim_pitch']-last['pitch'])<2
    elif name in ['inside_engagement','hunter_fire']:passed=shots>=2
    elif name=='island_like_blocker':passed=shots==0 and not last['visible']
    elif name in ['mechanical_yaw_limit','mechanical_pitch_limit']:passed=shots==0 and any('mechanical' in row['status'] for row in rows)
    st['results'].append(dict(name=name,passed=passed,shots=shots,rows=rows))
    output=globals().get('OUTPUT','ISOLATED_CASES.json')
    (root/'Docs/NavalCombat'/output).write_text(json.dumps(dict(results=st['results']),indent=2),encoding='utf8')
def tick(dt):
    t=unreal.GameplayStatics.get_time_seconds(w)
    try:
        if st['index']<0:begin(0)
        if t-st['last']>.2:
            st['last']=t;st['rows'].append(dict(t=t,target=c.target.get_name() if c.target else None,status=c.firing_status,visible=c.target_visible,
                yaw=c.turret_yaw,desired_yaw=c.desired_turret_yaw,pitch=c.barrel_pitch,desired_pitch=c.desired_barrel_pitch,
                anim_yaw=mesh.get_anim_instance().get_editor_property('GunYaw'),anim_pitch=mesh.get_anim_instance().get_editor_property('GunPitch'),
                bone_h_rotation=str(mesh.get_socket_rotation('front_gun_h_jnt')),bone_v_rotation=str(mesh.get_socket_rotation('front_gun_v_jnt')),
                muzzle_rotation=str(mesh.get_socket_rotation('front_gun_v_socket')),hull_rotation=str(mesh.get_world_rotation()),shots=weapon.shots_fired))
        if t-st['started']>=st['case']['duration']:
            finish_case()
            if st['index']+1>=len(cases):
                unreal.unregister_slate_post_tick_callback(st['handle']);st['handle']=None
                if st['blocker']:st['blocker'].destroy_actor()
                print('COMBAT CASES COMPLETE',[(x['name'],x['passed']) for x in st['results']]);return
            begin(st['index']+1)
    except Exception as ex:
        st['error']=str(ex);unreal.unregister_slate_post_tick_callback(st['handle']);st['handle']=None;print('COMBAT CASE ERROR',ex)
st['handle']=unreal.register_slate_post_tick_callback(tick)
print('Bounded isolated combat fixtures started; all modifications are PIE-only')
