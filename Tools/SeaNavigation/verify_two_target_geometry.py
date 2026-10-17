"""Independent GIS audit of the saved destinations and current accepted grid.

Reference A* is a test oracle only; gameplay uses FSeaNavigationMap.
"""
import base64
import json
import math
import numpy as np
import shapely
from shapely.geometry import shape, Point, LineString
from common import ROOT, metric_geometry
from validate_navigation_map import find_path

read=lambda p:json.loads(p.read_text(encoding="utf8"))
chart=read(ROOT/"Content/SeaNavigation/halong_navigation.json")
run=read(ROOT/"Docs/ShipEnemyNavigation/TWO_TARGET_PIE.json")
saved=read(ROOT/"Docs/ShipEnemyNavigation/TWO_TARGET_EDITOR_ORIGINAL.json")
grid=np.frombuffer(base64.b64decode(chart["cells_base64"]),dtype=np.uint8).reshape(chart["height"],chart["width"])
land=metric_geometry(shape(read(ROOT/"Data/SeaNavigation/halong_land.geojson")["features"][0]["geometry"]))
buffered=shape(read(ROOT/"Data/SeaNavigation/halong_buffered_enu.geojson")["geometry"])
cell=lambda p:(math.floor((p[0]-chart["min_east"])/chart["cell_size"]),math.floor((p[1]-chart["min_north"])/chart["cell_size"]))
center=lambda c:(chart["min_east"]+(c[0]+.5)*chart["cell_size"],chart["min_north"]+(c[1]+.5)*chart["cell_size"])
ship=saved["BP_ShipEnemy"]["world"]
# Saved level's georeference is the fixed chart origin (X east, Y south, cm).
points={"ship":[ship[0]/100,-ship[1]/100],"A":run["rows"][0]["target_a_en"],"B":run["rows"][0]["target_b_en"]}
routes={}
for a,b in [("ship","A"),("A","B"),("B","A")]:
    path=find_path(grid,cell(points[a]),cell(points[b]),chart["cell_size"])
    assert path,"No reference route for "+a+"->"+b
    line=LineString([points[a]]+[center(c) for c in path]+[points[b]])
    routes[a+"->"+b]=dict(raw_points=len(path),length_m=line.length,buffer_intersection=line.intersects(buffered),
        coast_clearance_m=line.distance(land),direct_segment_crosses_buffer=LineString([points[a],points[b]]).intersects(buffered))
half=chart["cell_size"]/2
centers=np.array([center((int(x),int(y))) for y,x in np.argwhere(grid)])
boxes=shapely.box(centers[:,0]-half,centers[:,1]-half,centers[:,0]+half,centers[:,1]+half)
intersections=int(np.count_nonzero(shapely.intersects(boxes,buffered)))
report=dict(method="Independent GIS/reference search; not physical PIE acceptance",safety_margin_m=chart["safety_margin"],
    hull_radius_m=80.66128851625078,local_margin_m=30,accepted_cells=len(centers),accepted_cell_buffer_intersections=intersections,
    targets={k:dict(en_m=p,coast_clearance_m=Point(p).distance(land),water=bool(grid[cell(p)[1],cell(p)[0]])) for k,p in points.items()},
    routes=routes,depth="UNKNOWN")
assert intersections==0 and all(not r["buffer_intersection"] for r in routes.values())
(ROOT/"Docs/ShipEnemyNavigation/TWO_TARGET_GEOMETRY.json").write_text(json.dumps(report,indent=2),encoding="utf8")
print(json.dumps(report,indent=2))
