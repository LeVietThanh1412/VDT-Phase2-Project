"""Real saved-map menu/travel integration; no ship pose or movement injection.

Button handler calls drive the matrix; desktop interaction/visual evidence is
recorded separately. Damage is disabled only to inspect each selected roster.
"""
import unreal,builtins,json,time,traceback,hashlib
from pathlib import Path
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
cases=globals().get('CASES',[
 dict(name='manual_player',manual=True,full=False,blue=3,red=3,classes=[2]*6),
 dict(name='manual_full',manual=True,full=True,blue=3,red=3,classes=[2]*6),
 dict(name='seed_full',manual=False,full=True,blue=3,red=3,classes=[0,0,0,0,1,2]),
 dict(name='seed_red_player',manual=False,full=False,team=1,blue=3,red=3,classes=[0,0,0,0,1,2]),
 dict(name='manual_1v1_turns',manual=True,full=False,blue=1,red=1,mode=1,skill=0,classes=[2]*6),
 dict(name='manual_2v2_full_turns',manual=True,full=True,blue=2,red=2,mode=1,skill=2,classes=[2]*6),
 dict(name='seed_asymmetric',manual=False,full=True,blue=2,red=3,skill=0,classes=[0,0,0,0,1,2]),
 dict(name='manual_red_small',manual=True,full=False,team=1,blue=3,red=1,skill=2,classes=[2,2,2,0,2,2]),
 dict(name='seed_zero_1v1',manual=False,full=True,blue=1,red=1,seed=0,classes=[0]*6),
])
name=globals().get('OUTPUT','MATCH_UI_INTEGRATION.json');s=dict(status='RUNNING',phase='menu',case=0,results=[],error=None,started=time.monotonic(),native_dll_sha256=hashlib.sha256((root/'Binaries/Win64/UnrealEditor-VDTPhase2Project.dll').read_bytes()).hexdigest());builtins.match_ui_probe=s
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
def sub(w,c):return lib.call_method('GetWorldSubsystem',args=(w,c.static_class()))
def save():
 p=root/'Saved'/name;t=p.with_suffix('.tmp');t.write_text(json.dumps({k:v for k,v in s.items() if k not in ('handle','old_world','settings')},indent=2));t.replace(p)
