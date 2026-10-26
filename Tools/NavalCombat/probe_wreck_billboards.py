"""Bounded disposable visual prototype, using the real shared death event.

Run only after the native fallback component and its two materials are built.
CAPSIZE=True inspects geographic-up placement on a rolled wreck. This fixture
extends only its own burning interval for screenshots, then stops PIE at 32 s.
"""
import builtins,json,traceback
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir());capsize=globals().get('CAPSIZE',False)
prefix='WRECK_BILLBOARD_CAPSIZE' if capsize else 'WRECK_BILLBOARD_HEALTH'
st=dict(handle=None,initialized=False,rows=[],shots=[],error=None,last=-1)
builtins.wreck_billboard_probe=st

def finish(status):
    st['status']=status;unreal.unregister_slate_post_tick_callback(st['handle'])
    (root/'Docs/NavalCombat'/(prefix+'.json')).write_text(json.dumps({k:v for k,v in st.items() if k not in ['handle','target','camera','effects']},indent=2),encoding='utf-8')
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()

def tick(dt):
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not w:return
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t<2:return
    try:
        if not st['initialized']:
            ships=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
            target=next(a for a in ships if a.get_actor_label()=='BP_ShipEnemy');source=next(a for a in ships if a.is_player_controlled())
            for a in ships:
                a.set_actor_tick_enabled(False)
                c=a.get_component_by_class(unreal.ShipCombatComponent);n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
                if c:c.enable_combat=False;c.draw_debug=False
                if n:n.stop_navigation();n.draw_debug=False
            hull=next(c for c in target.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
            effect_class=unreal.load_class(None,'/Script/VDTPhase2Project.ShipWreckBillboardComponent');assert effect_class
            fire=unreal.load_asset('/Game/NavalBattle/FX/M_WreckFlamesBillboard');smoke=unreal.load_asset('/Game/NavalBattle/FX/M_WreckSmokeBillboard');assert fire and smoke
            specs=[(fire,-3500,-300,1200,2200),(fire,0,100,1500,2600),(fire,3000,200,1000,1800),(smoke,-2000,0,3000,5500),(smoke,2000,0,2500,4500)]
            effects=list(target.get_components_by_class(unreal.ShipWreckBillboardComponent))
            for i,(mat,x,y,sx,sy) in enumerate([] if effects else specs):
                component=target.call_method('AddComponentByClass',args=(effect_class,True,unreal.Transform(),False))
                assert component
                component.attach_to_component(hull,'',unreal.AttachmentRule.KEEP_RELATIVE,unreal.AttachmentRule.KEEP_RELATIVE,unreal.AttachmentRule.KEEP_RELATIVE,False)
                component.deck_offset_cm=unreal.Vector(x,y,500+sy);component.phase=i*.37
                component.add_element(mat,None,False,sx,sy,None);effects.append(component)
            st['hidden_while_alive']=all(not c.wreck_visual_active and not c.is_visible() for c in effects)
            weapon=target.get_component_by_class(unreal.ShipWeaponComponent);weapon.burning_seconds=90
            if capsize:
                hull.set_simulate_physics(False);r=target.get_actor_rotation();r.roll=135;target.set_actor_rotation(r,True)
                assert weapon.die(unreal.ShipDeathReason.CAPSIZED,source)
            else:assert weapon.receive_ship_damage(100,source)
            unreal.SystemLibrary.flush_persistent_debug_lines(w)
            gameplay=unreal.get_default_object(unreal.GameplayStatics.static_class());transform=unreal.Transform()
            camera=gameplay.call_method('BeginDeferredActorSpawnFromClass',args=(w,unreal.CameraActor.static_class(),transform,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
            gameplay.call_method('FinishSpawningActor',args=(camera,transform,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
            unreal.GameplayStatics.get_player_controller(w,0).set_view_target_with_blend(camera,0)
            st['render_diagnostics']=[dict(scale=str(c.get_world_scale()),elements=str(c.get_editor_property('elements')),owner_hidden=target.get_editor_property('hidden'),render_main=c.get_editor_property('render_in_main_pass')) for c in effects]
            st.update(initialized=True,target=target,camera=camera,effects=effects)
        target=st['target'];focus=target.get_actor_location()+unreal.Vector(0,0,2000)
        index=0 if t<13 else 1 if t<21 else 2
        eye=target.get_actor_location()+[unreal.Vector(22000,22000,14000),unreal.Vector(-20000,14000,18000),unreal.Vector(5000,-26000,22000)][index]
        st['camera'].set_actor_location(eye,False,True);st['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye,focus),False)
        if t-st['last']>=1:
            st['last']=t
            st['rows'].append(dict(t=t,view=index,roll=target.get_actor_rotation().roll,effects=[dict(active=c.wreck_visual_active,visible=c.is_visible(),position=[c.get_world_location().x,c.get_world_location().y,c.get_world_location().z],collision=str(c.get_collision_enabled())) for c in st['effects']]))
        for name,at in [('A',8),('B',16),('C',26)]:
            if t>=at and name not in st['shots']:
                unreal.SystemLibrary.execute_console_command(w,'Shot filename='+str(root/'Docs/NavalCombat'/(prefix+'_'+name+'.png')));st['shots'].append(name)
        if t>=32:
            st['checks']=dict(hidden_while_alive=st['hidden_while_alive'],five_active_visible_after_death=all(r['active'] and r['visible'] for row in st['rows'] for r in row['effects']),no_collision=all('NO_COLLISION' in r['collision'] for row in st['rows'] for r in row['effects']))
            finish('PROTOTYPE_FUNCTIONAL_PASS' if all(st['checks'].values()) else 'ASSERTION_FAILURE')
    except Exception:st['error']=traceback.format_exc();finish('PROBE_ERROR')

assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
