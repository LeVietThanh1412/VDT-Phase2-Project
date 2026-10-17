"""Read-only Cesium round trips and streamed collision samples at real OSM coastline normals."""
import json
import math
import numpy as np
import shapely
from shapely.geometry import shape, Point
from common import ROOT, read_json, write_json, metric_geometry, en_to_ll
from mcp_client import call_tool

land=metric_geometry(shape(read_json(ROOT/'Data/SeaNavigation/halong_land.geojson')['features'][0]['geometry']))
boundary=land.boundary
lines=list(boundary.geoms) if hasattr(boundary,'geoms') else [boundary]
samples=[]
for line in lines:
    coords=list(line.coords)
    for i in range(0,len(coords)-1,max(1,len(coords)//30)):
        p=np.array(coords[i]);q=np.array(coords[i+1]);d=q-p
        if np.linalg.norm(p-np.array([64,-1945]))>4000 or np.linalg.norm(d)<0.01:continue
        n=np.array([-d[1],d[0]])/np.linalg.norm(d)
        if land.covers(Point(*(p+n*80))):n=-n
        if land.covers(Point(*(p+n*80))) or not land.covers(Point(*(p-n*80))):continue
        samples.append((p,n))
samples=samples[::max(1,len(samples)//16)][:16]
queries=[]
for i,(p,n) in enumerate(samples):
    for offset in [-300,-200,-100,0,100,200,300]:
        e,no=p+n*offset;lo,la=en_to_ll(float(e),float(no))
        queries.append([i,offset,float(lo),float(la)])
code='import unreal,json,math,re\nqueries='+repr(queries)+'\n'+r'''
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
g=unreal.GameplayStatics.get_actor_of_class(w,unreal.CesiumGeoreference)
ignore=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor) if a.get_class().get_name().startswith('Water') or 'Ship' in a.get_class().get_name()]
rows=[]
for i,offset,lon,lat in queries:
    a=g.transform_longitude_latitude_height_position_to_unreal(unreal.Vector(lon,lat,500))
    b=g.transform_longitude_latitude_height_position_to_unreal(unreal.Vector(lon,lat,-500))
    hit=unreal.SystemLibrary.line_trace_single(w,a,b,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignore,unreal.DrawDebugTrace.NONE)
    elevation=None
    if hit is not None:
        text=hit.export_text()
        match=re.search(r'(?:^|,)Location=\(X=([^,]+),Y=([^,]+),Z=([^\)]+)\)',text)
        if match:
            location=unreal.Vector(*[float(x) for x in match.groups()])
            elevation=g.transform_unreal_position_to_longitude_latitude_height(location).z
    rows.append({'coastline_sample':i,'offset_toward_osm_water_m':offset,'lon':lon,'lat':lat,'terrain_ellipsoid_height_m':elevation})
errors=[]
for p in [unreal.Vector(6370,194550,680),unreal.Vector(-44418.397924,99947.030647,0),unreal.Vector(1000000,-1000000,5000),unreal.Vector(-1500000,1200000,-200)]:
    llh=g.transform_unreal_position_to_longitude_latitude_height(p)
    back=g.transform_longitude_latitude_height_position_to_unreal(llh)
    errors.append(math.sqrt((back.x-p.x)**2+(back.y-p.y)**2+(back.z-p.z)**2))
print(json.dumps({'samples':rows,'round_trip_errors_cm':errors}))
'''
r=call_tool('python/execute',{'code':code})
if not r['success']:raise RuntimeError(r['stderr'])
report=json.loads(r['stdout'])
observed=[x for x in report['samples'] if x['terrain_ellipsoid_height_m'] is not None]
violations=[x for x in observed if x['offset_toward_osm_water_m']>=200 and x['terrain_ellipsoid_height_m']>5]
report.update({'coastline_normals':len(samples),'query_count':len(queries),'collision_hits':len(observed),
               'missing_collision_is_unknown':True,'water_side_high_terrain_samples':violations,
               'global_alignment_verified':False,
               'interpretation':'Streamed samples only; ellipsoid heights are not MSL. Missing tiles and low terrain are not proof of safe water. Full regional alignment remains a PIE acceptance item.'})
write_json(ROOT/'Data/SeaNavigation/cesium_alignment_report.json',report)
print({k:v for k,v in report.items() if k!='samples'})
