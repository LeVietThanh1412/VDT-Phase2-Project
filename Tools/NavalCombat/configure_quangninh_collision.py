"""Persist selective DEM collision and the compact battlefield's fixed frame."""
import unreal
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs()])
actors=api.get_all_level_actors();terrain=next(a for a in actors if a.get_actor_label()=='QuangNinh_Terrain')
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
if not terrain.get_component_by_class(unreal.SeaTerrainCollisionComponent):
    handles=sub.k2_gather_subobject_data_for_instance(terrain)
    parent=next(h for h in handles if unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h))==terrain)
    _,error=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.SeaTerrainCollisionComponent))
    assert not str(error),str(error)
assert len(terrain.get_components_by_class(unreal.SeaTerrainCollisionComponent))==1
filter=terrain.get_component_by_class(unreal.SeaTerrainCollisionComponent)
filter.modify();filter.set_editor_property('sea_level_tolerance_m',.05)
# Cesium's documented view-dependent culling removes physical islands behind
# the camera. This compact 3.3 km fleet needs collision from every direction.
terrain.modify();terrain.set_editor_property('enable_frustum_culling',False)
terrain.set_editor_property('enable_fog_culling',False)
changed=[terrain]
bp=unreal.load_asset('/Game/Ship/BluePrint/BP_Ship')
for h in sub.k2_gather_subobject_data_for_blueprint(bp):
    c=unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h))
    if isinstance(c,unreal.CesiumOriginShiftComponent):c.modify();c.set_editor_property('mode',unreal.CesiumOriginShiftMode.DISABLED)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
# Keep all existing anchors. ResolveGeoreference already selects the single
# correct actor. Avoid automatic rebases of the unanchored flat Water system.
for a in api.get_all_level_actors():
    for c in a.get_components_by_class(unreal.CesiumOriginShiftComponent):
        a.modify();c.modify();c.set_editor_property('mode',unreal.CesiumOriginShiftMode.DISABLED);changed.append(a)
assert unreal.ShipEnemyNavigationComponent.validate_installed_blueprint().startswith('OK:')
print(unreal.ShipEnemyNavigationComponent.compile_battle_blueprints())
assert unreal.EditorLoadingAndSavingUtils.save_packages(list({a.get_package() for a in changed}|{bp.get_package()}),False)
print('Saved selective terrain collision, retained anchors and one fixed local frame.')
