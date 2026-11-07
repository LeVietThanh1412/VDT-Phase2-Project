"""Fault injection: remove one transient PIE ship during preparation, then retry.

No saved asset or pose is edited. The production menu, failure guard and travel
must recover without a dangling preparation actor or a leftover loading overlay.
"""
import unreal,builtins,json,time,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
p=root/'Tools/Testing/verify_final_demo.py';ctx=dict(KIND='patrol',FULL_AI=True,HULL_CLASSES=[2]*6,OBSERVE_SECONDS=99999,CAPTURE=False,OUTPUT='ACTOR_LOSS_STARTUP.json')
exec(compile(p.read_text(),str(p),'exec'),ctx);base=builtins.final_demo_probe
s=dict(status='RUNNING',phase='startup',started=time.monotonic(),checks={},error=None,native_dll_sha256=base['native_dll_sha256']);builtins.preparation_loss_probe=s
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
sub=lambda w,c:lib.call_method('GetWorldSubsystem',args=(w,c.static_class()))

def save():
 (root/'Saved/PREPARATION_ACTOR_LOSS.json').write_text(json.dumps({k:v for k,v in s.items() if k not in ['handle','old_world']},indent=2))

def finish(status):
 s['status']=status;save();unreal.unregister_slate_post_tick_callback(s['handle'])
 if base['status']=='RUNNING':unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_ACTOR_LOSS'
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()

def tick(dt):
 try:
  assert time.monotonic()-s['started']<160,'Actor-loss recovery timeout'
  w=ed.get_game_world()
  if not w:return
  pc=unreal.GameplayStatics.get_player_controller(w,0);battle=sub(w,unreal.NavalBattleSubsystem);field=sub(w,unreal.NavalBattlefieldSubsystem)
  fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
  if s['phase']=='startup':
   assert base['status']!='FAILED',base.get('error')
   if base['phase']!='initialization' or w==base.get('old_world') or not battle.match_configured or len(fleet)!=6:return
   assert not field.ready and not battle.outcome
   unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_ACTOR_LOSS'
   victim=next(a for a in fleet if a.get_component_by_class(unreal.ShipWeaponComponent).team()==1)
   s['victim']=victim.get_name();victim.destroy_actor();s['phase']='error';save();return
  if s['phase']=='error':
   if not pc.setup_widget:return
   assert 'removed or destroyed during battlefield preparation' in field.initialization_error,field.initialization_error
   assert not field.ready and not battle.outcome and unreal.GameplayStatics.is_game_paused(w) and pc.show_mouse_cursor
   assert not any(x.is_in_viewport() for x in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.LoadingScreenWidget,False))
   s['checks'].update(named_error=True,menu_and_cursor=True,loading_removed=True)
   s['capture']=unreal.ShipCombatLibrary.capture_gameplay_hud(w,str(root/'Saved/PREPARATION_ACTOR_LOSS.png'))
   s.update(phase='restart',old_world=w);pc.setup_widget.start_match();save();return
  if s['phase']=='restart' and w!=s['old_world']:
   assert not field.initialization_error,field.initialization_error
   if not field.ready:return
   assert len(fleet)==6 and battle.blue_alive==3 and battle.red_alive==3 and not battle.outcome
   assert len(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.AIController))==6 and not pc.setup_widget
   s['checks']['retry_restores_roster']=True;finish('PASSED')
 except Exception:s['error']=traceback.format_exc();finish('FAILED')
s['handle']=unreal.register_slate_post_tick_callback(tick);save()
