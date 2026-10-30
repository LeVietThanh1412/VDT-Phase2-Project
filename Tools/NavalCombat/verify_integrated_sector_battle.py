"""Actual seeded Blue/Red battle plus real menu restart.
Normal native combat/damage and physics. Player trigger requests are explicit
manual-input emulation; no health, ammo, transforms or AI targets are altered.
"""
import unreal,builtins,json,time,math,traceback,hashlib
from pathlib import Path
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
scope=dict(SEED=20261028,HULL_CLASSES=[2,1,2,0,0,2],ENABLE_TACTICS=True,ENABLE_ASSAULT=True,COMBAT_TEAMS=[0,1],ALLOW_DAMAGE=True,OBSERVE_SECONDS=99999,WALL_LIMIT_SECONDS=1800,OUTPUT='COMBINED_STARTUP.json',OUTPUT_DIR='Saved',__name__='combined_start')
p=root/'Tools/NavalCombat/test_seeded_spawn.py';exec(compile(p.read_text(encoding='utf-8'),str(p),'exec'),scope)
base=scope['st'];st=dict(status='RUNNING',phase='startup',rows=[],ships={},checks={},events=[],error=None,last=-1,started=time.monotonic(),native_dll_sha256=hashlib.sha256((root/'Binaries/Win64/UnrealEditor-VDTPhase2Project.dll').read_bytes()).hexdigest())
builtins.integrated_sector_probe=st

def save():
 (root/'Saved/INTEGRATED_SECTOR_BATTLE_FULL.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','fleet','old_world','field','grid']},indent=2),encoding='utf-8')
def finish(status):
 st['status']=status;save()
 (root/'Docs/QuangNinhIntegration/INTEGRATED_SECTOR_BATTLE.json').write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','fleet','old_world','field','grid','rows']},indent=2),encoding='utf-8')
 unreal.unregister_slate_post_tick_callback(st['handle'])
 if base.get('status')=='RUNNING':unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_INTEGRATED_FIXTURE'
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def check_tags(a,weapon,movement,nav):
 tags={str(x) for x in a.tags};assert ('Team.Blue' in tags)==(weapon.team()==0);assert ('Team.Red' in tags)==(weapon.team()==1)
 kind={unreal.ShipHullClass.SMALL:'Small',unreal.ShipHullClass.MEDIUM:'Medium',unreal.ShipHullClass.LARGE:'Large'}[movement.hull_class];assert 'Ship.Class.'+kind in tags
 if nav and not weapon.is_destroyed:assert len([x for x in tags if x.startswith('AI.Role.')])==1 and len([x for x in tags if x.startswith('AI.State.')])==1
 return sorted(tags)
