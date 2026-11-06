"""Final demo PIE evidence through production menu/travel and real physics.
KIND=nonlethal disables damage only; KIND=lethal requires the native winner.
No ship transforms, drive commands, aim parameters or target assignments occur here.
"""
import unreal,builtins,json,time,traceback,math,hashlib
from pathlib import Path
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
kind=globals().get('KIND','nonlethal');seed=globals().get('SEED',20261028);full_ai=globals().get('FULL_AI',True)
name=globals().get('OUTPUT','FINAL_'+kind.upper()+'_'+str(seed)+'.json')
st=dict(status='RUNNING',kind=kind,seed=seed,full_ai=full_ai,phase='menu',started=time.monotonic(),ships={},samples=[],error=None,frame_times=[],camera_checks=[])
st['native_dll_sha256']=hashlib.sha256((root/'Binaries/Win64/UnrealEditor-VDTPhase2Project.dll').read_bytes()).hexdigest()
st['production_sources_sha256']=hashlib.sha256(b''.join(p.read_bytes() for p in sorted((root/'Source/VDTPhase2Project').rglob('*')) if p.suffix in ['.h','.cpp'])).hexdigest()
builtins.final_demo_probe=st
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
def sub(w,c):return lib.call_method('GetWorldSubsystem',args=(w,c.static_class()))
def save():
 data={k:v for k,v in st.items() if k not in ['handle','old_world','last_positions','last_shots']}
 tmp=root/'Saved'/(name+'.tmp');tmp.write_text(json.dumps(data,indent=2),encoding='utf-8')
 try:tmp.replace(root/'Saved'/name)
 except PermissionError:
  # A concurrent Windows reader may briefly deny rename. Keep the completed
  # temporary snapshot and retry on the next sample instead of aborting PIE.
  pass
