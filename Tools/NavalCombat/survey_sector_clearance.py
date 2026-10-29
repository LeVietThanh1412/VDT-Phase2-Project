"""Read-only physical clearance survey in a held diagnostic PIE world."""
import json,math
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
grid=lib.call_method('GetWorldSubsystem',args=(w,unreal.NavalSectorSubsystem.static_class()))
fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
ignore=fleet+list(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.WaterBody))
ocean=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.WaterBodyOcean)[0].get_water_body_component()
geo=unreal.CesiumGeoreference.get_default_georeference(w)
profiles={}
for a in fleet:
    m=a.get_component_by_class(unreal.ShipMovementComponent);h=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_name()=='ShipSM')
    profiles[str(m.hull_class)]=(h.get_local_bounds(),h.get_world_scale())
result=dict(scope='Physical turn clearance at each navigable sector anchor, actual hull + 2 m at queried ocean waterline; no ship moved',profiles={},rows=[],terrain=[])
for cls,(bounds,scale) in profiles.items():
    lo,hi=bounds;center=(lo+hi)*.5*scale;ext=(hi-lo)*.5*scale+unreal.Vector(200,200,200)
    radius=math.hypot(max(abs(lo.x),abs(hi.x))*scale.x,max(abs(lo.y),abs(hi.y))*scale.y)+3000
    ext.x=ext.y=radius
    result['profiles'][cls]=dict(bounds=[list(v.to_tuple()) for v in bounds],extent_cm=list(ext.to_tuple()),safe_sectors=[])
    for s in grid.sectors:
        if not s.navigable:continue
        p=grid.chart_to_world(s.anchor_en);p.z=ocean.get_water_surface_info_at_location(p,False)[0].z
        c=p+center
        hit=unreal.SystemLibrary.box_trace_single(w,c,c+unreal.Vector(0,0,.01),ext,unreal.Rotator(),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignore,unreal.DrawDebugTrace.NONE,True)
        row=dict(hull_class=cls,sector=str(s.id),en_m=list(s.anchor_en.to_tuple()),clear=hit is None)
        if hit:
            ht=hit.to_tuple();component=ht[10];row.update(component=component.get_path_name() if component else None,hit_llh=list(geo.transform_unreal_position_to_longitude_latitude_height(ht[5]).to_tuple()))
        else:result['profiles'][cls]['safe_sectors'].append(str(s.id))
        result['rows'].append(row)
for terrain in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Cesium3DTileset):
    if terrain.get_component_by_class(unreal.SeaTerrainCollisionComponent):
        result['terrain'].append(dict(label=terrain.get_actor_label(),load_progress=terrain.get_editor_property('load_progress'),components=[dict(name=c.get_name(),visible=c.is_visible(),collision=str(c.get_collision_enabled()),location=list(c.get_world_location().to_tuple())) for c in terrain.get_components_by_class(unreal.StaticMeshComponent)]))
(root/'Saved/SECTOR_OPERATIONAL_CLEARANCE_SURVEY.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps({k:len(v['safe_sectors']) for k,v in result['profiles'].items()}))
