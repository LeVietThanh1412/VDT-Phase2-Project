"""Restore existing five patrol pawns beside their real validated A/B targets.

Preserves map/georeference, actor identities, controller classes and target
references. Requires the external-actor recovery recorded before this edit.
"""
import json,math,base64,contextlib,io
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
assert (root/'Saved/active_demo_position_recovery.txt').exists()
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
plan=json.loads((root/'Docs/QuangNinhIntegration/ORIGIN_PATROL_PLAN.json').read_text())
chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text());cells=base64.b64decode(chart['cells_base64'])
def accepted(en):
    x=math.floor((en[0]-chart['min_east'])/chart['cell_size']);y=math.floor((en[1]-chart['min_north'])/chart['cell_size'])
    return 0<=x<chart['width'] and 0<=y<chart['height'] and cells[y*chart['width']+x]==1
assert all(accepted(s['spawn_en_m']) for s in plan['ships']) and accepted(plan['player_en_m'])
assert all(accepted(en) for en in plan['targets'].values())
unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs()])
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();lookup={a.get_actor_label():a for a in actors}
geo=next(a for a in actors if isinstance(a,unreal.CesiumGeoreference));changed=[];before=[]
def place(a,llh):
    a.modify();a.set_editor_property('is_spatially_loaded',False);a.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    anchor=a.get_component_by_class(unreal.CesiumGlobeAnchorComponent)
    if anchor:anchor.move_to_longitude_latitude_height(unreal.Vector(*llh))
    else:a.set_actor_location(geo.transform_longitude_latitude_height_position_to_unreal(unreal.Vector(*llh)),False,True)
    changed.append(a)
for name,llh in plan['target_llh'].items():place(lookup[name],llh)
for spec in plan['ships']:
    a=lookup[spec['label']];n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
    before.append(dict(label=a.get_actor_label(),path=a.get_path_name(),controller_class=a.get_editor_property('ai_controller_class').get_path_name(),a=n.patrol_actor_a.get_path_name(),b=n.patrol_actor_b.get_path_name()))
    assert n.patrol_actor_a==lookup[spec['a']] and n.patrol_actor_b==lookup[spec['b']]
    place(a,spec['spawn_llh']);delta=lookup[spec['b']].get_actor_location()-a.get_actor_location()
    a.set_actor_rotation(unreal.Rotator(yaw=math.degrees(math.atan2(delta.y,delta.x))),True)
    a.get_component_by_class(unreal.CesiumGlobeAnchorComponent).sync()
    n.set_editor_property('new_navigation_enabled',True);n.set_editor_property('navigation_mode',unreal.ShipNavigationMode.PATROL)
    n.set_editor_property('start_at_patrol_b',True);n.set_editor_property('patrol_offset_a',unreal.Vector());n.set_editor_property('patrol_offset_b',unreal.Vector())
    n.set_editor_property('patrol_wait_seconds',3);n.set_editor_property('arrival_radius_m',80)
    a.set_editor_property('DestinationActor',n.patrol_actor_b)
    assert a.get_editor_property('bAIEnabled') and a.get_editor_property('auto_possess_player')==unreal.AutoReceiveInput.DISABLED
place(lookup['PlayerStart'],plan['player_llh']);lookup['PlayerStart'].set_actor_rotation(unreal.Rotator(yaw=-90),True)
anchor=lookup['PlayerStart'].get_component_by_class(unreal.CesiumGlobeAnchorComponent)
if anchor:anchor.sync()
if 'TP_ShipDestination' in lookup:
    llh=list(plan['player_llh']);llh[2]=3;place(lookup['TP_ShipDestination'],llh)
for record in before:
    a=lookup[record['label']];n=a.get_component_by_class(unreal.ShipEnemyNavigationComponent)
    assert a.get_path_name()==record['path'] and a.get_editor_property('ai_controller_class').get_path_name()==record['controller_class']
    assert n.patrol_actor_a.get_path_name()==record['a'] and n.patrol_actor_b.get_path_name()==record['b']
assert unreal.ShipEnemyNavigationComponent.validate_installed_blueprint().startswith('OK')
assert unreal.EditorLoadingAndSavingUtils.save_packages(list({a.get_package() for a in changed}),False)
(root/'Docs/NavalCombat/DEMO_POSITION_REPAIR.json').write_text(json.dumps(dict(status='SAVED_VALIDATED_CHART_POSITIONS',recovery=(root/'Saved/active_demo_position_recovery.txt').read_text(),preserved=before,changed=[a.get_actor_label() for a in changed],runtime_loop_acceptance='PENDING'),indent=2),encoding='utf-8')
scope=dict(OUTPUT='DEMO_POSITION_AUDIT_AFTER.json',__name__='demo_position_after');p=root/'Tools/NavalCombat/audit_current_patrol.py'
with contextlib.redirect_stdout(io.StringIO()):exec(compile(p.read_text(),str(p),'exec'),scope)
print('Saved five existing AI, their ten A/B targets, PlayerStart and the optional legacy marker. References/controllers preserved; full real-physics loop test follows.')
