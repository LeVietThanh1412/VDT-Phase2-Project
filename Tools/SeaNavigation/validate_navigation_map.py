"""Independent Shapely certification of grid/route safety, plus reproducible real-data A*."""
import base64
import heapq
import math
from collections import deque
import numpy as np
import shapely
from shapely.geometry import shape, LineString
from PIL import Image, ImageDraw
from common import ROOT, read_json, write_json, ll_to_en, en_to_ll


def find_path(grid, start, goal, cell=100.):
    h,w=grid.shape
    def valid(x,y):return 0<=x<w and 0<=y<h and grid[y,x]
    if not valid(*start) or not valid(*goal):return None
    q=[(0.,0.,start)];g={start:0.};parent={};expanded=0
    while q:
        _,cost,current=heapq.heappop(q)
        if cost!=g[current]:continue
        expanded+=1
        if expanded>100000:raise RuntimeError('Python reference search expansion bound')
        if current==goal:
            route=[goal]
            while route[-1]!=start:route.append(parent[route[-1]])
            return route[::-1]
        x,y=current
        for dx,dy in [(1,0),(-1,0),(0,1),(0,-1),(1,1),(1,-1),(-1,1),(-1,-1)]:
            n=x+dx,y+dy
            if not valid(*n) or dx and dy and (not valid(x+dx,y) or not valid(x,y+dy)):continue
            ng=cost+math.hypot(dx,dy)
            if ng<g.get(n,math.inf):
                g[n]=ng;parent[n]=current;heapq.heappush(q,(ng+math.dist(n,goal),ng,n))
    return None


def main():
    j=read_json(ROOT/'Content/SeaNavigation/halong_navigation.json')
    w,h=j['width'],j['height'];size=j['cell_size']
    grid=np.frombuffer(base64.b64decode(j['cells_base64']),dtype=np.uint8).reshape(h,w)
    land=shape(read_json(ROOT/'Data/SeaNavigation/halong_buffered_enu.geojson')['geometry'])
    def center(p):return (j['min_east']+(p[0]+.5)*size,j['min_north']+(p[1]+.5)*size)
    def cell(p):return int((p[0]-j['min_east'])/size),int((p[1]-j['min_north'])/size)
    a=tuple(map(float,ll_to_en(107.29702857184641,20.975025161990075)))
    original_b=tuple(map(float,ll_to_en(107.29214425167515,20.983569683709124)))
    start=cell(a)
    if not grid[start[1],start[0]]:raise RuntimeError('Existing enemy start is blocked; do not silently relocate')
    # Select a new NAVIGATION DEFAULT only; original TargetPoint and map actor stay untouched.
    choices=sorted([(math.dist(center((x,y)),original_b),(x,y)) for y,x in np.argwhere(grid)],key=lambda x:x[0])
    destination=None
    for _,b in choices[:100]:
        route=find_path(grid,start,b)
        if route and len(route)>3:destination=b;break
    if destination is None:raise RuntimeError('No connected nearby safe patrol B')
    B=center(destination);lon,lat=en_to_ll(*B)
    write_json(ROOT/'Data/SeaNavigation/patrol_defaults.json',{
        'patrol_a_llh':[107.29702857184641,20.975025161990075,0.],
        'patrol_b_llh':[float(lon),float(lat),0.],
        'patrol_a_en_m':a,'patrol_b_en_m':B,
        'original_target_llh':[107.29214425167515,20.983569683709124,0.],
        'original_target_blocked':not bool(grid[cell(original_b)[1],cell(original_b)[0]]),
        'new_component_default_offset_from_original_target_m':math.dist(B,original_b),
        'map_actor_modified':False})
    # Independent geometry test: every accepted cardinal/diagonal neighbor edge clears buffered land.
    edges=0
    for dx,dy in [(1,0),(0,1),(1,1),(-1,1)]:
        starts=[];ends=[]
        for y,x in np.argwhere(grid):
            nx,ny=x+dx,y+dy
            if nx<0 or ny<0 or nx>=w or ny>=h or not grid[ny,nx]:continue
            if dx and dy and (not grid[y,nx] or not grid[ny,x]):continue
            starts.append(center((x,y)));ends.append(center((nx,ny)))
        if starts:
            lines=shapely.linestrings(np.stack([starts,ends],axis=1))
            if np.any(shapely.intersects(land,lines)):raise AssertionError('Accepted edge crosses buffered geographic land')
            edges+=len(lines)
    examples=[('existing_enemy_to_safe_patrol_b',start,destination)]
    rng=np.random.default_rng(20261017);water=np.argwhere(grid)
    for _ in range(500):
        p=water[rng.integers(len(water))];q=water[rng.integers(len(water))]
        s=(int(p[1]),int(p[0]));g=(int(q[1]),int(q[0]))
        distance=math.dist(center(s),center(g))
        if not 1500<distance<12000:continue
        if not land.intersects(LineString([center(s),center(g)])):continue
        r=find_path(grid,s,g)
        if r:
            examples.append(('island_detour_'+str(len(examples)),s,g))
            if len(examples)>=5:break
    if len(examples)<2:raise AssertionError('No real island detour found')
    results=[]
    image=Image.open(ROOT/'Data/SeaNavigation/navigation_grid.png');draw=ImageDraw.Draw(image)
    for name,s,g in examples:
        r=find_path(grid,s,g)
        assert r
        points=[center(p) for p in r]
        assert not land.intersects(LineString(points))
        results.append({'name':name,'start_en_m':center(s),'goal_en_m':center(g),'raw_path_points':len(r),
                        'length_m':sum(math.dist(points[i-1],points[i]) for i in range(1,len(points))),
                        'straight_crosses_buffered_land':land.intersects(LineString([points[0],points[-1]])),'route_en_m':points})
        draw.line([((x+.5)/w*1200,1200-(y+.5)/h*1200) for x,y in r],fill=(255,220,50),width=3)
    image.save(ROOT/'Data/SeaNavigation/validated_routes.png')
    # CRS axis/order regression and local-chart round trip.
    max_error=0
    for x,y in [a,B,(-12000.,7000.),(10000.,-10000.)]:
        lo,la=en_to_ll(x,y);ex,ny=ll_to_en(lo,la)
        # EN->tangent ECEF->ellipsoid LLH->ENU has a tiny radial projection difference.
        max_error=max(max_error,math.dist((x,y),(float(ex),float(ny))))
    assert max_error<0.1, max_error
    report={'status':'IMPLEMENTED_AND_TESTED','grid_cells':w*h,'water_cells':int(grid.sum()),
            'accepted_edges_certified_against_buffered_OSM':edges,'coordinate_round_trip_max_error_m':max_error,
            'original_target_is_blocked':not bool(grid[cell(original_b)[1],cell(original_b)[0]]),
            'examples':results,'bathymetry':'UNKNOWN','cesium_alignment':'REQUIRES_EDITOR_COLLISION_SAMPLING'}
    write_json(ROOT/'Data/SeaNavigation/validation_report.json',report)
    print({k:v for k,v in report.items() if k!='examples'})
    print('PATROL_DEFAULTS',read_json(ROOT/'Data/SeaNavigation/patrol_defaults.json'))


if __name__=='__main__':main()
