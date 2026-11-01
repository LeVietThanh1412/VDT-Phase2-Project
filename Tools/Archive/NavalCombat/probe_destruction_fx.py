"""Temporary FX inspection world. Stop PIE after inspection; never save fixtures."""
import unreal,builtins
st=dict(handle=None)
def setup(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w or unreal.GameplayStatics.get_time_seconds(w)<1:return
    unreal.unregister_slate_post_tick_callback(st['handle'])
    ships=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'))
    for s in ships:
        c=s.get_component_by_class(unreal.ShipCombatComponent);n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent)
        if c:c.enable_combat=False
        if n:n.stop_navigation()
    target=next(s for s in ships if s.get_actor_label()=='BP_ShipEnemy');source=next(s for s in ships if s.is_player_controlled())
    weapon=target.get_component_by_class(unreal.ShipWeaponComponent);weapon.burning_seconds=86400;weapon.receive_ship_damage(100,source)
    builtins.vfx_probe=weapon
    pos=target.get_actor_location();eye=pos+unreal.Vector(20000,20000,12000)
    transform=unreal.Transform(location=eye,rotation=unreal.MathLibrary.find_look_at_rotation(eye,pos))
    gameplay=unreal.get_default_object(unreal.GameplayStatics.static_class())
    camera=gameplay.call_method('BeginDeferredActorSpawnFromClass',args=(w,unreal.CameraActor.static_class(),transform,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    gameplay.call_method('FinishSpawningActor',args=(camera,transform,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    unreal.GameplayStatics.get_player_controller(w,0).set_view_target_with_blend(camera,0)
st['handle']=unreal.register_slate_post_tick_callback(setup)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
