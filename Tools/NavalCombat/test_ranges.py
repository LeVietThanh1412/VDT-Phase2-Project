"""Run the existing isolated ballistic/turret fixtures against the six-ship scene."""
import builtins
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
def initialize(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w or unreal.GameplayStatics.get_time_seconds(w)<1:return
    unreal.unregister_slate_post_tick_callback(builtins.range_init_handle)
    ships=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'))
    e=next(s for s in ships if s.get_component_by_class(unreal.ShipWeaponComponent).team()==1)
    p=next(s for s in ships if s.is_player_controlled())
    for s in ships:
        s.can_be_damaged=False
        c=s.get_component_by_class(unreal.ShipCombatComponent);n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent)
        if c:c.enable_combat=s==e;c.enable_hit_and_run=False
        if n:n.stop_navigation()
        if s not in [e,p]:
            h=next(x for x in s.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM');h.set_simulate_physics(False)
            s.set_actor_location(s.get_actor_location()+unreal.Vector(0,0,1000000),False,True)
    for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
        if a.get_class().get_name().startswith('BP_ShipProjectile'):a.destroy_actor()
    source=(root/'Tools/NavalCombat/test_cases.py').read_text()
    source=source.replace('e=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,"/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy_C"))[0]',
        'e=next(s for s in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,"/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy_C")) if s.get_component_by_class(unreal.ShipWeaponComponent).team()==1)')
    source=source.replace('unreal.CollisionResponse.BLOCK','unreal.CollisionResponseType.ECR_BLOCK')
    source=source.replace('dict(name="mechanical_pitch_limit",distance=250,yaw=0,height=450','dict(name="mechanical_pitch_limit",distance=180,yaw=0,height=250')
    source=source.replace("print('COMBAT CASES COMPLETE',", "unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play();print('COMBAT CASES COMPLETE',")
    scope={'OUTPUT':'RANGES_TURRETS_PIE.json','CASES_FILTER':['outside_detection','detected_outside_engagement','left_tracking','right_tracking','barrel_elevation','barrel_lowering','inside_engagement','friendly_rejected','invalid_rejected','island_like_blocker','mechanical_yaw_limit','mechanical_pitch_limit']}
    scope['OUTPUT']=globals().get('OUTPUT',scope['OUTPUT'])
    scope['CASES_FILTER']=globals().get('CASES_FILTER_OVERRIDE',scope['CASES_FILTER'])
    exec(source,scope)
builtins.range_init_handle=unreal.register_slate_post_tick_callback(initialize)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
