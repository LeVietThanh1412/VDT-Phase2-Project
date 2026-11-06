"""Controlled result/menu regression; lethal fixture damage is not a natural match.
WAIT_FOR_CLICK leaves the result card visible for a real desktop click.
"""
import unreal,builtins,json,time,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());full_ai=globals().get('FULL_AI',True);loser=globals().get('LOSING_TEAM',1);player_team=globals().get('PLAYER_TEAM',0)
name=globals().get('OUTPUT',f'RETURN_TO_SETUP_{int(full_ai)}_{loser}_{player_team}.json')
p=root/'Tools/Testing/verify_final_demo.py';ctx=dict(KIND='patrol',FULL_AI=full_ai,PLAYER_TEAM=player_team,OBSERVE_SECONDS=99999,OUTPUT='RETURN_STARTUP.json',CAPTURE=False)
exec(compile(p.read_text(encoding='utf-8'),str(p),'exec'),ctx);base=ctx['st']
s=dict(status='RUNNING',phase='startup',full_ai=full_ai,losing_team=loser,player_team=player_team,checks={},error=None,started=time.monotonic(),native_dll_sha256=base['native_dll_sha256'],production_sources_sha256=base['production_sources_sha256']);builtins.return_setup_probe=s
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
def sub(w,c):return lib.call_method('GetWorldSubsystem',args=(w,c.static_class()))
def save():(root/'Saved'/name).write_text(json.dumps({k:v for k,v in s.items() if k not in ['handle','old_world']},indent=2),encoding='utf-8')
def finish(status):
 s['status']=status;save();unreal.unregister_slate_post_tick_callback(s['handle'])
 if base['status']=='RUNNING':unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_RESULT_FIXTURE'
 if status=='PASSED' or not globals().get('HOLD_FAILURE',False):unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
 try:
  assert time.monotonic()-s['started']<300,'Result fixture wall-time limit'
  w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  if not w:return
  pc=unreal.GameplayStatics.get_player_controller(w,0);t=unreal.GameplayStatics.get_time_seconds(w)
  fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
  for a in fleet:
   a.can_be_damaged=False
   if c:=a.get_component_by_class(unreal.ShipCombatComponent):c.enable_combat=False
  if s['phase']=='startup':
   if base['status']=='FAILED':raise AssertionError(base.get('error'))
   if globals().get('EARLY_RESULT',False):
    if base['phase']!='initialization' or w==base.get('old_world'):return
    if not sub(w,unreal.NavalBattleSubsystem).match_configured or len(fleet)!=6:return
   elif base['phase']!='observe':return
   unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_RESULT_FIXTURE'
   pc.return_to_match_setup();assert not pc.setup_widget,'Return opened setup before a match result'
   s['checks']['inactive_before_result']=True
   s['initial_player_ship']=next((a.get_name() for a in fleet if a.is_player_controlled()),None)
   if globals().get('HIDE_HUD',False):
    for hud in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.ShipDetailsUI,False):hud.set_visibility(unreal.SlateVisibility.COLLAPSED)
   source=next(a for a in fleet if a.get_component_by_class(unreal.ShipWeaponComponent).team()!=loser)
   s['loading_before_outcome']=any(u.is_in_viewport() for u in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.LoadingScreenWidget,False))
   for a in fleet:
    wp=a.get_component_by_class(unreal.ShipWeaponComponent)
    if wp.team()==loser:a.can_be_damaged=True;assert wp.receive_ship_damage(1000,source)
   battle=sub(w,unreal.NavalBattleSubsystem);expected=('BLUE WINS' if loser==1 else 'RED WINS') if full_ai else ('VICTORY' if loser!=player_team else 'DEFEAT')
   assert battle.outcome==expected,(battle.outcome,expected)
   s['cursor_when_outcome_set']=pc.show_mouse_cursor
   s['loading_when_outcome_set']=any(u.is_in_viewport() for u in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.LoadingScreenWidget,False))
   if globals().get('EARLY_RESULT',False):assert s['loading_before_outcome'],'Early-result fault missed the loading interval'
   s.update(outcome=battle.outcome,phase='result',outcome_at=t);save();return
  if s['phase']=='result':
   # Player defeat also exercises HUD operation after the selected wreck cleans up.
   if t-s['outcome_at']<float(globals().get('RESULT_HOLD_SECONDS',1)):return
   if any(u.is_in_viewport() for u in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.LoadingScreenWidget,False)):
    assert t-s['outcome_at']<25,'Loading overlay obscured the finished match result'
    return
   if not full_ai and loser==player_team and any(a.get_name()==s['initial_player_ship'] for a in fleet):
    assert t-s['outcome_at']<120,'Player wreck did not complete its native cleanup'
    return
   # In a hidden/occluded Editor Slate may not tick the HUD until it paints.
   # Render the actual viewport before inspecting the displayed result text.
   if not s.get('result_rendered'):
    s.setdefault('render_started',time.monotonic())
    if time.monotonic()<s.get('render_next_at',0):return
    rendered=unreal.ShipCombatLibrary.capture_gameplay_hud(w,str(root/'Saved'/name.replace('.json','_RESULT.png')))
    s['render_next_at']=time.monotonic()+.5
    if rendered.startswith('ERROR:'):
     s['render_error']=rendered;save()
     assert time.monotonic()-s['render_started']<10,'Editor viewport could not paint the result: '+rendered
     return
    s['result_rendered']=rendered
    return
   summaries=[u for u in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.MatchSummaryWidget,False) if u.is_in_viewport()]
   assert len(summaries)==1,'Latest teammate summary missing or duplicated'
   assert pc.show_mouse_cursor,'Result did not release the mouse cursor'
   battle=sub(w,unreal.NavalBattleSubsystem)
   assert battle.outcome==s['outcome'] and (battle.blue_alive==0 if loser==0 else battle.red_alive==0),'Global result/counters stale after wreck cleanup'
   s['checks']['global_result_and_counters_current']=True
   s['checks']['teammate_summary_interactive']=True
   if not full_ai and loser==player_team:
    # The controller may select its free-camera pawn after the ship is removed.
    s['checks']['works_after_selected_ship_cleanup']=not any(a.get_name()==s['initial_player_ship'] for a in fleet)
   unreal.ShipCombatLibrary.capture_gameplay_hud(w,str(root/'Saved'/name.replace('.json','_RESULT.png')))
   s.update(phase='click',click_method='desktop mouse' if globals().get('WAIT_FOR_CLICK',False) else 'same controller action bound by result button');save();return
  if s['phase']=='click':
   if not pc.setup_widget:
    if globals().get('WAIT_FOR_CLICK',False):return
    pc.return_to_match_setup()
   menu=pc.setup_widget;assert menu
   assert unreal.GameplayStatics.is_game_paused(w) and pc.show_mouse_cursor
   assert menu.blue_count_selector.get_selected_index()==2 and menu.red_count_selector.get_selected_index()==2
   assert menu.control_selector.get_selected_index()==(1 if full_ai else 0) and menu.player_team_selector.get_selected_index()==player_team
   assert str(menu.seed_input.get_text())=='20261028'
   pc.return_to_match_setup();assert pc.setup_widget==menu,'Repeated return closed the setup menu'
   s['checks'].update(setup_opened=True,paused_and_cursor_visible=True,selections_preserved=True,idempotent=True)
   pc.close_match_setup();assert not pc.setup_widget and pc.show_mouse_cursor and not unreal.GameplayStatics.is_game_paused(w),'Back to result lost the clickable cursor'
   pc.return_to_match_setup();menu=pc.setup_widget;assert menu and unreal.GameplayStatics.is_game_paused(w)
   s['checks']['return_available_after_back_to_result']=True
   unreal.ShipCombatLibrary.capture_gameplay_hud(w,str(root/'Saved'/name.replace('.json','_MENU.png')))
   s.update(phase='restart',old_world=w,restart_at=time.monotonic());menu.start_match();save();return
  if s['phase']=='restart' and w!=s['old_world']:
   field=sub(w,unreal.NavalBattlefieldSubsystem);assert not field.initialization_error,field.initialization_error
   if not field.ready:return
   battle=sub(w,unreal.NavalBattleSubsystem)
   assert not battle.outcome and battle.blue_alive==3 and battle.red_alive==3
   assert len(fleet)==6 and not pc.setup_widget and not unreal.GameplayStatics.is_game_paused(w)
   assert sum(a.is_player_controlled() for a in fleet)==(0 if full_ai else 1)
   s['checks'].update(new_match_resets_result=True,six_ship_roster_restored=True)
   assert all(s['checks'].values()),s['checks'];finish('PASSED')
 except Exception:s['error']=traceback.format_exc();finish('FAILED')
s['handle']=unreal.register_slate_post_tick_callback(tick);save()