def end(status):
 st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle'])
 if not globals().get('HOLD',False):unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
 try:
  assert time.monotonic()-st['started']<float(globals().get('WALL_LIMIT_SECONDS',1400)),'Fixture wall-time bound'
  w=ed.get_game_world()
  if not w:return
  t=unreal.GameplayStatics.get_time_seconds(w);pc=unreal.GameplayStatics.get_player_controller(w,0)
  fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
  for a in fleet:
   weapon=a.get_component_by_class(unreal.ShipWeaponComponent)
   a.can_be_damaged=kind=='lethal' and not weapon.is_destroyed
   if kind in ['patrol','turns'] and (c:=a.get_component_by_class(unreal.ShipCombatComponent)):c.enable_combat=False
  if st['phase']=='menu' and time.monotonic()-st['started']>2:
   settings=lib.call_method('GetGameInstanceSubsystem',args=(unreal.GameplayStatics.get_game_instance(w),unreal.NavalMatchSettings.static_class()))
   settings.enable_sector_tactics=bool(globals().get('TACTICS',True));settings.enable_red_assault=bool(globals().get('ASSAULT',True))
   if not pc.setup_widget:pc.toggle_match_setup()
   ui=pc.setup_widget;ui.blue_count_selector.set_selected_index(int(globals().get('BLUE_COUNT',3))-1);ui.red_count_selector.set_selected_index(int(globals().get('RED_COUNT',3))-1);ui.control_selector.set_selected_index(1 if full_ai else 0);ui.balance_selector.set_selected_index(0)
   ui.seed_input.set_text(str(seed));ui.mode_selector.set_selected_index(int(globals().get('BATTLE_MODE',0)))
   ui.player_team_selector.set_selected_index(int(globals().get('PLAYER_TEAM',0)))
   ui.generate_from_seed()
   for c,i in zip(ui.hull_selectors,globals().get('HULL_CLASSES',[0,0,0,0,1,2])):c.set_selected_index(i)
   st.update(phase='initialization',old_world=w);ui.start_match()
   settings.enable_sector_tactics=bool(globals().get('TACTICS',True));settings.enable_red_assault=bool(globals().get('ASSAULT',True));settings.use_island_patrol_circuit=settings.enable_sector_tactics
   save();return
  if st['phase']=='initialization' and w!=st['old_world']:
   field=sub(w,unreal.NavalBattlefieldSubsystem);battle=sub(w,unreal.NavalBattleSubsystem)
   if int(t)!=st.get('init_last',-1):
    st['init_last']=int(t);st['initialization']=dict(t=t,status=field.status,error=field.initialization_error,configured=battle.match_configured,fleet=len(fleet));save()
   assert not field.initialization_error,field.initialization_error
   assert not battle.configuration_error.startswith('Full AI:'),battle.configuration_error
   assert t<40,'Native battlefield initialization did not finish'
   if field.ready:
    st.update(phase='observe',ready_at=t,layout=json.loads(field.layout_report()),controllers={a.get_name():a.get_controller().get_name() if a.get_controller() else None for a in fleet})
    expected_total=int(globals().get('BLUE_COUNT',3))+int(globals().get('RED_COUNT',3));expected_ai=expected_total if full_ai else expected_total-1
    assert len(fleet)==expected_total and sum(bool(a.get_component_by_class(unreal.ShipEnemyNavigationComponent)) for a in fleet)==expected_ai,'Incorrect ship roster'
    assert sum(a.is_player_controlled() for a in fleet)==(0 if full_ai else 1),'Ship possession incorrect'
    if not full_ai:assert unreal.GameplayStatics.get_player_pawn(w,0).get_component_by_class(unreal.ShipWeaponComponent).team()==int(globals().get('PLAYER_TEAM',0)),'Player team selection lost'
    if full_ai:assert pc.full_ai_spectating and isinstance(unreal.GameplayStatics.get_player_pawn(w,0),unreal.FreeCameraPawn),'Missing independent spectator'
    for a in fleet:
     wpn=a.get_component_by_class(unreal.ShipWeaponComponent);assert wpn.max_health==100 and wpn.base_projectile_damage==10,'Wrong standard demo balance'
     assert wpn.magazine_capacity==10 and wpn.reload_duration_seconds==30,'Wrong standard magazine/reload'
     st.setdefault('team_tags',{})[a.get_name()]=[str(x) for x in a.tags]
     assert ('Team.Blue' if wpn.team()==0 else 'Team.Red') in st['team_tags'][a.get_name()],'Missing authoritative team tag'
    save()
  if st['phase']=='observe':
   st['frame_times'].append(float(dt))
   if t-st.get('last',-1)<.5:return
   st['last']=t;age=t-st['ready_at'];field=sub(w,unreal.NavalBattlefieldSubsystem);grid=sub(w,unreal.NavalSectorSubsystem);battle=sub(w,unreal.NavalBattleSubsystem)
   sample=dict(t=t,age=age,outcome=battle.outcome,blue=battle.blue_alive,red=battle.red_alive,ships=[],projectiles=sum(bool(a.get_component_by_class(unreal.ShipProjectileDamageComponent)) for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor)))
   for a in fleet:
    label=a.get_actor_label();key=a.get_name();weapon=a.get_component_by_class(unreal.ShipWeaponComponent);n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent);c=a.get_component_by_class(unreal.ShipCombatComponent)
    h=next(x for x in a.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM');en=list(grid.world_to_chart(h.get_world_location()).to_tuple())
    s=st['ships'].setdefault(key,dict(label=label,player_controlled=a.is_player_controlled(),team=weapon.team(),class_name=str(a.get_component_by_class(unreal.ShipMovementComponent).hull_class),first_detection=None,first_in_range=None,first_visible=None,first_valid_opportunity=None,first_shot=None,shots=0,hits=0,damage_events=0,distance_m=0,physical=True,controller_preserved=True,dead=False,recoveries=0,legs=0,failures=[],last_en=en))
    s['distance_m']+=math.dist(en,s['last_en']);s['last_en']=en;s['physical'] &= bool(weapon.is_destroyed or h.is_simulating_physics());s['controller_preserved'] &= bool(weapon.is_destroyed or (a.get_controller() and a.get_controller().get_name()==st['controllers'].get(key)))
    assert kind=='lethal' or not weapon.is_destroyed,'Nonlethal hull died: '+label
    s.update(shots=weapon.shots_fired,hits=weapon.hits,damage_events=weapon.accepted_damage_events,hp=weapon.current_health,dead=weapon.is_destroyed,reloads_started=weapon.reloads_started,reloads_completed=weapon.reloads_completed)
    if n:
     s.update(legs=n.patrol_legs_completed,circuits=n.patrol_circuits_completed,circuit_legs=n.circuit_legs_completed,recoveries=max(s['recoveries'],n.recovery_attempt_count),max_search_ms=max(s.get('max_search_ms',0),n.last_search_milliseconds))
     diagnostic=json.loads(n.get_navigation_diagnostics());s['plan_count']=diagnostic.get('plans',0);s['recovery_retries']=diagnostic.get('recovery_retries',0)
     if n.stuck and not s.get('was_stuck',False):s['stuck_events']=s.get('stuck_events',0)+1
     s['was_stuck']=n.stuck
    if c:s.update(retreats=c.retreats_started,backward_retreats=c.backward_retreats_started,patrols_resumed=c.patrols_resumed)
    if weapon.is_destroyed and 'death_age' not in s:
     s.update(death_age=age,death_reason=str(weapon.death_reason))
     if kind=='lethal' and globals().get('CAPTURE',True):
      s['death_capture']=unreal.ShipCombatLibrary.capture_gameplay_hud(w,str(root/'Saved'/(name.removesuffix('.json')+'_DEATH_'+str(len([v for v in st['ships'].values() if v.get('dead')]))+'.png')))
    if n and n.last_navigation_failure_reason and n.last_navigation_failure_reason not in s['failures']:s['failures'].append(n.last_navigation_failure_reason)
    valid=c and c.target and c.is_hostile(c.target)
    in_range=bool(valid and c.min_engagement_radius_m<=c.distance_to_target_m<=c.engagement_radius_m)
    for condition,event in [(valid,'first_detection'),(in_range,'first_in_range'),(valid and c.target_visible,'first_visible'),(in_range and c.target_visible and c.has_firing_solution,'first_valid_opportunity'),(weapon.shots_fired>0,'first_shot')]:
     if condition and s[event] is None:s[event]=age
    sample['ships'].append(dict(ship=key,en=en,rounds=weapon.rounds_remaining,reloading=weapon.reloading,reload_remaining=weapon.reload_remaining(),nav=str(n.navigation_state) if n else '',legs=n.patrol_legs_completed if n else 0,tactics=str(c.tactical_state) if c else '',target=c.target.get_name() if valid else None,distance=c.distance_to_target_m if valid else None,visible=c.target_visible if c else False,solution=c.has_firing_solution if c else False,firing=c.firing_status if c else '',speed=h.get_physics_linear_velocity().length()/100,hp=weapon.current_health,shots=weapon.shots_fired,in_water=a.get_component_by_class(unreal.BuoyancyComponent).is_in_water_body()))
   st['samples'].append(sample)
   if kind=='turns':
    if not st.get('turns') or st['turns'][-1]['number']!=battle.turn_number:
     active=battle.active_turn_ship;team=active.get_component_by_class(unreal.ShipWeaponComponent).team() if active else None
     st.setdefault('turns',[]).append(dict(number=battle.turn_number,age=age,team=team,actor=active.get_name() if active else None))
    if age-st['turns'][-1]['age']>.25:
     for a in fleet:
      if a==battle.active_turn_ship:continue
      m=a.get_component_by_class(unreal.ShipMovementComponent);n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
      assert m.last_propulsion_force.length()<.01 and m.last_rudder_force.length()<.01 and (not n or abs(n.desired_throttle)<.001),'Inactive ship issued movement force'
   if globals().get('EXPORT_CHARTS',False) and 'charts_exported' not in st:
    for a in fleet:
     cls=str(a.get_component_by_class(unreal.ShipMovementComponent).hull_class)
     (root/'Saved'/('FINAL_DEMO_CHART_'+cls.split('.')[1].split(':')[0]+'.json')).write_text(field.inspect_navigation_chart(a),encoding='utf-8')
    st['charts_exported']=True
   if age>10 and not st.get('camera_checked') and full_ai:
    for i in range(7):pc.call_method('CycleShipCamera')
    pc.show_battlefield_overview();st['camera_checks'].append(dict(t=t,all_controllers_unchanged=all(a.get_controller() and a.get_controller().get_name()==st['controllers'][a.get_name()] for a in fleet),no_ship_player_possession=not any(a.is_player_controlled() for a in fleet)))
    st['camera_checked']=True
   if age>12 and globals().get('CAPTURE',True) and not st.get('captured'):
    st['captured']=unreal.ShipCombatLibrary.capture_gameplay_hud(w,str(root/'Saved'/(name.removesuffix('.json')+'.png')))
   if int(age)%5==0:save()
   if kind=='nonlethal' and len(st['ships'])==6 and all(s['first_valid_opportunity'] is not None and s['first_shot'] is not None for s in st['ships'].values()):
    st['duration']=age;end('PASSED');return
   if kind=='turns' and age>=float(globals().get('OBSERVE_SECONDS',120)):
    turns=st['turns'];assert len(turns)>=8
    assert all(a['team']!=b['team'] and abs(b['age']-a['age']-10)<.75 for a,b in zip(turns,turns[1:])),'Turn timing/team alternation failed'
    st['duration']=age;end('PASSED');return
   if kind=='lethal' and battle.outcome:
    assert battle.blue_alive==0 or battle.red_alive==0,'Winner with surviving opponents'
    st.setdefault('outcome_first_seen',age)
    summaries=[u for u in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.MatchSummaryWidget,False) if u.is_in_viewport()]
    if not summaries:
     assert age-st['outcome_first_seen']<5,'Native winner did not display teammate match summary'
     return
    assert len(summaries)==1 and pc.show_mouse_cursor,'Result UI duplicated or mouse unavailable'
    st['result_summary_visible']=True
    if globals().get('CAPTURE',True):st['result_capture']=unreal.ShipCombatLibrary.capture_gameplay_hud(w,str(root/'Saved'/(name.removesuffix('.json')+'_RESULT.png')))
    st.update(duration=age,outcome=battle.outcome,final_layout=json.loads(field.layout_report()));end('PASSED');return
   required_circuits=int(globals().get('REQUIRED_CIRCUITS',0))
   loops_done=not required_circuits or all(s.get('circuits',0)>=required_circuits for s in st['ships'].values() if s['team']==0)
   if kind=='patrol' and loops_done and age>=float(globals().get('OBSERVE_SECONDS',60)):
    assert all((s.get('player_controlled',False) or s['distance_m']>50) and s['physical'] and s['controller_preserved'] for s in st['ships'].values()),'Stationary/invalid ship'
    st['duration']=age;end('PASSED');return
   if age>=float(globals().get('LIMIT_SECONDS',720 if kind=='lethal' else 300)):
    st.update(duration=age,final_layout=json.loads(field.layout_report()));end('TIME_LIMIT_REACHED');return
  assert time.monotonic()-st['started']<float(globals().get('WALL_LIMIT_SECONDS',1400)),'Fixture wall-time bound'
 except Exception:st['error']=traceback.format_exc();end('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick);save();unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
