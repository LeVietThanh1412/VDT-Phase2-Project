"""Controlled-damage real PIE check of player death and defeat; no assets changed."""
import builtins,json,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());st=dict(handle=None,stage=0,rows=[],error=None,at=2)
def hp(s):return s.get_component_by_class(unreal.ShipWeaponComponent)
def hud(w):
    h=unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.ShipHUDWidget,False)[0]
    return dict(score=h.score_text,hp=h.health_text,result=h.result_text)
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<st['at']:return
    try:
        ships=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'))
        if st['stage']==0:
            p=next(s for s in ships if s.is_player_controlled());enemy=next(s for s in ships if hp(s).team()==1)
            for s in ships:
                c=s.get_component_by_class(unreal.ShipCombatComponent)
                if c:c.enable_combat=False
            hp(p).receive_ship_damage(100,enemy);p.get_controller().set_view_target_with_blend(p,0)
            st['rows'].append(dict(name='player_death',hud=hud(w),death_events=hp(p).destruction_events,
                can_fire=hp(p).try_fire_weapon(unreal.Vector(),None,unreal.Vector(),False),tick=p.is_actor_tick_enabled(),
                move_ignored=p.get_controller().is_move_input_ignored(),look_ignored=p.get_controller().is_look_input_ignored()))
            unreal.SystemLibrary.execute_console_command(w,'Shot showui filename='+str(root/'Docs/NavalCombat/UI_PLAYER_DESTROYED.png'))
        elif st['stage']==1:
            enemy=next(s for s in ships if hp(s).team()==1)
            for s in ships:
                if hp(s).team()==0:hp(s).receive_ship_damage(100,enemy)
            for s in ships:
                c=s.get_component_by_class(unreal.ShipCombatComponent)
                if c:c.scan_targets()
            st['rows'].append(dict(name='defeat',hud=hud(w),players=sum(s.is_player_controlled() for s in ships),
                valid_targets=[s.get_component_by_class(unreal.ShipCombatComponent).target.get_actor_label() if s.get_component_by_class(unreal.ShipCombatComponent).target else None for s in ships if s.get_component_by_class(unreal.ShipCombatComponent)]))
            unreal.SystemLibrary.execute_console_command(w,'Shot showui filename='+str(root/'Docs/NavalCombat/UI_DEFEAT.png'))
        else:
            (root/'Docs/NavalCombat/DEFEAT_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k!='handle'},indent=2))
            unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play();return
        st['stage']+=1;st['at']=t+3
    except Exception:
        st['error']=traceback.format_exc();(root/'Docs/NavalCombat/DEFEAT_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k!='handle'},indent=2));unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
st['handle']=unreal.register_slate_post_tick_callback(tick);builtins.defeat_test=st
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
