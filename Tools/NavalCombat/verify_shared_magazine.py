"""Actual original BP projectiles on six physical ships; no ammo or timer writes."""
import builtins,json,time,traceback,math
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
scope=dict(SEED=20261028,OBSERVE_SECONDS=99999,WALL_LIMIT_SECONDS=900,OUTPUT='MAGAZINE_STARTUP.json',OUTPUT_DIR='Saved',__name__='magazine_start')
p=root/'Tools/NavalCombat/test_seeded_spawn.py';exec(compile(p.read_text(encoding='utf-8'),str(p),'exec'),scope)
base=scope['st'];st=dict(status='RUNNING',phase='startup',ships={},checks={},rows=[],error=None,started=time.monotonic(),last=-1)
builtins.magazine_probe=st

def save():
 (root/'Docs/QuangNinhIntegration/SHARED_MAGAZINE_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','fleet']},indent=2),encoding='utf-8')
def finish(status):
 st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle'])
 if base.get('status')=='RUNNING':unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_MAGAZINE_FIXTURE'
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
 try:
  if base.get('status')=='FAILED':raise AssertionError(base.get('error'))
  w=ed.get_game_world()
  if not w:return
  t=unreal.GameplayStatics.get_time_seconds(w)
  if st['phase']=='startup':
   if base.get('phase')!='observe':return
   fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
   st.update(fleet=fleet,phase='firing',at=t)
   for a in fleet:
    weapon=a.get_component_by_class(unreal.ShipWeaponComponent);assert weapon.rounds_remaining==10 and not weapon.reloading
    st['ships'][a.get_actor_label()]=dict(player=a.is_player_controlled(),shots=0,empty_at=None,lowest_countdown=30,one_round_per_shot=True,target_switch_preserved=True,duplicate_callback_preserved=True,spam_guarded=True,reload_completed=False)
   st['checks']['six_initialized_full']=len(fleet)==6
   probe=json.loads(unreal.ShipCombatLibrary.verify_magazine_failure_path(next(a for a in fleet if a.is_player_controlled())))
   st['failed_completion_probe']=probe;st['checks']['failed_spawn_completion_consumes_zero']=probe['status']=='PASSED'
   save();return
  if t-st['last']<.15:return
  st['last']=t;sample=[]
  battle=scope['sub'](w,unreal.NavalBattleSubsystem)
  for a in st['fleet']:
   weapon=a.get_component_by_class(unreal.ShipWeaponComponent);r=st['ships'][a.get_actor_label()]
   if r['reload_completed']:continue
   if r['empty_at'] is None:
    before=weapon.rounds_remaining;shots=weapon.shots_fired
    if (c:=a.get_component_by_class(unreal.ShipCombatComponent)):
     enemies=[other for other in st['fleet'] if other.get_component_by_class(unreal.ShipWeaponComponent).team()!=weapon.team()]
     assert c.set_hostile_target(enemies[int(t)%len(enemies)])
     r['target_switch_preserved'] &= weapon==a.get_component_by_class(unreal.ShipWeaponComponent) and weapon.rounds_remaining==before
    accepted=weapon.try_fire_weapon(a.get_actor_location()+unreal.Vector(t*100,1000,0),None,unreal.Vector(),False)
    delta=weapon.shots_fired-shots
    r['one_round_per_shot'] &= delta==int(accepted) and weapon.rounds_remaining==before-delta
    if accepted:
     r['shots']+=1;ammo=weapon.rounds_remaining;weapon.finish_shot(weapon.last_projectile);weapon.finish_shot(None)
     r['duplicate_callback_preserved'] &= weapon.rounds_remaining==ammo and weapon.shots_fired==shots+1
     r['spam_guarded'] &= not any(weapon.try_fire_weapon(unreal.Vector(),None,unreal.Vector(),False) for _ in range(20)) and weapon.rounds_remaining==ammo
     if r['shots']==1:r['first_shot_rounds']=weapon.rounds_remaining
    if weapon.rounds_remaining==0:
     assert weapon.reloading and weapon.reloads_started==1
     r['empty_at']=t;r['last_remaining']=weapon.reload_remaining();r['first_countdown']=weapon.reload_remaining()
   else:
    if weapon.reloading:
     assert weapon.rounds_remaining==0 and weapon.reloads_started==1 and weapon.reloads_completed==0
     assert not weapon.try_fire_weapon(unreal.Vector(),None,unreal.Vector(),False)
     r['lowest_countdown']=min(r['lowest_countdown'],weapon.reload_remaining())
    else:
     assert weapon.rounds_remaining==10 and weapon.reloads_completed==1 and weapon.reloads_started==1
     r['reload_elapsed']=t-r['empty_at'];assert 29.9<=r['reload_elapsed']<=31.5
     r['reload_completed']=True;r['reload_finished_inactive_turn']=not battle.is_ship_active(a)
   sample.append(dict(t=t,ship=a.get_actor_label(),ammo=weapon.rounds_remaining,reloading=weapon.reloading,remaining=weapon.reload_remaining(),shots=weapon.shots_fired,tags=[str(x) for x in a.tags]))
  if sample:st['rows'].extend(sample)
  if all(r['empty_at'] is not None for r in st['ships'].values()) and t-min(r['empty_at'] for r in st['ships'].values())>=5 and 'turn_reload' not in st:
   battle.set_battle_mode(unreal.NavalBattleMode.TURN_BASED);st['turn_reload']=True
  if battle.hud and battle.player_ship:
   p=battle.player_ship;text=battle.hud.ammo_text
   assert ('RELOADING:' in text)==p.reloading
   assert ('AMMO: '+str(p.rounds_remaining)+'/10') in text
   if p.reloading:
    seconds=math.ceil(p.reload_remaining());shown=text.rsplit(':',1)[-1];assert abs(int(shown)-seconds)<=1,'HUD countdown stale'
   st['checks']['hud_matches_authority']=True
  if all(r['reload_completed'] for r in st['ships'].values()):
   records=list(st['ships'].values());st['checks'].update(player_and_five_ai=len(records)==6 and sum(r['player'] for r in records)==1,one_round_per_accepted_shot=all(r['one_round_per_shot'] and r['first_shot_rounds']==9 and r['shots']==10 for r in records),spam_and_duplicate_guards=all(r['spam_guarded'] and r['duplicate_callback_preserved'] for r in records),target_switch_keeps_ammo=all(r['target_switch_preserved'] for r in records),reload_once_30_world_seconds=all(r['reload_completed'] and r['lowest_countdown']<2 for r in records))
   victim=next(a for a in st['fleet'] if not a.is_player_controlled());v=victim.get_component_by_class(unreal.ShipWeaponComponent);before=v.rounds_remaining;v.die(unreal.ShipDeathReason.HEALTH_DEPLETED,None)
   st['checks']['world_time_reload_during_inactive_turn']=any(r.get('reload_finished_inactive_turn') for r in records)
   st['checks']['death_blocks_fire']=not v.try_fire_weapon(unreal.Vector(),None,unreal.Vector(),False) and v.rounds_remaining==before and not v.reloading
   finish('PASSED' if all(st['checks'].values()) else 'FAILED');return
  if t-st['at']>160:raise AssertionError('Magazine/reload fixture time bound')
  if len(st['rows'])%30<6:save()
 except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick);save()
