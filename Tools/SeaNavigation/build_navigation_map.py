"""Conservative whole-cell raster of buffered land in fixed ENU meters."""
import argparse
import base64
import hashlib
import math
import numpy as np
import shapely
from shapely.geometry import box, shape, mapping
from PIL import Image
from common import ROOT, ORIGIN, O, E, N, metric_geometry, write_json, read_json
from build_unbuffered_coast import add_unbuffered_coast


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--cell-size',type=float,default=100.)
    p.add_argument('--safety-margin',type=float,default=200.,help='Total exclusion: hull radius + shoreline/calibration margin, meters')
    p.add_argument('--half-extent',type=float,default=20000.)
    args=p.parse_args()
    if args.cell_size<=0 or args.safety_margin<0 or args.half_extent>25000:raise ValueError('Invalid regional grid configuration')
    source=ROOT/'Data/SeaNavigation/halong_land.geojson'
    collection=read_json(source)
    land=metric_geometry(shape(collection['features'][0]['geometry']))
    bounds=box(-args.half_extent,-args.half_extent,args.half_extent,args.half_extent)
    # The source extent must surround every cell AND its exclusion buffer.
    source_bbox=collection['features'][0]['properties']['bbox_lon_lat']
    coverage=metric_geometry(box(*source_bbox))
    if not coverage.contains(bounds.buffer(args.safety_margin+args.cell_size)):
        raise RuntimeError('OSM clip does not fully cover requested grid and safety buffer')
    buffered=shapely.make_valid(land.buffer(args.safety_margin))
    size=math.ceil(args.half_extent*2/args.cell_size)
    xs=-args.half_extent+np.arange(size)*args.cell_size
    ys=-args.half_extent+np.arange(size)*args.cell_size
    xx,yy=np.meshgrid(xs,ys)
    cells=shapely.box(xx,yy,xx+args.cell_size,yy+args.cell_size)
    shapely.prepare(buffered)
    water=np.logical_not(shapely.intersects(buffered,cells))
    # Block dataset boundary: out-of-coverage must never become navigable.
    water[[0,-1],:]=False;water[:,[0,-1]]=False
    centers=shapely.points(xx+args.cell_size/2,yy+args.cell_size/2)
    clearance=np.clip(shapely.distance(centers,buffered)/args.cell_size,0,255).astype(np.uint8)
    metadata={'format':'VDT_SEA_NAV','version':1,'coordinate_system':'WGS84_ECEF_FIXED_ENU_METERS',
              'origin_llh':ORIGIN,'origin_ecef':O.tolist(),'east_axis':E.tolist(),'north_axis':N.tolist(),
              'min_east':-args.half_extent,'min_north':-args.half_extent,'cell_size':args.cell_size,
              'width':size,'height':size,'safety_margin':args.safety_margin,'max_vessel_radius_m':125.,
              'water_cells':int(water.sum()),'blocked_cells':int((~water).sum()),
              'cells_base64':base64.b64encode(water.astype(np.uint8).tobytes()).decode(),
              'clearance_base64':base64.b64encode(clearance.tobytes()).decode(),
              'source_clip_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
              'source_manifest':read_json(ROOT/'Data/SeaNavigation/source_manifest.json'),
              'safety_status':'OSM_COASTLINE_ONLY_DEPTH_UNKNOWN',
              'height_convention':'Coastlines projected at ellipsoid h=0; route EN coordinates are horizontal only. No bathymetry.',
              'attribution':'© OpenStreetMap contributors; https://www.openstreetmap.org/copyright; ODbL 1.0'}
    add_unbuffered_coast(metadata)
    write_json(ROOT/'Content/SeaNavigation/halong_navigation.json',metadata)
    write_json(ROOT/'Data/SeaNavigation/halong_buffered_enu.geojson',{'type':'Feature','properties':{'units':'meters','crs':'fixed_ENU','margin_m':args.safety_margin},'geometry':mapping(buffered.intersection(bounds))})
    pixels=np.zeros((size,size,3),dtype=np.uint8);pixels[:]=[75,90,55];pixels[water]=[25,100,165]
    Image.fromarray(np.flipud(pixels)).resize((1200,1200)).save(ROOT/'Data/SeaNavigation/navigation_grid.png')
    print({k:v for k,v in metadata.items() if not k.endswith('base64') and k not in ('source_manifest','unbuffered_coast')})
    print('Regenerate sector data after changing the navigation chart: Tools/NavalCombat/build_sector_battlefield.py')


if __name__=='__main__':main()