def finish(status):
 s['status']=status;save();unreal.unregister_slate_post_tick_callback(s['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
 try:
  assert time.monotonic()-s['started']<1400,'UI matrix timeout'
  w=ed.get_game_world()
  if not w:return
  t=unreal.GameplayStatics.get_time_seconds(w);pc=unreal.GameplayStatics.get_player_controller(w,0);case=cases[s['case']]
  fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
  for a in fleet:a.can_be_damaged=False
  if s['phase']=='menu':
   if time.monotonic()-s.get('menu_at',s['started'])<1:return
   if not pc.setup_widget:pc.toggle_match_setup()
   ui=pc.setup_widget
   assert len(unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.ShipSetupWidget,False))==1,'Duplicate setup menus'
   if s['case']==0:
    ui.blue_count_selector.set_selected_index(0);ui.call_method('AddBlueShip');assert ui.blue_count_selector.get_selected_index()==1
    ui.call_method('AddBlueShip');ui.call_method('AddBlueShip');assert ui.blue_count_selector.get_selected_index()==2
    for c,i in zip(ui.hull_selectors[:3],[0,1,2]):c.set_selected_index(i)
    ui.call_method('RemoveBlueShip1');assert ui.blue_count_selector.get_selected_index()==1 and ui.hull_selectors[1].get_selected_index()==2
    ui.red_count_selector.set_selected_index(0);ui.call_method('RemoveRedShip0');assert ui.red_count_selector.get_selected_index()==0
    ui.call_method('AddRedShip');ui.call_method('RemoveRedShip1');assert ui.red_count_selector.get_selected_index()==0
    s['add_remove_limits_and_class_shift']=True
   ui.blue_count_selector.set_selected_index(case['blue']-1);ui.red_count_selector.set_selected_index(case['red']-1)
   ui.control_selector.set_selected_index(int(case['full']));ui.player_team_selector.set_selected_index(case.get('team',0));ui.mode_selector.set_selected_index(case.get('mode',0));ui.difficulty_selector.set_selected_index(case.get('skill',1));ui.balance_selector.set_selected_index(0)
   for c,i in zip(ui.hull_selectors,case['classes']):c.set_selected_index(i)
   ui.use_my_map() if case['manual'] else ui.generate_from_seed()
   ui.seed_input.set_text('-1');assert ui.apply_selection()==case['manual'];ui.seed_input.set_text(str(case.get('seed',20261028)));assert ui.apply_selection();ui.refresh_slots()
   captured=unreal.ShipCombatLibrary.capture_gameplay_hud(w,str(root/'Saved'/('UI_'+case['name']+'_MENU.png')))
   s['menu_capture']=captured;s.update(phase='travel',old_world=w,travel_at=time.monotonic());ui.start_match();save();return
  if s['phase']=='travel':
   assert time.monotonic()-s['travel_at']<90,'Level travel/loading stuck'
   if w==s['old_world']:return
   f=sub(w,unreal.NavalBattlefieldSubsystem);b=sub(w,unreal.NavalBattleSubsystem)
   assert not f.initialization_error,f.initialization_error
   assert not (not b.match_configured and b.configuration_error and not b.configuration_error.startswith('MY MAP:')),b.configuration_error
   if not f.ready:return
   assert b.match_configured and b.blue_alive==case['blue'] and b.red_alive==case['red'],'Roster counters do not match menu'
   assert len(fleet)==case['blue']+case['red'],'Duplicate/missing ship'
   controls=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.AIController)
   assert len(controls)==len(fleet)-(not case['full']),'AI controller count mismatch'
   assert sum(a.is_player_controlled() for a in fleet)==(not case['full'])
   assert b.battle_mode==[unreal.NavalBattleMode.REAL_TIME,unreal.NavalBattleMode.TURN_BASED][case.get('mode',0)]
   assert f.using_authored_layout==case['manual']
   assert f.actual_seed>0 and (case.get('seed',20261028)==0 or f.actual_seed==case.get('seed',20261028))
   assert not pc.setup_widget
   settings=lib.call_method('GetGameInstanceSubsystem',args=(unreal.GameplayStatics.get_game_instance(w),unreal.NavalMatchSettings.static_class()))
   assert list(settings.hull_classes)==[[unreal.ShipHullClass.SMALL,unreal.ShipHullClass.LARGE,unreal.ShipHullClass.MEDIUM][i] for i in case['classes']]
   for a in fleet:
    c=a.get_component_by_class(unreal.ShipCombatComponent)
    if c:assert c.shooting_skill==[unreal.ShipAISkill.EASY,unreal.ShipAISkill.NORMAL,unreal.ShipAISkill.HARD][case.get('skill',1)]
   if not case['full']:
    player=unreal.GameplayStatics.get_player_pawn(w,0);assert player.get_component_by_class(unreal.ShipWeaponComponent).team()==case.get('team',0)
   s.update(phase='observe',ready_at=t,controllers={a.get_name():a.get_controller().get_name() for a in fleet});save();return
  if s['phase']=='observe':
   if t-s['ready_at']<15:return
   assert not any(u.is_in_viewport() for u in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.LoadingScreenWidget,False)),'Loading overlay never released gameplay'
   assert not pc.is_move_input_ignored() and not pc.is_look_input_ignored(),'Loading input locks retained'
   assert all(a.get_controller() and a.get_controller().get_name()==s['controllers'][a.get_name()] for a in fleet)
   if case['full']:
    pc.call_method('CycleShipCamera');pc.show_battlefield_overview();assert all(a.get_controller().get_name()==s['controllers'][a.get_name()] for a in fleet)
   s['results'].append(dict(case=case,status='PASSED',ready_at=s['ready_at'],actual_seed=sub(w,unreal.NavalBattlefieldSubsystem).actual_seed,ships=len(fleet),ai_controllers=len(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.AIController)),capture=unreal.ShipCombatLibrary.capture_gameplay_hud(w,str(root/'Saved'/('UI_'+case['name']+'_PLAY.png')))))
   s['case']+=1;save()
   if s['case']==len(cases):finish('PASSED')
   else:s.update(phase='menu',menu_at=time.monotonic())
 except Exception:s['error']=traceback.format_exc();finish('FAILED')
s['handle']=unreal.register_slate_post_tick_callback(tick);save();unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
