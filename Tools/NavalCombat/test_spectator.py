"""Saved-level death cameras and wreck fallback; disposable, bounded real PIE.

CASE=health/capsize/free. Never possesses an AI, edits assets or changes routes.
"""
import builtins,json,traceback,math
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());case=globals().get('CASE','health')
st=dict(handle=None,checks=[],phase=0,error=None)
builtins.naval_spectator_test=st
def check(name,value):
    st['checks'].append(dict(name=name,passed=bool(value)));assert value,name
def weapon(p):return p.get_component_by_class(unreal.ShipWeaponComponent)
def valid(p):
    try:return unreal.SystemLibrary.is_valid(p)
    except TypeError:return False
def finish(status):
    st['status']=status
    (root/'Docs/NavalCombat'/('SPECTATOR_'+case.upper()+'.json')).write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','ships','pc','player','enemy','friends','controllers','geo','camera','followed']},indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    try:
        if t<5:return
        if st['phase']==0:
            pc=unreal.GameplayStatics.get_player_controller(w,0);p=unreal.GameplayStatics.get_player_pawn(w,0)
            ships=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if weapon(a)]
            friends=[a for a in ships if a!=p and weapon(a).team()==0];enemy=next(a for a in ships if weapon(a).team()==1)
            check('six ships, two Blue AI',len(ships)==6 and len(friends)==2)
            for a in ships:
                c=a.get_component_by_class(unreal.ShipCombatComponent)
                if c:c.enable_combat=False
            effects=p.get_components_by_class(unreal.ShipWreckBillboardComponent)
            check('five saved effects hidden alive',len(effects)==5 and all(not e.wreck_visual_active and not e.is_visible() for e in effects))
            st.update(pc=pc,player=p,ships=ships,friends=friends,enemy=enemy,controllers=[a.get_controller() for a in ships if a!=p],phase=1)
            if case=='free':pc.toggle_free_camera();check('free camera before death',unreal.GameplayStatics.get_player_pawn(w,0)!=p)
            if case=='capsize':
                p.set_actor_tick_enabled(False)
                r=p.get_actor_rotation();r.roll=135;p.set_actor_rotation(r,True)
            elif case=='projectile':
                for a in [p,enemy]:
                    a.set_actor_tick_enabled(False)
                    n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                    if n:n.stop_navigation()
                    next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM').set_simulate_physics(False)
                enemy.set_actor_location(enemy.get_actor_location()+unreal.Vector(0,0,100000),False,True)
                mesh=enemy.get_component_by_class(unreal.SkeletalMeshComponent)
                start=mesh.get_socket_location('front_gun_v_socket');direction=mesh.get_socket_rotation('front_gun_v_socket').get_forward_vector()
                p.set_actor_rotation(unreal.Rotator(yaw=math.degrees(math.atan2(direction.y,direction.x))+90),True)
                body=next(c for c in p.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM');low,high=body.get_local_bounds()
                point=body.get_world_transform().transform_location(unreal.Vector((low.x+high.x)/2,0,0))
                endpoint=start+direction*30000+unreal.Vector(0,0,.5*-980*1.5**2)
                p.set_actor_location(p.get_actor_location()+endpoint-point,False,True)
                check('prepare player 10 HP',weapon(p).receive_ship_damage(90,enemy))
                check('original projectile fired',weapon(enemy).try_fire_weapon(unreal.Vector(),None,unreal.Vector(),False))
            else:check('shared health death',weapon(p).receive_ship_damage(100,enemy))
        if case=='capsize' and st['phase']==1 and not weapon(st['player']).is_destroyed:
            r=st['player'].get_actor_rotation();r.roll=135;st['player'].set_actor_rotation(r,True)
        if case=='projectile' and st['phase']==1 and not weapon(st['player']).is_destroyed:
            if t>10:raise AssertionError('real player projectile impact did not kill')
            return
        if st['phase']==1 and weapon(st['player']).is_destroyed:
            # Slate callbacks can observe projectile death before this frame's
            # PlayerTick. Allow the next controller tick, bounded at 0.2 s.
            if 'death_observed' not in st:st['death_observed']=t
            if t-st['death_observed']<.2:return
        if t>=6 and st['phase']==1:
            if case=='capsize':check('confirmed player capsize death',weapon(st['player']).death_reason==unreal.ShipDeathReason.CAPSIZED)
            if case=='projectile':check('real projectile killed player',weapon(st['player']).last_applied_impact_damage==10 and weapon(st['player']).death_reason==unreal.ShipDeathReason.HEALTH_DEPLETED)
            pc=st['pc'];check('automatic spectator',pc.death_spectating)
            check('automatic Blue teammate follow',pc.followed_teammate in st['friends'] and pc.get_view_target()==pc.followed_teammate)
            check('camera pawn owns player possession',isinstance(unreal.GameplayStatics.get_player_pawn(w,0),unreal.FreeCameraPawn))
            check('five AI controllers preserved',all(a.get_controller()==c for a,c in zip([a for a in st['ships'] if a!=st['player']],st['controllers'])))
            check('no blocked camera input',not pc.is_move_input_ignored() and not pc.is_look_input_ignored())
            check('saved effects visible after death',all(e.wreck_visual_active and e.is_visible() for e in st['player'].get_components_by_class(unreal.ShipWreckBillboardComponent)))
            before=pc.followed_teammate;pc.next_teammate();check('Tab handler changes teammate',pc.followed_teammate!=before and pc.followed_teammate in st['friends'])
            st.update(phase=2)
        if t>=8 and st['phase']==2:
            pc=st['pc'];pc.show_battlefield_overview();camera=unreal.GameplayStatics.get_player_pawn(w,0)
            check('M handler overview',pc.followed_teammate is None and pc.get_view_target()==camera)
            anchor=camera.get_component_by_class(unreal.CesiumGlobeAnchorComponent);check('camera has geographic anchor',anchor is not None)
            geo=anchor.resolve_georeference();p=anchor.get_longitude_latitude_height()
            st.update(camera=camera,geo=geo,camera_llh=[p.x,p.y,p.z],phase=3)
            geo.set_origin_longitude_latitude_height(geo.get_origin_longitude_latitude_height()+unreal.Vector(.015,.01,0))
        if t>=10 and st['phase']==3:
            p=st['camera'].get_component_by_class(unreal.CesiumGlobeAnchorComponent).get_longitude_latitude_height();before=st['camera_llh']
            check('overview stays geographic after rebase',abs(p.x-before[0])+abs(p.y-before[1])<1e-5 and abs(p.z-before[2])<1)
            st['pc'].next_teammate();st['followed']=st['pc'].followed_teammate
            check('follow resumes after overview',st['followed'] in st['friends'])
            weapon(st['followed']).receive_ship_damage(100,st['enemy']);st['phase']=4
        if t>=12 and st['phase']==4:
            pc=st['pc'];check('dead followed teammate replaced',pc.followed_teammate in st['friends'] and pc.followed_teammate!=st['followed'] and not weapon(pc.followed_teammate).is_destroyed)
            weapon(pc.followed_teammate).receive_ship_damage(100,st['enemy']);st['phase']=5
        if t>=14 and st['phase']==5:
            pc=st['pc'];check('no friends uses valid overview',pc.followed_teammate is None and valid(pc.get_view_target()) and pc.get_view_target()==st['camera'])
            pc.next_teammate();pc.toggle_free_camera();check('no dead pawn restored',unreal.GameplayStatics.get_player_pawn(w,0)==st['camera'])
            check('team elimination remains defeat',unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.ShipHUDWidget,False)[0].result_text=='DEFEAT')
            st['phase']=6
        if t>=25 and st['phase']==6:
            check('submerged fallback hidden',all(not e.wreck_visual_active and not e.is_visible() for e in st['player'].get_components_by_class(unreal.ShipWreckBillboardComponent)))
            st['phase']=7
        if t>=40 and st['phase']==7:
            check('all Blue wrecks cleaned',not valid(st['player']) and all(not valid(p) for p in st['friends']))
            check('camera survives wreck cleanup',valid(st['camera']) and valid(st['pc'].get_view_target()))
            check('Red AI controllers still intact',all(a.get_controller()==c for a,c in zip([a for a in st['ships'] if a!=st['player']],st['controllers']) if valid(a)))
            finish('PASSED')
        if t>=45:raise AssertionError('spectator test timeout')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
