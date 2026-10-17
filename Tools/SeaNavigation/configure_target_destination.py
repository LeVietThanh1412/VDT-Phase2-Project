# Editor script enabling the existing enemy's A* TargetPoint example.
# Preserves the original target, ship transforms, parent Blueprint and physics.
# Only saves the active enemy Blueprint, enemy instance and new target packages.
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None, "Stop PIE before configuring editor assets"
actors_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actors_api.get_all_level_actors()
enemy = next(a for a in actors if a.get_class().get_name() == "BP_ShipEnemy_C")
nav = enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
assert nav, "Existing enemy navigation component is missing"
anchor = enemy.get_component_by_class(unreal.CesiumGlobeAnchorComponent)
geo = anchor.resolve_georeference()
assert geo, "Existing ship anchor has no georeference"
original = next((a for a in actors if a.get_actor_label() == "TP_ShipDestination"), None)
defaults = json.loads((root / "Data/SeaNavigation/patrol_defaults.json").read_text())
llh = unreal.Vector(*defaults["patrol_b_llh"])
target_label = "TP_ShipEnemy_AStarDestination"
target = next((a for a in actors if a.get_actor_label() == target_label), None)
if target is None:
    # Existing scene has no rebasing or actor attachment; use its existing georeference.
    assert enemy.get_attach_parent_actor() is None, "Resolve attached ship frame before placing target"
    target = actors_api.spawn_actor_from_class(
        unreal.TargetPoint, geo.transform_longitude_latitude_height_position_to_unreal(llh))
    assert target, "Could not create the example TargetPoint"
    target.set_actor_label(target_label)
    target.set_editor_property("tags", ["ShipEnemyAStarExample"])

bp = unreal.load_asset("/Game/ShipEnemy/BluePrint/BP_ShipEnemy")
subobjects = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
# Keep the new marker fixed on the globe when a runtime origin shift occurs.
target.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
if target.get_component_by_class(unreal.CesiumGlobeAnchorComponent) is None:
    actor_handles = subobjects.k2_gather_subobject_data_for_instance(target)
    actor_handle = next(h for h in actor_handles if
        unreal.SubobjectDataBlueprintFunctionLibrary.get_object(
            unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)) == target)
    anchor_handle, fail_reason = subobjects.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=actor_handle, new_class=unreal.CesiumGlobeAnchorComponent))
    assert not str(fail_reason), str(fail_reason)
    assert target.get_component_by_class(unreal.CesiumGlobeAnchorComponent), "Target anchor creation failed"
templates = []
for handle in subobjects.k2_gather_subobject_data_for_blueprint(bp):
    data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
    obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
    if isinstance(obj, unreal.ShipEnemyNavigationComponent):
        templates.append(obj)
assert len(templates) == 1, "Expected exactly one enemy navigation template"
templates[0].set_editor_property("new_navigation_enabled", True)
templates[0].set_editor_property("navigation_mode", unreal.ShipNavigationMode.DESTINATION)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
validation = unreal.ShipEnemyNavigationComponent.validate_installed_blueprint()
assert validation.startswith("OK:"), validation

# Compilation can reconstruct instance components. Resolve it again before editing.
nav = enemy.get_component_by_class(unreal.ShipEnemyNavigationComponent)
enemy.set_editor_property("DestinationActor", target)
assert enemy.get_editor_property("bAIEnabled"), "Existing enemy AI must be enabled"
nav.set_editor_property("new_navigation_enabled", True)
nav.set_editor_property("navigation_mode", unreal.ShipNavigationMode.DESTINATION)
nav.set_editor_property("role", unreal.ShipEnemyRole.PASSIVE)
nav.set_editor_property("draw_debug", True)
packages = [bp.get_package(), enemy.get_package(), target.get_package()]
assert unreal.EditorLoadingAndSavingUtils.save_packages(packages, False), "Package save failed"
print(json.dumps({
    "validation": validation,
    "enemy": enemy.get_path_name(),
    "destination": target.get_actor_label(),
    "previous_destination_preserved": original.get_actor_label() if original else None,
    "target_llh": defaults["patrol_b_llh"],
    "enabled": nav.get_editor_property("new_navigation_enabled"),
    "mode": str(nav.get_editor_property("navigation_mode")),
    "saved_packages": [p.get_name() for p in packages],
}))
