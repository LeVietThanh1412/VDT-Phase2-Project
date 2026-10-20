"""Disposable real-PIE impact fixtures. Fires original BP weapons into actual hulls.
Ships are frozen/repositioned ONLY in this isolated test; nothing is saved.
"""
import builtins,json,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());state=dict(handle=None,phase='initialize',rows=[],stage=0,at=0,error=None)
builtins.damage_ui_test=state
def save():
    (root/'Docs/NavalCombat/DAMAGE_UI_PIE.json').write_text(json.dumps({k:v for k,v in state.items() if k not in ['handle','ships','cases','positions','shooter','victim','projectile']},indent=2))
def snap(name,w):
    unreal.SystemLibrary.execute_console_command(w,'Shot showui filename='+str(root/'Docs/NavalCombat'/name))
def finish(w):
    save();unreal.unregister_slate_post_tick_callback(state['handle']);state['handle']=None
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def hp(s):return s.get_component_by_class(unreal.ShipWeaponComponent)
def hud(w):
    h=unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.ShipHUDWidget,False)[0]
    return dict(hp=h.health_text,score=h.score_text,result=h.result_text,viewport=h.is_in_viewport())
def bar(s):
    w=hp(s).health_widget
    if not w:return None
    x=w.get_user_widget_object()
    return dict(fraction=x.displayed_fraction,color=[x.health_color.r,x.health_color.g,x.health_color.b],visible=w.is_visible())
def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    try:
        if state['phase']=='initialize':
            if t<1:return
            ships=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,'/Game/Ship/BluePrint/BP_Ship.BP_Ship_C'))
            assert len(ships)==6
            p=next(s for s in ships if s.is_player_controlled());f=[s for s in ships if hp(s).team()==0 and s!=p];e=[s for s in ships if hp(s).team()==1]
            state.update(ships=ships,positions={s.get_name():s.get_actor_location() for s in ships},cases=[
                ('player_to_friendly',p,f[0],100),('enemy_to_enemy',e[0],e[1],100),
                ('player_to_enemy',p,e[0],90),('enemy_to_friendly',e[1],f[0],90),
                *[(('yellow_hp' if value==50 else 'red_hp' if value==20 else 'enemy_destroyed' if value==0 else 'hostile_hit_'+str(value)),p,e[0],value) for value in range(80,-1,-10)]])
            state['events']=[]
            def health_event(weapon,current,maximum):
                state['events'].append(dict(event='health',ship=weapon.get_owner().get_actor_label(),hp=current,maximum=maximum))
            def death_event(weapon,attacker):
                row=dict(event='destroyed',ship=weapon.get_owner().get_actor_label(),active_destruction_fx=[])
                try:
                    for fx in unreal.ObjectIterator(unreal.NiagaraComponent):
                        scale=fx.get_world_scale()
                        if fx.is_active() and abs(scale.x-12)<.01:
                            row['active_destruction_fx'].append(dict(asset=fx.get_asset().get_path_name(),scale=[scale.x,scale.y,scale.z]))
                except Exception as error:row['vfx_inspection_error']=str(error)
                state['events'].append(row);snap('DEATH_VFX_'+row['ship']+'.png',w)
            for s in ships:
                hp(s).on_health_changed.add_callable(health_event);hp(s).on_ship_destroyed.add_callable(death_event)
                c=s.get_component_by_class(unreal.ShipCombatComponent);n=s.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=False
                if n:n.stop_navigation()
                h=next(x for x in s.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM');h.set_simulate_physics(False)
            state['rows'].append(dict(name='initial',hud=hud(w),ships=[dict(name=s.get_actor_label(),hp=hp(s).current_health,team=hp(s).team(),bar=bar(s)) for s in ships]))
            snap('UI_INITIAL.png',w);state['phase']='fire';state['at']=t+1;save()
        elif state['phase']=='fire' and t>=state['at']:
            name,shooter,victim,expected=state['cases'][state['stage']]
            # Isolate original hull collisions from all unrelated actors and old shells.
            for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                if a.get_class().get_name().startswith('BP_ShipProjectile'):a.destroy_actor()
            for s in state['ships']:
                s.set_actor_location(state['positions'][s.get_name()]+(unreal.Vector() if s in [shooter,victim] else unreal.Vector(0,0,1000000)),False,True)
            mesh=hp(shooter).mesh() if hasattr(hp(shooter),'mesh') else shooter.get_component_by_class(unreal.SkeletalMeshComponent)
            start=mesh.get_socket_location('front_gun_v_socket');direction=mesh.get_socket_rotation('front_gun_v_socket').get_forward_vector()
            flight=1.5;end=start+direction*30000+unreal.Vector(0,0,.5*-980.0*flight*flight)
            hull=next(x for x in victim.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=='ShipSM')
            origin,extent,radius=unreal.SystemLibrary.get_component_bounds(hull);victim.set_actor_location(victim.get_actor_location()+end-origin,False,True)
            before=hp(victim).accepted_damage_events;before_hp=hp(victim).current_health
            fired=hp(shooter).try_fire_weapon(unreal.Vector(),None,unreal.Vector(),False)
            state.update(shooter=shooter,victim=victim,at=t+3.5,phase='check',before=before,before_hp=before_hp,fired=fired)
        elif state['phase']=='check' and t>=state['at']:
            name,shooter,victim,expected=state['cases'][state['stage']];v=hp(victim)
            row=dict(name=name,fired=state['fired'],hp=v.current_health,expected=expected,before_hp=state['before_hp'],damage_events=v.accepted_damage_events,
                new_damage_events=v.accepted_damage_events-state['before'],rejected=v.rejected_team_hits,destruction_events=v.destruction_events,
                destroyed=v.is_destroyed,bar=bar(victim),hud=hud(w))
            row['passed']=row['fired'] and row['hp']==expected and row['new_damage_events']==(0 if 'to_friendly' in name and name.startswith('player') or name=='enemy_to_enemy' else 1)
            state['rows'].append(row);snap('UI_'+name+'.png',w);save()
            if state['stage']+1<len(state['cases']):state['stage']+=1;state['phase']='fire';state['at']=t+2
            else:
                # Repeated damage after death cannot broadcast destruction a second time.
                repeat=v.receive_ship_damage(100,shooter)
                state['rows'].append(dict(name='destruction_once',passed=not repeat and v.destruction_events==1,hud=hud(w)))
                for s in state['ships']:
                    if hp(s).team()==1 and not hp(s).is_destroyed:hp(s).receive_ship_damage(100,shooter)
                state['rows'].append(dict(name='victory',hud=hud(w),passed=hud(w)['result']=='VICTORY'))
                snap('UI_VICTORY.png',w);state['phase']='done';state['at']=t+2;save()
        elif state['phase']=='done' and t>=state['at']:finish(w)
    except Exception:
        state['error']=traceback.format_exc();state['phase']='ERROR';finish(w)
state['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
