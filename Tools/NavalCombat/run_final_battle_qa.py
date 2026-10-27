"""Actual menu-selected mixed 3v3 battle, normal skill, original combat/physics.

Only the existing Q camera handler is exercised during combat. No damage,
positions, HP, cooldowns or movement inputs are overridden. Bounded at 300 s.
"""
import builtins,json,time,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
st=dict(handle=None,phase='setup',checks=[],rows=[],frames=[],last=-1,error=None,status='RUNNING',wall_last=None)
builtins.final_battle_qa=st
def hp(a):return a.get_component_by_class(unreal.ShipWeaponComponent)
def check(name,value):st['checks'].append(dict(name=name,passed=bool(value)));assert value,name
def save():
    skip=['handle','old_world','scope']
    (root/'Docs/NavalCombat/FINAL_BATTLE_QA.json').write_text(json.dumps({k:v for k,v in st.items() if k not in skip},indent=2),encoding='utf-8')
def finish(status):
    st['status']=status;save();unreal.unregister_slate_post_tick_callback(st['handle'])
    if editor.get_game_world():unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
def tick(dt):
    w=editor.get_game_world()
    try:
        if not w:
            if st['phase']=='battle' and st['scope']['st']['status']!='RUNNING':
                base=st['scope']['st'];rows=base['rows']
                check('battle probe completed without runtime error',base['error'] is None and base['status'] in ['BOUNDED_RUN_COMPLETE','BATTLE_FINISHED'])
                check('both teams fired original projectiles',all(max((s['shots'] for r in rows for s in r['ships'] if s['team']==team),default=0)>0 for team in [0,1]))
                check('actual AI damage occurred',max((s['damage_events'] for r in rows for s in r['ships']),default=0)>0)
                check('target factions always hostile',all(s.get('target_team') is None or s['target_team']!=s['team'] for r in rows for s in r['ships']))
                check('no live Failed navigation state',all('FAILED' not in s.get('nav','') for r in rows for s in r['ships'] if not s['dead']))
                check('camera observation preserves AI controllers',all(r['controllers_valid'] for r in st['rows']))
                check('Small and Large physical AI observed moving',all(any(s['class']==kind and s['speed_mps']>5 and s['physics'] for r in st['rows'] for s in r['ships'] if s['ai'] and not s['dead']) for kind in ['SMALL','LARGE']))
                st['base_status']=base['status'];finish('PASSED');return
            return
        t=unreal.GameplayStatics.get_time_seconds(w)
        if st['phase']=='setup' and t>=3:
            pc=unreal.GameplayStatics.get_player_controller(w,0);pc.toggle_match_setup();ui=pc.setup_widget
            ui.blue_count_selector.set_selected_index(2);ui.red_count_selector.set_selected_index(2);ui.difficulty_selector.set_selected_index(1);ui.mode_selector.set_selected_index(0)
            for c,i in zip(ui.hull_selectors,[1,0,1,0,1,0]):c.set_selected_index(i)
            st.update(phase='travel',old_world=w);ui.start_match();return
        if st['phase']=='travel':
            if w==st['old_world'] or t<3:return
            fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if hp(a)]
            check('menu selected six ships',len(fleet)==6 and sum(a.is_player_controlled() for a in fleet)==1)
            st['controllers']={a.get_name():a.get_controller().get_path_name() for a in fleet if not a.is_player_controlled()}
            check('three Large three Small selected',sum(a.get_component_by_class(unreal.ShipMovementComponent).hull_class==unreal.ShipHullClass.LARGE for a in fleet)==3)
            p=root/'Tools/NavalCombat/run_battle_validation.py';code=p.read_text().replace("('VALIDATION_'+mode.upper()+'.json')","'FINAL_INTEGRATED_BATTLE.json'")
            start='unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()';assert code.count(start)==1
            code=code.replace(start,'# Existing fresh menu-travel world is already playing.')
            scope=dict(MODE='battle',LIMIT=300,__name__='final_battle');exec(compile(code,str(p),'exec'),scope)
            st.update(phase='battle',scope=scope,wall_last=time.monotonic())
            unreal.GameplayStatics.get_player_controller(w,0).cycle_ship_camera()
        if st['phase']=='battle':
            now=time.monotonic()
            if st['wall_last'] is not None:st['frames'].append(dict(t=t,wall_dt=now-st['wall_last'],slate_dt=dt))
            st['wall_last']=now
            if t-st['last']>=1:
                st['last']=t;fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if hp(a)];rows=[];controllers=True
                for a in fleet:
                    weapon=hp(a);movement=a.get_component_by_class(unreal.ShipMovementComponent);body=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
                    ai=a.get_name() in st['controllers']
                    if ai and not weapon.is_destroyed:controllers &= a.get_controller() is not None and a.get_controller().get_path_name()==st['controllers'][a.get_name()]
                    rows.append(dict(name=a.get_name(),ai=ai,dead=weapon.is_destroyed,hp=weapon.current_health,stage=str(weapon.destruction_stage),sunk=weapon.sunk_distance_m,**{'class':'LARGE' if movement.hull_class==unreal.ShipHullClass.LARGE else 'SMALL'},physics=body.is_simulating_physics(),speed_mps=body.get_physics_linear_velocity().length()/100,fallback_visible=sum(c.is_visible() for c in a.get_components_by_class(unreal.ShipWreckBillboardComponent))))
                terrain=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Cesium3DTileset) if a.get_actor_label()=='QuangNinh_Terrain');f=terrain.get_component_by_class(unreal.SeaTerrainCollisionComponent)
                st['rows'].append(dict(t=t,ships=rows,controllers_valid=controllers,collision_records=f.filtered_tiles,collision_proxies=len(terrain.get_components_by_class(unreal.ProceduralMeshComponent)),collision_error=f.last_error))
                for at in [30,90,150,240]:
                    if t>=at and at not in st.setdefault('screenshots',[]):
                        unreal.SystemLibrary.execute_console_command(w,'Shot showui filename='+str(root/'Docs/NavalCombat'/('FINAL_BATTLE_'+str(at)+'.png')));st['screenshots'].append(at)
                if int(t)//60>st.get('camera_cycle',0):
                    unreal.GameplayStatics.get_player_controller(w,0).cycle_ship_camera();st['camera_cycle']=int(t)//60
                if len(st['rows'])%20==0:save()
    except Exception:
        st['error']=traceback.format_exc();finish('FAILED')
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
