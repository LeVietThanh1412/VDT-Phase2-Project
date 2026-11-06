"""PIE-only invalid-faction fault verifies the real loading watchdog/retry UI.

The fixture never saves the fault, and does not count this as a playable match.
"""
import unreal,builtins,json,time,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
p=root/'Tools/Testing/verify_authored_patrol.py'
exec(compile(p.read_text(),str(p),'exec'),dict(FULL_AI=True,REQUIRED_LEGS=9999,OUTPUT='UI_TIMEOUT_STARTUP.json'))
base=builtins.authored_patrol_probe;s=dict(status='RUNNING',phase='startup',started=time.monotonic(),error=None);builtins.loading_timeout_probe=s
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
def battle(w):return lib.call_method('GetWorldSubsystem',args=(w,unreal.NavalBattleSubsystem.static_class()))
def save():(root/'Saved/UI_LOADING_TIMEOUT.json').write_text(json.dumps({k:v for k,v in s.items() if k!='handle'},indent=2))
def finish(status):
 s['status']=status;save();unreal.unregister_slate_post_tick_callback(s['handle'])
 if base['status']=='RUNNING':unreal.unregister_slate_post_tick_callback(base['handle'])
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
 try:
  assert time.monotonic()-s['started']<130,'Loading watchdog fixture exceeded its wall bound'
  w=ed.get_game_world()
  if not w:return
  if s['phase']=='startup':
   if base['phase']!='startup' or w==base['old_world']:return
   b=battle(w);assert not b.match_configured,'Fault injected too late'
   unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_LOADING_FAULT'
   ships=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipEnemyNavigationComponent)]
   ships[0].get_component_by_class(unreal.ShipWeaponComponent).faction=99
   ships[0].get_component_by_class(unreal.ShipCombatComponent).faction=99
   s.update(phase='wait',fault_at=time.monotonic());save();return
  pc=unreal.GameplayStatics.get_player_controller(w,0);b=battle(w)
  if int(time.monotonic()-s['fault_at'])//5!=s.get('saved'):
   s['saved']=int(time.monotonic()-s['fault_at'])//5
   s['last_state']=dict(configured=b.match_configured,error=b.configuration_error,paused=unreal.GameplayStatics.is_game_paused(w),menu=bool(pc.setup_widget))
   save()
  if not pc.setup_widget:return
  assert not b.match_configured and 'timed out' in b.configuration_error,b.configuration_error
  elapsed=time.monotonic()-s['fault_at'];assert 88<elapsed<120,elapsed
  assert unreal.GameplayStatics.is_game_paused(w) and pc.show_mouse_cursor
  assert not any(u.is_in_viewport() for u in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.LoadingScreenWidget,False))
  s.update(elapsed_seconds=elapsed,error_shown=b.configuration_error,capture=unreal.ShipCombatLibrary.capture_gameplay_hud(w,str(root/'Saved/UI_LOADING_TIMEOUT.png')))
  finish('PASSED')
 except Exception:s['error']=traceback.format_exc();finish('FAILED')
s['handle']=unreal.register_slate_post_tick_callback(tick);save()
