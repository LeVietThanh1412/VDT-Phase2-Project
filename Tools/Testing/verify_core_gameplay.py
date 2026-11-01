"""Controlled real-PIE core gameplay regression; not a natural match.
Original BP shot dispatch supplies projectiles. Region impacts, one capsize pose
and lethal damage are explicit fixtures. Survivor navigation/physics remain live.
"""
import unreal,builtins,json,time,math,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());p=root/'Tools/NavalCombat/test_seeded_spawn.py'
ctx=dict(SEED=20261028,OBSERVE_SECONDS=99999,WALL_LIMIT_SECONDS=700,OUTPUT='CORE_STARTUP.json',OUTPUT_DIR='Saved')
exec(compile(p.read_text(encoding='utf-8'),str(p),'exec'),ctx);base=ctx['st']
s=dict(status='RUNNING',phase='startup',checks={},impacts=[],events=[],error=None,started=time.monotonic());builtins.core_gameplay_probe=s
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
def sub(w,c):return lib.call_method('GetWorldSubsystem',args=(w,c.static_class()))
def weapon(a):return a.get_component_by_class(unreal.ShipWeaponComponent)
def hull(a):return next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
def save():
 (root/'Saved/CORE_GAMEPLAY_REGRESSION.json').write_text(json.dumps({k:v for k,v in s.items() if k not in ['handle','source','victim','capsize','projectile','controllers','fleet']},indent=2),encoding='utf-8')
