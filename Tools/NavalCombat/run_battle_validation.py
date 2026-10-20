"""Real PIE telemetry. MODE patrol disables combat only in the disposable world.
No transforms, physics commands, time dilation or speed settings are changed.
"""
import builtins,json,math,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());mode=globals().get('MODE','battle');limit=globals().get('LIMIT',360 if mode=='battle' else 1200)
chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
classes=globals().get('SHIP_CLASSES',{})
st=dict(handle=None,last=-1,rows=[],mode=mode,status='RUNNING',error=None)
builtins.validation_probe=st
def save():
    dest=root/'Docs/NavalCombat'/('VALIDATION_'+mode.upper()+'.json')
    temporary=dest.with_suffix('.tmp')
    temporary.write_text(json.dumps({k:v for k,v in st.items() if k!='handle'},indent=2))
    temporary.replace(dest)
def end(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle']);st['handle']=None
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<.01 or t-st['last']<1:return
    st['last']=t
    try:
        ships=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'))
        rows=[]
        for s in ships:
            movement=s.get_component_by_class(unreal.ShipMovementComponent)
            configured=st.setdefault('profile_configured',[])
            if s.get_actor_label() in classes and s.get_actor_label() not in configured:
                assert movement.apply_ship_class(getattr(unreal.ShipHullClass,classes[s.get_actor_label()].upper()))
                configured.append(s.get_actor_label())
            c=s.get_component_by_class(unreal.ShipCombatComponent);n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent);weapon=s.get_component_by_class(unreal.ShipWeaponComponent)
            if mode=='patrol' and c:c.enable_combat=False
            h=next(x for x in s.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM');m=s.get_component_by_class(unreal.SkeletalMeshComponent)
            g=s.get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference();origin=unreal.GameplayStatics.get_world_origin_location(w)
            ecef=g.transform_unreal_position_to_earth_centered_earth_fixed(h.get_world_location()+unreal.Vector(origin.x,origin.y,origin.z))
            d=[ecef.x-chart['origin_ecef'][0],ecef.y-chart['origin_ecef'][1],ecef.z-chart['origin_ecef'][2]]
            en=[sum(d[i]*chart[k][i] for i in range(3)) for k in ['east_axis','north_axis']]
            v=lambda q:[q.x,q.y,q.z]
            r=dict(name=s.get_actor_label(),team=weapon.team(),hp=weapon.current_health,dead=weapon.is_destroyed,player=s.is_player_controlled(),en_m=en,
                heading=h.get_world_rotation().yaw,velocity=v(h.get_physics_linear_velocity()),physics=h.is_simulating_physics(),buoyancy=bool(s.get_component_by_class(unreal.BuoyancyComponent)),
                shots=weapon.shots_fired,hits=weapon.hits,damage_events=weapon.accepted_damage_events,death_events=weapon.destruction_events,
                muzzle=v(m.get_socket_location('front_gun_v_socket')),muzzle_rotation=str(m.get_socket_rotation('front_gun_v_socket')),
                bone_h=str(m.get_socket_rotation('front_gun_h_jnt')),bone_v=str(m.get_socket_rotation('front_gun_v_jnt')),
                anim_yaw=m.get_anim_instance().get_editor_property('GunYaw'),anim_pitch=m.get_anim_instance().get_editor_property('GunPitch'))
            if n:r.update(nav=str(n.navigation_state),failure=n.last_navigation_failure_reason,path=n.pathfinding_succeeded,legs=n.patrol_legs_completed,
                waypoint=n.current_waypoint_index,route=[v(q) for q in n.current_route],throttle=n.desired_throttle,rudder=n.desired_rudder,
                goal=v(n.current_final_destination),yielding=n.yielding_to_ship,a=n.patrol_actor_a.get_actor_label(),b=n.patrol_actor_b.get_actor_label())
            if c:r.update(tactic=str(c.tactical_state),target=c.target.get_actor_label() if c.target else None,
                target_team=c.target.get_component_by_class(unreal.ShipWeaponComponent).team() if c.target else None,distance=c.distance_to_target_m,
                retreats=c.retreats_started,resumed=c.patrols_resumed,yaw=c.turret_yaw,pitch=c.barrel_pitch,
                desired_yaw=c.desired_turret_yaw,desired_pitch=c.desired_barrel_pitch,status=c.firing_status)
            if movement:
                llh=g.transform_unreal_position_to_longitude_latitude_height(h.get_world_location())
                up=(g.transform_longitude_latitude_height_position_to_unreal(llh+unreal.Vector(0,0,1))-h.get_world_location()).normal()
                r.update(hull_class=str(movement.hull_class),mass=h.get_mass(),height_m=llh.z,up_dot=unreal.MathLibrary.dot_vector_vector(h.get_up_vector(),up),water_overlap=s.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body())
            rows.append(r)
        hud=unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.ShipHUDWidget,False)
        living=[r for r in rows if not r['dead']]
        pairs=[math.dist(a['en_m'],b['en_m']) for i,a in enumerate(living) for b in living[i+1:]]
        effects=[]
        if mode=='battle':
            for fx in unreal.ObjectIterator(unreal.NiagaraComponent):
                if fx.is_active() and abs(fx.get_world_scale().x-12)<.01:
                    effects.append(dict(asset=fx.get_asset().get_path_name(),position=v(fx.get_world_location()),scale=v(fx.get_world_scale())))
        st['rows'].append(dict(t=t,ships=rows,min_spacing_m=min(pairs) if pairs else None,destruction_effects=effects,hud=[dict(score=h.score_text,hp=h.health_text,result=h.result_text) for h in hud]))
        if len(st['rows'])%15==0:
            try:save()
            except OSError:pass # A transient reader/antivirus lock must not terminate the simulation.
        if t>limit:end('BOUNDED_RUN_COMPLETE')
        elif mode=='patrol' and len(rows)==6 and all(r.get('legs',0)>=2 for r in rows if not r['player']):end('ALL_FIVE_COMPLETED_TWO_LEGS')
        elif mode=='battle' and hud and hud[0].result_text in ['VICTORY','DEFEAT']:end('BATTLE_FINISHED')
    except Exception:st['error']=traceback.format_exc();end('PROBE_ERROR')
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
