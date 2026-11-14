"""Run with -run=pythonscript in a separate commandlet. Never saves assets."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Levels/MainMapLevel')
r=dict(level=str(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()))
for name,fn in [('navigation_graph',unreal.ShipEnemyNavigationComponent.validate_installed_blueprint),('movement_graphs',unreal.ShipEnemyNavigationComponent.inspect_movement_blueprints),('small_mesh_and_original_skeleton',unreal.ShipMovementComponent.inspect_archetype_sources)]:
 try:
  value=fn()
  try:value=json.loads(value)
  except (ValueError,TypeError):pass
  r[name]=value
 except Exception as e:r[name]=dict(error=str(e))
r['projectile_debug_graph']=unreal.ShipCombatLibrary.inspect_impact_debug(False)
r['profiles']=[]
for name in ['Small','Medium','Large']:
 p=unreal.load_asset('/Game/NavalBattle/Profiles/DA_'+name+'Ship')
 row=dict(asset=p.get_path_name())
 for field in ['hull_class','mass_kg','forward_max_speed','reverse_max_speed','cruise_speed','heave_damping_per_second','lateral_damping_per_second','roll_damping_per_second','pitch_damping_per_second','max_health','base_projectile_damage','static_visual_mesh','gun_yaw_mesh','gun_pitch_mesh','inertia_scale']:
  row[field]=str(p.get_editor_property(field))
 r['profiles'].append(row)
r['input_mappings']=[dict(action=m.action.get_name(),key=str(unreal.InputLibrary.key_get_display_name(m.key))) for m in unreal.load_asset('/Game/Ship/Inputs/IMC_Ship').get_editor_property('mappings')]
r['level_ships']=[]
r['terrain_sources']=[]
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if actor.get_class().get_name()=='Cesium3DTileset':
  row=dict(label=actor.get_actor_label(),url=str(actor.get_editor_property('url')),overlays=[])
  for c in actor.get_components_by_class(unreal.ActorComponent):
   if 'RasterOverlay' in c.get_class().get_name():
    try:row['overlays'].append(dict(type=c.get_class().get_name(),url=str(c.get_editor_property('url'))))
    except Exception:pass
  r['terrain_sources'].append(row)
 n=actor.get_component_by_class(unreal.ShipEnemyNavigationComponent)
 if n:
  row=dict(label=actor.get_actor_label(),class_path=actor.get_class().get_path_name(),transform=str(actor.get_actor_transform()),components=[dict(name=c.get_name(),type=c.get_class().get_name()) for c in actor.get_components_by_class(unreal.ActorComponent)])
  for field in ['patrol_actor_a','patrol_actor_b','arrival_radius_m','patrol_wait_seconds','new_navigation_enabled']:
   try:row[field]=str(n.get_editor_property(field))
   except Exception as e:row[field]='unavailable: '+str(e)
  r['level_ships'].append(row)
(root/'reports/research/UNREAL_INSPECTION.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8')
print('Read-only inspection saved; ships',len(r['level_ships']))
