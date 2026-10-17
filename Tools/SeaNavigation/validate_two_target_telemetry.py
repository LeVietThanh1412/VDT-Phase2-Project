"""Independent GIS checks of observed native routes and physical hull tracks."""
import base64
import json
import math
from pathlib import Path
import numpy as np
import shapely
from shapely.geometry import shape, Point, LineString
from common import ROOT, metric_geometry

read=lambda p:json.loads(Path(p).read_text(encoding="utf8"))
run=read(ROOT/"Docs/ShipEnemyNavigation/TWO_TARGET_PIE.json")
chart=read(ROOT/"Content/SeaNavigation/halong_navigation.json")
land=metric_geometry(shape(read(ROOT/"Data/SeaNavigation/halong_land.geojson")["features"][0]["geometry"]))
buffered=shape(read(ROOT/"Data/SeaNavigation/halong_buffered_enu.geojson")["geometry"])
cells=np.frombuffer(base64.b64decode(chart["cells_base64"]),dtype=np.uint8).reshape(chart["height"],chart["width"])
def water(p):
    x=math.floor((p[0]-chart["min_east"])/chart["cell_size"])
    y=math.floor((p[1]-chart["min_north"])/chart["cell_size"])
    return 0<=x<chart["width"] and 0<=y<chart["height"] and bool(cells[y,x])
rows=run["rows"]
track=LineString([r["en_m"] for r in rows])
routes=[LineString([p[:2] for p in r["points"]]) for r in run["routes"]]
targets={name:rows[0]["target_"+name.lower()+"_en"] for name in ["A","B"]}
report=dict(status=run["status"],simulation_time_s=rows[-1]["t"]-rows[0]["t"],time_dilation=run["time_dilation"],
    samples=len(rows),observed_route_generations=run["observed_route_generations"],
    completed_legs=rows[-1]["completed_legs"]-run["initial_legs"],arrival_events=run["events"],
    maximum_recovery_counter=max(r["recovery_attempts"] for r in rows),
    failed_state_samples=sum(r["state"]=="<ShipNavigationState.FAILED: 10>" for r in rows),
    minimum_distance_m=run["minimum_distance_m"],maximum_speed_mps=max(r["speed_mps"] for r in rows),
    hull_ecef_displacement_m=math.dist(rows[0]["ecef"],rows[-1]["ecef"]),
    physical_track_length_m=track.length,physical_track_land_intersection=track.intersects(land),
    physical_track_exclusion_intersection=track.intersects(buffered),minimum_track_coast_clearance_m=track.distance(land),
    physical_samples_in_blocked_cells=sum(not water(r["en_m"]) for r in rows),
    route_land_intersections=sum(p.intersects(land) for p in routes),route_buffer_intersections=sum(p.intersects(buffered) for p in routes),
    commands_match_blueprint=all(r["throttle"]==r["bp_throttle"] and r["rudder"]==r["bp_rudder"] for r in rows),
    targets={name:dict(en_m=p,water=water(p),coast_clearance_m=Point(p).distance(land)) for name,p in targets.items()},
    failures=[dict(t=r["t"],reason=r["failure"]) for r in rows if r["state"]=="<ShipNavigationState.FAILED: 10>"],
    bathymetry="UNKNOWN; supplied map contains coastline exclusions, not a depth survey")
(ROOT/"Docs/ShipEnemyNavigation/TWO_TARGET_RUNTIME_VALIDATION.json").write_text(json.dumps(report,indent=2),encoding="utf8")
print(json.dumps(report,indent=2))
