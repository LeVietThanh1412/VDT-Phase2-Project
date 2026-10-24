"""Health/capsize deaths, real stages, geographic rebase, FX and cleanup.
Only forced capsize subjects receive fixture pose/angular-velocity changes.
"""
import builtins,json,math,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());st=dict(handle=None,phase='init',rows=[],checks=[],last=-1,error=None,shots=[])
builtins.destruction_test=st
def hull(s):return next(c for c in s.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
def hp(s):return s.get_component_by_class(unreal.ShipWeaponComponent)

def valid(s):
    # UE 5.6 may reject a stale PyUObject during argument marshaling instead of
    # returning false from IsValid after the native actor has been cleaned up.
    try:return unreal.SystemLibrary.is_valid(s)
    except TypeError:return False
def check(name,value):st['checks'].append(dict(name=name,passed=bool(value)));assert value,name
def save():
    (root/'Docs/NavalCombat/DESTRUCTION_V2_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','ships','victims','capsize','brief','camera','geo']},indent=2))
def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def roll(s,angle):
    r=s.get_actor_rotation();s.set_actor_rotation(unreal.Rotator(pitch=0,yaw=r.yaw,roll=angle),True)
def set_camera():
    g=st['geo'];focus=g.transform_longitude_latitude_height_position_to_unreal(unreal.Vector(*st['focus_llh']))
    # Existing regional ENU frame: a stable camera position relative to focus.
    up=(g.transform_longitude_latitude_height_position_to_unreal(unreal.Vector(*st['focus_llh'])+unreal.Vector(0,0,1))-focus).normal()
    eye=focus+unreal.Vector(30000,30000,0)+up*18000
    st['camera'].set_actor_location(eye,False,True);st['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye,focus),True)
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<1:return
    try:
        if st['phase']=='init':
            ships=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'));st['ships']=ships
            by={s.get_actor_label():s for s in ships};player=next(s for s in ships if s.is_player_controlled())
            st['victims']=[player,by['BP_ShipEnemy'],by['BP_ShipEnemy2'],by['Friendly_AI_1'],by['Friendly_AI_2']]
            st['capsize']=[by['Friendly_AI_1'],by['Friendly_AI_2']];st['brief']=by['BP_ShipEnemy3']
            for s in ships:
                c=s.get_component_by_class(unreal.ShipCombatComponent);n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=False
                if n:n.stop_navigation()
                s.set_actor_tick_enabled(False)
            for s in [by['BP_ShipEnemy2'],by['Friendly_AI_2']]:check('Large class applied',s.get_component_by_class(unreal.ShipMovementComponent).apply_ship_class(unreal.ShipHullClass.LARGE))
            st['geo']=by['BP_ShipEnemy'].get_component_by_class(unreal.CesiumGlobeAnchorComponent).resolve_georeference()
            g=st['geo'];p=g.transform_unreal_position_to_longitude_latitude_height(hull(by['BP_ShipEnemy']).get_world_location());st['focus_llh']=[p.x,p.y,p.z]
            gameplay=unreal.get_default_object(unreal.GameplayStatics.static_class());transform=unreal.Transform()
            camera=gameplay.call_method('BeginDeferredActorSpawnFromClass',args=(w,unreal.CameraActor.static_class(),transform,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
            gameplay.call_method('FinishSpawningActor',args=(camera,transform,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT));st['camera']=camera;set_camera()
            unreal.GameplayStatics.get_player_controller(w,0).set_view_target_with_blend(camera,0)
            st.update(phase='brief_roll',at=t)
        if st['phase']=='brief_roll':
            if t-st['at']<.22:roll(st['brief'],130)
            elif t-st['at']<.3:
                roll(st['brief'],0);hull(st['brief']).set_physics_angular_velocity_in_degrees(unreal.Vector(),False)
            elif t-st['at']>=1.5:
                check('brief roll does not kill',not hp(st['brief']).is_destroyed)
                for s in st['victims'][:3]:
                    attacker=st['brief'] if hp(s).team()==0 else st['victims'][0]
                    check('health depletion accepted',hp(s).receive_ship_damage(100,attacker))
                st.update(phase='deaths',death_at=t,initial={})
        if st['phase']=='deaths':
            for s in st['capsize']:
                if valid(s) and hp(s) and not hp(s).is_destroyed:roll(s,135)
            elapsed=t-st['death_at']
            if elapsed>2 and not st.get('confirmed'):
                check('all five deaths confirmed',all(hp(s).is_destroyed and hp(s).destruction_events==1 for s in st['victims']))
                check('capsize uses shared reason',all(hp(s).death_reason==unreal.ShipDeathReason.CAPSIZED for s in st['capsize']))
                check('health death uses shared reason',all(hp(s).death_reason==unreal.ShipDeathReason.HEALTH_DEPLETED for s in st['victims'][:3]))
                for s in st['victims']:
                    check('live hull retained during burn',valid(s) and not s.get_editor_property('hidden'))
                    check('burning FX active',hp(s).destruction_fx is not None and hp(s).destruction_fx.is_active())
                    check('rigid body disabled before animation',not hull(s).is_simulating_physics())
                    check('death cannot repeat',not hp(s).die(unreal.ShipDeathReason.OTHER,None) and hp(s).destruction_events==1)
                st['confirmed']=True
            if elapsed>6 and not st.get('rebased'):
                g=st['geo'];origin=g.get_origin_longitude_latitude_height();g.set_origin_longitude_latitude_height(origin+unreal.Vector(.015,.01,0));set_camera();st['rebased']=True
            if t-st['last']>.2:
                st['last']=t;rows=[]
                for s in st['victims']:
                    if not valid(s):continue
                    weapon=hp(s);anchor=s.get_component_by_class(unreal.CesiumGlobeAnchorComponent);p=anchor.get_longitude_latitude_height()
                    q=anchor.get_east_south_up_rotation().rotator()
                    rows.append(dict(name=s.get_actor_label(),stage=str(weapon.destruction_stage),dead=weapon.is_destroyed,deaths=weapon.destruction_events,hp=weapon.current_health,
                        llh=[p.x,p.y,p.z],sunk=weapon.sunk_distance_m,roll=q.roll,pitch=q.pitch,fx=weapon.destruction_fx.is_active() if weapon.destruction_fx else False,
                        physics=hull(s).is_simulating_physics(),buoyancy_tick=s.get_component_by_class(unreal.BuoyancyComponent).is_component_tick_enabled(),hidden=s.get_editor_property('hidden')))
                hud=unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.ShipHUDWidget,False)
                st['rows'].append(dict(t=t,elapsed=elapsed,ships=rows,hud=[dict(score=x.score_text,result=x.result_text) for x in hud]));save()
            for name,at in [('BURN',2.5),('SINK',10),('SUBMERGED',17)]:
                if elapsed>=at and name not in st['shots']:
                    unreal.SystemLibrary.execute_console_command(w,'Shot showui filename='+str(root/'Docs/NavalCombat'/('V2_'+name+'.png')));st['shots'].append(name)
            if elapsed>=27:
                living=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'))
                check('all wreck actors cleaned after stages',len(living)==1 and living[0]==st['brief'])
                hud=unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.ShipHUDWidget,False)[0]
                check('death counts stable',hud.score_text=='BLUE 0   |   RED 1' and hud.result_text=='DEFEAT')
                for name in [s['name'] for s in st['rows'][0]['ships']]:
                    rows=[s for r in st['rows'] for s in r['ships'] if s['name']==name and s['dead']]
                    check(name+' visible burning interval',any('BURNING' in r['stage'] and r['sunk']==0 for r in rows))
                    check(name+' progressive sinking',any(5<r['sunk']<55 for r in rows) and max(r['sunk'] for r in rows)>=64.9)
                    check(name+' geographic height decreases',all(b['llh'][2]<=a['llh'][2]+.03 for a,b in zip(rows,rows[1:])))
                    check(name+' longitude latitude stable',max(abs(r['llh'][0]-rows[0]['llh'][0])+abs(r['llh'][1]-rows[0]['llh'][1]) for r in rows)<1e-6)
                finish('DESTRUCTION_V2_CHECKS_PASSED')
        if t>40:raise AssertionError('destruction fixture timeout')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