def finish(status):
 s['status']=status;save();unreal.unregister_slate_post_tick_callback(s['handle'])
 if base['status']=='RUNNING':unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_CORE_FIXTURE'
 if not globals().get('HOLD_FAILURE',False) or status=='PASSED':unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
 try:
  if base['status']=='FAILED':raise AssertionError(base.get('error'))
  w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  if not w:return
  t=unreal.GameplayStatics.get_time_seconds(w)
  fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
  for a in fleet:
   a.can_be_damaged=False
   if c:=a.get_component_by_class(unreal.ShipCombatComponent):c.enable_combat=False
  if s['phase']=='startup':
   if base.get('phase')!='observe':return
   source=next(a for a in fleet if a.is_player_controlled());red=[a for a in fleet if weapon(a).team()==1]
   unreal.unregister_slate_post_tick_callback(base['handle']);base['status']='OWNED_BY_CORE_FIXTURE'
   s.update(source=source,source_name=source.get_name(),victim=red[0],capsize=red[1],phase='regions',at=t,fleet=fleet,controllers={a.get_name():a.get_controller().get_name() for a in fleet})
   assert all(weapon(a).max_health==100 and weapon(a).base_projectile_damage==10 for a in fleet)
   s['checks']['standard_100_10']=True;save();return
  if s['phase']=='regions':
   src=s['source'];wp=weapon(src);victim=s['victim']
   if wp.try_fire_weapon(src.get_actor_location()+unreal.Vector(0,0,100000),None,unreal.Vector(0,0,20000),True):
    proj=wp.last_projectile;assert proj
    h=hull(victim);lo,hi=h.get_local_bounds();cases=[('Bow',.9,8),('Midship',.5,10),('Stern',.1,8)];label,frac,damage=cases[len(s['impacts'])]
    pos=h.get_world_transform().transform_location(unreal.Vector(lo.x+(hi.x-lo.x)*frac,0,(lo.z+hi.z)/2));victim.can_be_damaged=True
    before=weapon(victim).current_health;assert weapon(victim).receive_hull_impact(proj,pos);actual=before-weapon(victim).current_health;assert abs(actual-damage)<.01
    assert not weapon(victim).receive_hull_impact(proj,pos) and abs(weapon(victim).current_health-(before-damage))<.01
    src.can_be_damaged=True;assert not wp.receive_ship_damage(10,proj)
    friend=next(a for a in fleet if a!=src and weapon(a).team()==wp.team());friend.can_be_damaged=True;hp=weapon(friend).current_health;assert not weapon(friend).receive_ship_damage(10,proj) and weapon(friend).current_health==hp
    s['impacts'].append(dict(region=label,expected=damage,actual=actual,reported=str(weapon(victim).last_impact_region)))
    if len(s['impacts'])==3:
     c=victim.get_component_by_class(unreal.ShipCombatComponent);c.set_combat_role(unreal.ShipEnemyRole.PASSIVE);victim.can_be_damaged=True;assert weapon(victim).receive_ship_damage(1,src);assert victim.get_component_by_class(unreal.ShipEnemyNavigationComponent).role==unreal.ShipEnemyRole.FIGHTER
     c.set_combat_role(unreal.ShipEnemyRole.COWARD);c.defensive_fire=False;s.update(phase='coward',at=t,prior_shots=weapon(victim).shots_fired)
    save()
  elif s['phase']=='coward':
   c=s['victim'].get_component_by_class(unreal.ShipCombatComponent);c.enable_combat=True
   if t-s['at']>=2:
    assert weapon(s['victim']).shots_fired==s['prior_shots'];s['checks']['coward_no_defensive_fire']=True;s['checks']['passive_promotes_on_damage']=True
    c.set_combat_role(unreal.ShipEnemyRole.HUNTER);c.hunter_max_pursuit_seconds=3;s.update(phase='hunter',at=t)
  elif s['phase']=='hunter':
   c=s['victim'].get_component_by_class(unreal.ShipCombatComponent);c.enable_combat=True
   if t-s['at']>=4:
    assert s['victim'].get_component_by_class(unreal.ShipEnemyNavigationComponent).role==unreal.ShipEnemyRole.FIGHTER
    s['checks']['hunter_bounded_return_to_fighter']=True;s['hunter_exit']=c.last_role_transition;s['phase']='exhaust_magazine'
  elif s['phase']=='exhaust_magazine':
   src=s['source'];wp=weapon(src)
   if not wp.reloading:wp.try_fire_weapon(src.get_actor_location()+unreal.Vector(0,0,100000),None,unreal.Vector(0,0,20000),True)
   if wp.reloading:
    assert wp.rounds_remaining==0 and wp.reload_remaining()>28;s['projectile']=wp.last_projectile;s['projectile'].set_life_span(50)
    round=s['projectile'].get_component_by_class(unreal.ShipProjectileDamageComponent);assert round.source_team==0 and round.projectile_damage==10
    src.can_be_damaged=True;b=sub(w,unreal.NavalBattleSubsystem);before=b.blue_alive;assert wp.receive_ship_damage(1000,s['victim']);assert not wp.reloading and not wp.receive_ship_damage(1000,s['victim']) and b.blue_alive==before-1
    s['checks']['death_cancels_reload_once']=True;s['checks']['duplicate_death_counter_guard']=True;s.update(phase='wait_cleanup',death_at=t)
    cap=s['capsize'];h=hull(cap);rot=h.get_world_rotation();h.set_world_rotation(unreal.Rotator(0,rot.yaw,179),False,True);h.set_physics_angular_velocity_in_degrees(unreal.Vector())
    save()
  elif s['phase']=='wait_cleanup':
   pc=unreal.GameplayStatics.get_player_controller(w,0)
   if t-s['death_at']>1:
    assert pc.death_spectating;s['checks']['player_death_spectator']=True
    assert pc.get_selected_ship()==pc.followed_teammate,'Death spectator HUD selection does not follow the observed teammate'
    s['checks']['death_spectator_hud_follows_teammate']=True
    s['checks']['ai_controllers_preserved']=all(a.get_controller() and a.get_controller().get_name()==s['controllers'][a.get_name()] for a in fleet if not weapon(a).is_destroyed)
   caps=[a for a in fleet if a.get_name()==s['capsize'].get_name()] if t-s['death_at']<10 else []
   if caps and weapon(caps[0]).is_destroyed:s['checks']['physical_capsize_detected']=weapon(caps[0]).death_reason==unreal.ShipDeathReason.CAPSIZED
   if t-s['death_at']>8:assert s['checks'].get('physical_capsize_detected',False),'Capsize pose did not trigger the physical confirmation path'
   if not any(a.get_name()==s['source_name'] for a in fleet) and t-s['death_at']>22:
    victim=s['victim'];victim.can_be_damaged=True;hp=weapon(victim).current_health
    assert weapon(victim).receive_ship_damage(10,s['projectile']) and abs(weapon(victim).current_health-(hp-10))<.01
    s['checks']['cached_source_survives_shooter_cleanup']=True;s['checks']['regional_8_10_8_and_self_friendly_duplicate_guards']=len(s['impacts'])==3
    assert all(s['checks'].values());finish('PASSED');return
  if time.monotonic()-s['started']>400:raise AssertionError('Core fixture wall-time bound')
 except Exception:s['error']=traceback.format_exc();finish('FAILED')
s['handle']=unreal.register_slate_post_tick_callback(tick);save()
