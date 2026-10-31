"""Apply the reviewed new-map plan to existing actors, preserving identities."""
import json,math
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None,'End PIE before configuring the saved level'
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
plan=json.loads((root/'Docs/QuangNinhIntegration/ORIGIN_PATROL_PLAN.json').read_text())
unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs()])
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);lookup={a.get_actor_label():a for a in api.get_all_level_actors()}
assert all(s['label'] in lookup for s in plan['ships']) and all(n in lookup for n in plan['targets'])
geo=next(a for a in lookup.values() if isinstance(a,unreal.CesiumGeoreference));changed=[]
def place(actor,llh):
    actor.modify();actor.set_editor_property('is_spatially_loaded',False)
    actor.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    actor.set_actor_location(geo.transform_longitude_latitude_height_position_to_unreal(unreal.Vector(*llh)),False,True)
    changed.append(actor)
for name,llh in plan['target_llh'].items():place(lookup[name],llh)
for spec in plan['ships']:
    actor=lookup[spec['label']];place(actor,spec['spawn_llh'])
    target=lookup[spec['b']].get_actor_location();delta=target-actor.get_actor_location()
    actor.set_actor_rotation(unreal.Rotator(pitch=0,yaw=math.degrees(math.atan2(delta.y,delta.x)),roll=0),True)
    nav=actor.get_component_by_class(unreal.ShipEnemyNavigationComponent)
    assert nav.patrol_actor_a==lookup[spec['a']] and nav.patrol_actor_b==lookup[spec['b']]
    assert nav.patrol_offset_a.length()<.001 and nav.patrol_offset_b.length()<.001
    assert actor.get_editor_property('bAIEnabled') and actor.get_editor_property('auto_possess_player')==unreal.AutoReceiveInput.DISABLED
    actor.set_editor_property('DestinationActor',lookup[spec['b']])
player=lookup['PlayerStart'];place(player,plan['player_llh']);player.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-90,roll=0),True)
if 'TP_ShipDestination' in lookup:
    # Keep the legacy optional destination in the same bay; the five explicit
    # patrol pairs above remain the only active AI route references.
    llh=list(plan['ships'][1]['spawn_llh']);llh[2]=3;place(lookup['TP_ShipDestination'],llh)
assert unreal.ShipEnemyNavigationComponent.validate_installed_blueprint().startswith('OK:')
assert unreal.EditorLoadingAndSavingUtils.save_packages(list({a.get_package() for a in changed}),False)
print('Saved existing five AI, eleven TargetPoints and PlayerStart near main origin; no actors spawned.')
