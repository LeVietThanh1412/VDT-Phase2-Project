"""Native setup UI, actual level travel, roster/profile/skill/mode and fresh PIE.

Invokes the same handlers bound to menu buttons. Physical mouse/key input and
visual layout are separate checks. Runs five selected matches without asset edits.
"""
import builtins,json,time,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
cases=[(1,1,0,1,[1,0,0,0,0,0]),(2,2,2,0,[0,1,0,1,0,0]),(3,3,1,0,[1,0,1,0,1,0]),(2,3,0,1,[1,0,0,0,1,0]),(3,1,2,0,[0,1,0,1,0,0])]
st=dict(handle=None,phase='default',checks=[],matches=[],case=0,error=None,wall_started=time.monotonic())
builtins.naval_menu_test=st
def check(name,value):st['checks'].append(dict(name=name,passed=bool(value)));assert value,name
def hp(a):return a.get_component_by_class(unreal.ShipWeaponComponent)
def subsystem(w,kind):
    lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
    return lib.call_method('GetGameInstanceSubsystem' if kind==unreal.NavalMatchSettings else 'GetWorldSubsystem',args=(w,kind.static_class()))
def ships(w):return [a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if hp(a)]
def save():
    skip=['handle','ui','pc','old_world']
    (root/'Docs/NavalCombat/PREMATCH_MENU_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in skip},indent=2),encoding='utf-8')
def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def prepare(w):
    pc=unreal.GameplayStatics.get_player_controller(w,0);pc.toggle_match_setup();ui=pc.setup_widget
    check('setup widget opens',ui is not None and ui.is_in_viewport())
    blue,red,skill,mode,classes=cases[st['case']]
    ui.blue_count_selector.set_selected_index(blue-1);ui.red_count_selector.set_selected_index(red-1)
    ui.difficulty_selector.set_selected_index(skill);ui.mode_selector.set_selected_index(mode)
    for selector,index in zip(ui.hull_selectors,classes):selector.set_selected_index(index)
    ui.refresh_slots()
    check('unused class selectors disabled',all(c.get_is_enabled()==(i%3<(blue if i<3 else red)) for i,c in enumerate(ui.hull_selectors)))
    st.update(phase='travel',old_world=w,at=time.monotonic());ui.start_match()
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    try:
        if st['phase']=='restart_defaults' and w is None:
            st['phase']='fresh';unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play();return
        if not w:return
        t=unreal.GameplayStatics.get_time_seconds(w)
        if st['phase']=='default' and t>=3:
            settings=subsystem(w,unreal.NavalMatchSettings);battle=subsystem(w,unreal.NavalBattleSubsystem)
            check('direct PIE development defaults',not settings.explicit_selection and battle.match_configured and battle.blue_alive==3 and battle.red_alive==3 and battle.battle_mode==unreal.NavalBattleMode.REAL_TIME)
            pc=unreal.GameplayStatics.get_player_controller(w,0);check('direct PIE menu closed',pc.setup_widget is None)
            pc.toggle_match_setup();ui=pc.setup_widget;check('menu defaults 3v3 Normal RealTime',ui.blue_count_selector.get_selected_index()==2 and ui.red_count_selector.get_selected_index()==2 and ui.difficulty_selector.get_selected_index()==1 and ui.mode_selector.get_selected_index()==0)
            ui.blue_count_selector.set_selected_index(0)
            unreal.SystemLibrary.execute_console_command(w,'Shot showui filename='+str(root/'Docs/NavalCombat/PREMATCH_DEFAULT.png'))
            st.update(phase='cancel',pc=pc,ui=ui,paused_time=t,at=time.monotonic())
        elif st['phase']=='cancel' and time.monotonic()-st['at']>.5:
            check('configuration menu pauses match',abs(t-st['paused_time'])<.1)
            st['ui'].cancel();check('cancel closes menu',st['pc'].setup_widget is None)
            settings=subsystem(w,unreal.NavalMatchSettings);check('cancel preserves settings',not settings.explicit_selection and settings.blue_count==3)
            st.update(phase='after_cancel',at=time.monotonic())
        elif st['phase']=='after_cancel' and time.monotonic()-st['at']>.3:
            check('cancel resumes match',t>st['paused_time']+.1);prepare(w)
        elif st['phase']=='travel':
            if w==st['old_world'] or t<3:return
            blue,red,skill,mode,classes=cases[st['case']]
            battle=subsystem(w,unreal.NavalBattleSubsystem);settings=subsystem(w,unreal.NavalMatchSettings)
            check('selected match configured after actual travel',battle.match_configured and battle.blue_alive==blue and battle.red_alive==red)
            fleet=ships(w);player=unreal.GameplayStatics.get_player_pawn(w,0)
            check('exact selected ship count',len(fleet)==blue+red)
            check('exactly one Blue player',sum(a.is_player_controlled() for a in fleet)==1 and hp(player).team()==0)
            controllers=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.AIController)
            check('no duplicate AI controllers',len(controllers)==blue+red-1 and all(a.get_controller() in controllers for a in fleet if a!=player))
            teams=[[player]+sorted([a for a in fleet if hp(a).team()==0 and a!=player],key=lambda a:a.get_name()),sorted([a for a in fleet if hp(a).team()==1],key=lambda a:a.get_name())]
            for team,members in enumerate(teams):
                for i,a in enumerate(members):
                    movement=a.get_component_by_class(unreal.ShipMovementComponent)
                    check('selected per-slot physics class',movement.hull_class==[unreal.ShipHullClass.SMALL,unreal.ShipHullClass.LARGE][classes[team*3+i]])
                    combat=a.get_component_by_class(unreal.ShipCombatComponent)
                    if combat:check('selected AI shooting preset',combat.shooting_skill==[unreal.ShipAISkill.EASY,unreal.ShipAISkill.NORMAL,unreal.ShipAISkill.HARD][skill])
                    check('100 HP maximum retained',hp(a).max_health==100)
                    if a!=player:
                        nav=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                        check('existing patrol references retained',nav.patrol_actor_a is not None and nav.patrol_actor_b is not None)
            check('selected mode retained',battle.battle_mode==[unreal.NavalBattleMode.REAL_TIME,unreal.NavalBattleMode.TURN_BASED][mode])
            check('settings persist through level travel',settings.explicit_selection and settings.blue_count==blue and settings.red_count==red)
            if mode:check('preselected turn starts with player only',battle.active_turn_ship==player and sum(battle.is_ship_active(a) for a in fleet)==1)
            else:check('selected real time permits every ship',all(battle.is_ship_active(a) for a in fleet))
            st['matches'].append(dict(blue=blue,red=red,skill=skill,mode=mode,classes=classes,ships=[a.get_name() for a in fleet],ai_controllers=len(controllers)))
            save();st['case']+=1
            if st['case']<len(cases):prepare(w)
            else:st['phase']='restart_defaults';unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
        elif st['phase']=='fresh' and t>=3:
            settings=subsystem(w,unreal.NavalMatchSettings);battle=subsystem(w,unreal.NavalBattleSubsystem)
            check('fresh PIE resets to unselected development defaults',not settings.explicit_selection and settings.blue_count==3 and settings.red_count==3 and battle.blue_alive==3 and battle.red_alive==3 and battle.battle_mode==unreal.NavalBattleMode.REAL_TIME)
            finish('PASSED')
        if time.monotonic()-st['wall_started']>150:raise AssertionError('prematch fixture timeout')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
