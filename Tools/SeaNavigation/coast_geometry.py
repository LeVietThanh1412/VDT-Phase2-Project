"""Read-only coastline measurements without a GEOS/PROJ DLL dependency.

WGS84 geodetic -> ECEF -> the stored immutable EN chart. This is analysis
tooling only; gameplay continues to use its existing native geographic A*.
"""
import json,math
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[2]
class CoastGeometry:
    def __init__(self):
        self.chart=json.loads((ROOT/'Content/SeaNavigation/halong_navigation.json').read_text())
        g=json.loads((ROOT/'Data/SeaNavigation/halong_land.geojson').read_text())['features'][0]['geometry']
        source=g['coordinates'] if g['type']=='MultiPolygon' else [g['coordinates']]
        self.polygons=[];starts=[];ends=[]
        for polygon in source:
            rings=[np.array([self.to_metric(*p[:2]) for p in ring]) for ring in polygon]
            self.polygons.append(rings)
            for ring in rings:starts.extend(ring[:-1]);ends.extend(ring[1:])
        self.a=np.array(starts);self.b=np.array(ends);self.d=self.b-self.a
        self.length2=np.maximum(np.sum(self.d*self.d,axis=1),1e-12)
        self.bounds=[(r[0].min(axis=0),r[0].max(axis=0)) for r in self.polygons]
    def to_metric(self,lon,lat):
        lon,lat=map(math.radians,[lon,lat]);e2=6.6943799901413165e-3
        n=6378137/math.sqrt(1-e2*math.sin(lat)**2)
        ecef=[n*math.cos(lat)*math.cos(lon),n*math.cos(lat)*math.sin(lon),n*(1-e2)*math.sin(lat)]
        delta=[a-b for a,b in zip(ecef,self.chart['origin_ecef'])]
        return [sum(a*b for a,b in zip(delta,self.chart[k])) for k in ['east_axis','north_axis']]
    @staticmethod
    def in_ring(p,ring):
        x,y=p;a=ring[:-1];b=ring[1:]
        crossing=(a[:,1]>y)!=(b[:,1]>y)
        a=a[crossing];b=b[crossing]
        return bool(np.count_nonzero(x<(b[:,0]-a[:,0])*(y-a[:,1])/(b[:,1]-a[:,1])+a[:,0])%2)
    def contains(self,p):
        for rings,(lo,hi) in zip(self.polygons,self.bounds):
            if p[0]<lo[0] or p[1]<lo[1] or p[0]>hi[0] or p[1]>hi[1]:continue
            if self.in_ring(p,rings[0]) and not any(self.in_ring(p,r) for r in rings[1:]):return True
        return False
    def closest_boundary(self,p):
        p=np.array(p);t=np.clip(np.sum((p-self.a)*self.d,axis=1)/self.length2,0,1)
        nearest=self.a+t[:,None]*self.d;distance2=np.sum((p-nearest)**2,axis=1);i=int(np.argmin(distance2))
        return nearest[i].tolist(),math.sqrt(float(distance2[i]))
    def distance(self,p):return 0.0 if self.contains(p) else self.closest_boundary(p)[1]
    def route_distance_bound(self,route,spacing=20):
        # Distance to a closed set is 1-Lipschitz: subtract half sample spacing
        # for a conservative clearance lower bound along each whole segment.
        result=float('inf')
        for a,b in zip(route,route[1:]):
            length=math.dist(a,b);count=max(1,math.ceil(length/spacing));step=length/count
            for i in range(count+1):
                p=[a[k]+(b[k]-a[k])*i/count for k in range(2)]
                result=min(result,self.distance(p)-step/2)
        return max(0,result)
