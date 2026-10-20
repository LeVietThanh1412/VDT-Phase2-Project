"""Inject real Enhanced Input actions into the possessed player in disposable PIE."""
import builtins,json,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());st=dict(handle=None,rows=[],error=None,initialized=False)
actions={k:unreal.load_asset('/Game/Ship/Inputs/'+k) for k in ['IA_MoveForwardBackward','IA_MoveLeftRight','IA_LookAround','IA_Aim','IA_Fire','IA_Zoom']}
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<1:return
    try:
        p=unreal.GameplayStatics.get_player_pawn(w,0);pc=p.get_controller();weapon=p.get_component_by_class(unreal.ShipWeaponComponent)
        inputs=next(x for x in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not x.get_name().startswith('Default__'))
        if not st['initialized']:
            for s in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C')):
                s.can_be_damaged=False;c=s.get_component_by_class(unreal.ShipCombatComponent);n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=False
                if n:n.stop_navigation()
            st['initialized']=True
        def inject(name,value):inputs.inject_input_vector_for_action(actions[name],unreal.Vector(*value),[],[])
        if 2<t<8:inject('IA_MoveForwardBackward',(1,0,0))
        if 8<t<12:
            inject('IA_MoveForwardBackward',(.5,0,0));inject('IA_MoveLeftRight',(1,0,0))
        if 12<t<13:inject('IA_LookAround',(.4,.2,0))
        if 14<t<18.5:inject('IA_Aim',(1,0,0))
        if 16<t<17:inject('IA_Fire',(1,0,0))
        if 18<t<18.2:inject('IA_Zoom',(1,0,0))
        if not st['rows'] or t-st['rows'][-1]['t']>.1:
            v=p.get_actor_location();r=p.get_actor_rotation();look=pc.get_control_rotation()
            gun=next(x for x in p.get_components_by_class(unreal.CameraComponent) if x.get_name()=='GunCamera')
            a=p.get_component_by_class(unreal.SkeletalMeshComponent).get_anim_instance()
            st['rows'].append(dict(t=t,position=[v.x,v.y,v.z],yaw=r.yaw,look=[look.pitch,look.yaw],shots=weapon.shots_fired,
                gun_camera=gun.is_active(),gun_yaw=a.get_editor_property('GunYaw'),gun_pitch=a.get_editor_property('GunPitch'),
                player=p.is_player_controlled(),has_navigation=bool(p.get_component_by_class(unreal.ShipEnemyNavigationComponent))))
        if t>20:
            (root/'Docs/NavalCombat/PLAYER_CONTROLS_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k!='handle'},indent=2))
            unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    except Exception:
        st['error']=traceback.format_exc();(root/'Docs/NavalCombat/PLAYER_CONTROLS_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k!='handle'},indent=2));unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
st['handle']=unreal.register_slate_post_tick_callback(tick);builtins.controls_test=st
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