def tick(dt):
 try:
  if base.get('status')=='FAILED':raise AssertionError(base.get('error'))
  w=ed.get_game_world()
  if not w:return
  t=unreal.GameplayStatics.get_time_seconds(w)
  if st['phase']=='startup':
   if base.get('phase')!='observe':return
   fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
   st.update(phase='battle',fleet=fleet,at=t,field=scope['sub'](w,unreal.NavalBattlefieldSubsystem),grid=scope['sub'](w,unreal.NavalSectorSubsystem))
   st['checks']['six_seeded_ships_ready']=len(fleet)==6 and st['field'].actual_seed==20261028 and st['field'].ready
   st['initial_layout']=json.loads(st['field'].layout_report());st['checks']['friendly_filter_all_pairs']=True
   for a in fleet:
    weapon=a.get_component_by_class(unreal.ShipWeaponComponent);n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent);c=a.get_component_by_class(unreal.ShipCombatComponent)
    if c:
     for other in fleet:
      expected=other!=a and weapon.team()!=other.get_component_by_class(unreal.ShipWeaponComponent).team();assert c.is_hostile(other)==expected
    ally=next(b for b in fleet if b!=a and b.get_component_by_class(unreal.ShipWeaponComponent).team()==weapon.team());hp=weapon.current_health
    assert not weapon.receive_ship_damage(1,ally) and weapon.current_health==hp
    st['ships'][a.get_actor_label()]=dict(team=weapon.team(),player=a.is_player_controlled(),start=list(st['grid'].world_to_chart(a.get_actor_location()).to_tuple()),max_travel_m=0,shots=0,damage_events=0,reloads_started=0,reloads_completed=0,max_phase=0,interceptions=0,returns=0,retreats=0,dead=False,controller=a.get_controller().get_path_name(),depart_at=None,released_at=-1,slot=-1,last_failure_since=None,physical=True,hostile_targets=True)
   st['checks']['friendly_damage_rejected_all_six']=True;save();return
  if st['phase']=='restart':
   if w==st['old_world']:return
   field=scope['sub'](w,unreal.NavalBattlefieldSubsystem)
   assert not field.initialization_error,field.initialization_error
   if not field.ready:return
   fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
   st['checks']['restart_fresh_magazines_and_health']=len(fleet)==6 and all((weapon:=a.get_component_by_class(unreal.ShipWeaponComponent)).rounds_remaining==10 and weapon.shots_fired==0 and not weapon.reloading and not weapon.is_destroyed and weapon.current_health==weapon.max_health for a in fleet)
   st['checks']['restart_same_seed']=field.actual_seed==20261028
   finish('PASSED' if all(st['checks'].values()) else 'FAILED');return
  if t-st['last']<.5:return
  st['last']=t;field=st['field'];grid=st['grid'];battle=scope['sub'](w,unreal.NavalBattleSubsystem);sample=[]
  for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn):
   label=a.get_actor_label()
   if label not in st['ships']:continue
   r=st['ships'][label];weapon=a.get_component_by_class(unreal.ShipWeaponComponent);m=a.get_component_by_class(unreal.ShipMovementComponent);n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent);c=a.get_component_by_class(unreal.ShipCombatComponent);h=next(x for x in a.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM');tags=check_tags(a,weapon,m,n)
   r.update(shots=weapon.shots_fired,damage_events=weapon.accepted_damage_events,reloads_started=weapon.reloads_started,reloads_completed=weapon.reloads_completed,dead=weapon.is_destroyed)
   if weapon.is_destroyed:
    if n:
     st['events'].append(dict(t=t,event='dead_commands_observed',ship=label,throttle=n.desired_throttle,rudder=n.desired_rudder,combat=c.enable_combat))
     assert abs(n.desired_throttle)<.001 and abs(n.desired_rudder)<.001 and not c.enable_combat
    continue
   pos=list(grid.world_to_chart(a.get_actor_location()).to_tuple());travel=math.dist(r['start'],pos);r['max_travel_m']=max(r['max_travel_m'],travel)
   if r['depart_at'] is None and travel>20:r['depart_at']=t
   r['physical'] &= h.is_simulating_physics();assert r['physical'];assert a.get_controller() and a.get_controller().get_path_name()==r['controller']
   assignment=next(x for x in field.assignments if x.ship==a);r.update(slot=assignment.slot,released_at=assignment.assault_released_at);r['max_phase']=max(r['max_phase'],assignment.assault_phase);r['interceptions']=max(r['interceptions'],assignment.interceptions_started);r['returns']=max(r['returns'],assignment.defensive_returns)
   if c:
    r['retreats']=max(r['retreats'],c.retreats_started)
    if c.target and (target_weapon:=c.target.get_component_by_class(unreal.ShipWeaponComponent)) and not target_weapon.is_destroyed:assert c.is_hostile(c.target),'Live combat retained non-hostile target'
   if n:
    failed=n.navigation_state==unreal.ShipNavigationState.FAILED
    r['last_failure_since']=r['last_failure_since'] if failed and r['last_failure_since'] is not None else t if failed else None
    assert r['last_failure_since'] is None or t-r['last_failure_since']<45,'Navigation permanently failed'
    if weapon.team()==0:assert math.dist(pos,list(assignment.defensive_anchor_en.to_tuple()))<=field.defensive_leash_m+150 and pos[0]<=14750,'Blue left authorized defensive route allowance'
   # Explicit ordinary trigger emulation; AI aiming, firing and damage stay native.
   if r['player'] and weapon.shots_fired<10:weapon.fire_player()
   sample.append(dict(ship=label,team=weapon.team(),hp=weapon.current_health,ammo=weapon.rounds_remaining,reload_remaining=weapon.reload_remaining(),shots=weapon.shots_fired,en_m=pos,phase=assignment.assault_phase,mission=assignment.mission_state,tags=tags,nav=str(n.navigation_state) if n else None,target=c.target.get_actor_label() if c and c.target else None,roll=h.get_world_rotation().roll,pitch=h.get_world_rotation().pitch))
  st['rows'].append(dict(t=t,ships=sample,leader=field.assault_leader.get_actor_label() if field.assault_leader else None,leader_changes=field.assault_leader_changes,outcome=battle.outcome))
  if battle.hud and battle.player_ship and not battle.player_ship.is_destroyed:
   p=battle.player_ship;text=battle.hud.ammo_text;assert ('RELOADING:' in text)==p.reloading
   st['checks']['hud_ammo_live']=('AMMO: '+str(p.rounds_remaining)+'/10') in text
  for at in [30,90,150,240]:
   if t-st['at']>=at and at not in st.setdefault('screenshots',[]):
    unreal.SystemLibrary.execute_console_command(w,'Shot showui filename='+str(root/'Docs/QuangNinhIntegration'/('COMBINED_'+str(at)+'.png')));st['screenshots'].append(at)
  if len(st['rows'])%10==0:save()
  records=list(st['ships'].values());reds=sorted([r for r in records if r['team']==1],key=lambda r:r['slot'])
  ready=t-st['at']>=180 and all(any(r['shots']>0 and not r['player'] for r in records if r['team']==team) for team in [0,1]) and any(r['damage_events']>0 for r in records) and any(r['reloads_completed']>0 for r in records)
  if ready or t-st['at']>650 or battle.outcome:
   st['duration_game_seconds']=t-st['at'];st['checks'].update(both_teams_native_ai_fire=all(any(r['shots']>0 and not r['player'] for r in records if r['team']==team) for team in [0,1]),actual_hostile_damage=any(r['damage_events']>0 for r in records),magazine_and_reload_observed=any(r['reloads_completed']>0 for r in records),red_sequential_release=all(r['released_at']>=0 for r in reds) and all(b['released_at']-a['released_at']>=field.formation_departure_interval_seconds-.05 for a,b in zip(reds,reds[1:])),red_reaches_assault=any(r['max_phase']>=2 for r in reds),blue_intercepts=any(r['interceptions']>0 for r in records if r['team']==0),ai_physically_moving=all(r['max_travel_m']>25 for r in records if not r['player']),standard_tags_and_hostile_targets=True)
   if not all(st['checks'].values()):finish('FAILED');return
   unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_RESTART_FIXTURE'
   pc=unreal.GameplayStatics.get_player_controller(w,0);pc.toggle_match_setup();ui=pc.setup_widget;ui.seed_input.set_text('20261028');st.update(phase='restart',old_world=w);ui.start_match();save()
  if time.monotonic()-st['started']>1500:raise AssertionError('Combined battle wall time limit')
 except Exception:st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick);save()
