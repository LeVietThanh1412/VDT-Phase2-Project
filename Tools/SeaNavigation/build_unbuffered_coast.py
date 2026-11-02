"""Add a conservative, unbuffered shoreline mask; do not change the A* chart.

The navigation exclusion buffer is not terrain. Rasterizing raw land separately
lets collision filtering remove sea-level DEM without inventing a solid sea rim.
Run with the pythonai Conda interpreter (NumPy/Shapely required).
"""
import base64, hashlib, json
import numpy as np
import shapely
from shapely.geometry import Polygon
from coast_geometry import CoastGeometry, ROOT

def add_unbuffered_coast(data):
    """Attach collision classification while preserving buffered navigation data."""
    source_hash=hashlib.sha256((ROOT/'Data/SeaNavigation/halong_land.geojson').read_bytes()).hexdigest()
    if source_hash!=data['source_clip_sha256']:
        raise ValueError('Coastline changed; rebuild the buffered navigation chart and sectors first')
    coast=CoastGeometry(data)
    land=shapely.union_all([Polygon(r[0],r[1:]) for r in coast.polygons]);shapely.prepare(land)
    cell=25.
    widths=[data['width']*data['cell_size']/cell,data['height']*data['cell_size']/cell]
    if any(abs(v-round(v))>1e-6 or not 2<=round(v)<=4096 for v in widths):
        raise ValueError('Raw coastline requires a whole number of 25 m cells, at most 4096 per axis')
    width,height=map(round,widths)
    water=np.zeros((height,width),dtype=np.uint8);xs=data['min_east']+np.arange(width)*cell
    for first in range(0,height,32):
        ys=data['min_north']+np.arange(first,min(first+32,height))*cell
        xx,yy=np.meshgrid(xs,ys)
        water[first:first+len(ys)]=~shapely.intersects(land,shapely.box(xx,yy,xx+cell,yy+cell))
    water[[0,-1],:]=0;water[:,[0,-1]]=0
    data['unbuffered_coast']=dict(width=width,height=height,cell_size=cell,min_east=data['min_east'],min_north=data['min_north'],cells_base64=base64.b64encode(water.tobytes()).decode(),source_clip_sha256=source_hash,purpose='Raw whole-cell coastline classification; never an A* clearance replacement by itself.')
    print('Raw coastline mask:',width,height,'water cells',int(water.sum()))


if __name__=='__main__':
    path=ROOT/'Content/SeaNavigation/halong_navigation.json'
    previous=path.read_bytes();data=json.loads(previous);original=(data['cells_base64'],data['clearance_base64'])
    add_unbuffered_coast(data)
    assert original==(data['cells_base64'],data['clearance_base64'])
    path.write_text(json.dumps(data,separators=(',',':'))+'\n',encoding='utf-8')
    print('Navigation payload unchanged')
    # Only the optional mask changed: sector geometry still derives from the
    # identical buffered chart/source. Rebind checksums only if the previous
    # byte-level fingerprint matched; genuinely stale sectors need regeneration.
    sector_path=ROOT/'Content/SeaNavigation/quangninh_sectors.json'
    if sector_path.exists():
        sectors=json.loads(sector_path.read_text())
        if all(sectors['navigation_'+algo]==getattr(hashlib,algo)(previous).hexdigest() for algo in ('sha1','sha256')):
            for algo in ('sha1','sha256'):sectors['navigation_'+algo]=getattr(hashlib,algo)(path.read_bytes()).hexdigest()
            sector_path.write_text(json.dumps(sectors,separators=(',',':')),encoding='utf-8')
            qa_path=ROOT/'Docs/QuangNinhIntegration/SECTOR_GRID_QA.json'
            if qa_path.exists():
                qa=json.loads(qa_path.read_text());qa['data_sha256']=hashlib.sha256(sector_path.read_bytes()).hexdigest()
                qa_path.write_text(json.dumps(qa,indent=2),encoding='utf-8')
            print('Sector fingerprints refreshed; sector geometry unchanged')
        else:
            print('Sector data was already stale; regenerate with Tools/NavalCombat/build_sector_battlefield.py')
