"""Real P-menu selection, level travel and dedicated Small player acceptance."""
import builtins,json,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
st=dict(handle=None,status='RUNNING',phase='menu',checks=[],error=None)
builtins.small_selection=st
def save():
    (root/'Docs/NavalCombat/SMALL_BLUEPRINT_SELECTION.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','old_world']},indent=2),encoding='utf-8')
def check(name,passed):
    st['checks'].append(dict(name=name,passed=bool(passed)));assert passed,name
def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=ed.get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    try:
        if st['phase']=='menu' and t>3:
            pc=unreal.GameplayStatics.get_player_controller(w,0);pc.toggle_match_setup();ui=pc.setup_widget
            ui.blue_count_selector.set_selected_index(2);ui.red_count_selector.set_selected_index(2)
            for selector,index in zip(ui.hull_selectors,[0,2,0,0,1,2]):selector.set_selected_index(index)
            ui.mode_selector.set_selected_index(0);st.update(old_world=w,phase='travel');ui.start_match();save();return
        if st['phase']=='travel' and w!=st['old_world'] and t>3:
            p=unreal.GameplayStatics.get_player_pawn(w,0);pc=p.get_controller()
            fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
            check('exact six ships / one player',len(fleet)==6 and sum(a.is_player_controlled() for a in fleet)==1)
            check('Small selection spawns dedicated BP_ShipSmall',p.get_class().get_name()=='BP_ShipSmall_C')
            check('original CustomPlayerController retained',isinstance(pc,unreal.CustomPlayerController))
            weapon=p.get_component_by_class(unreal.ShipWeaponComponent)
            check('selected Small 38 HP / 6 damage',weapon.current_health==38 and weapon.max_health==38 and weapon.base_projectile_damage==6)
            parts={c.get_name():c for c in p.get_components_by_class(unreal.PrimitiveComponent)}
            check('authored parts reused without duplicates',all(sum(c.get_name().startswith(name) for c in p.get_components_by_class(unreal.PrimitiveComponent))==1 for name in ['SmallBoatVisual','SmallGunYaw','SmallGunPitch']))
            check('single authoritative physics body',sum(c.is_simulating_physics() for c in parts.values())==1)
            low,high=parts['ShipSM'].get_local_bounds();check('66 m compact hull',abs(high.x-low.x-6600)<1)
            check('imported source visible',parts['SmallBoatVisual'].is_visible() and parts['SmallBoatVisual'].static_mesh.get_path_name().startswith('/Game/ShipSmall/Asset/sweedenmilitaryboat'))
            st['roster']=[dict(label=a.get_actor_label(),pawn_class=a.get_class().get_name(),team=a.get_component_by_class(unreal.ShipWeaponComponent).team(),hull=str(a.get_component_by_class(unreal.ShipMovementComponent).hull_class),max_hp=a.get_component_by_class(unreal.ShipWeaponComponent).max_health,controller=a.get_controller().get_class().get_name() if a.get_controller() else None) for a in fleet]
            # Disposable showcase observation; prevent the known invalid old
            # saved patrol placements from obscuring Small selection testing.
            for a in fleet:
                c=a.get_component_by_class(unreal.ShipCombatComponent);n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                a.can_be_damaged=False
                if c:c.enable_combat=False
                if n:n.stop_navigation()
            st['phase']='observe';save()
        if st['phase']=='observe' and t>8:
            p=unreal.GameplayStatics.get_player_pawn(w,0);pos=p.get_actor_location();side=p.get_actor_right_vector()*12000+p.get_actor_forward_vector()*1000+unreal.Vector(0,0,5000)
            transform=unreal.Transform(location=pos+side,rotation=unreal.MathLibrary.find_look_at_rotation(pos+side,pos+unreal.Vector(0,0,700)))
            gameplay=unreal.get_default_object(unreal.GameplayStatics.static_class())
            camera=gameplay.call_method('BeginDeferredActorSpawnFromClass',args=(w,unreal.CameraActor.static_class(),transform,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
            gameplay.call_method('FinishSpawningActor',args=(camera,transform,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
            unreal.GameplayStatics.get_player_controller(w,0).set_view_target_with_blend(camera,0)
            st['phase']='capture'
        if st['phase']=='capture' and t>9:
            unreal.SystemLibrary.execute_console_command(w,'Shot showui filename='+str(root/'Docs/NavalCombat/SMALL_BLUEPRINT_SHOWCASE.png'))
            st['phase']='done'
        if st['phase']=='done' and t>10:finish('PASSED')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick);save()
if ed.get_game_world() is None:unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
