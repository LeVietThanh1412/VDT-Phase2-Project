"""Clip official OSM land polygons in an offline cache; never import global data into UE."""
import argparse
import hashlib
import pathlib
import urllib.request
import zipfile
import shapefile
from shapely import make_valid
from shapely.geometry import box, shape, mapping
from shapely.ops import unary_union
from common import ROOT, ORIGIN, write_json

URL = 'https://osmdata.openstreetmap.de/download/land-polygons-split-4326.zip'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=pathlib.Path, default=ROOT/'Saved/SeaNavigationTooling/land-polygons-split-4326.zip')
    parser.add_argument('--bbox', type=float, nargs=4, default=[107.05, 20.75, 107.55, 21.23], metavar=('W','S','E','N'))
    args = parser.parse_args()
    cache = args.source.parent/'land-polygons-split-4326'
    cache.mkdir(parents=True, exist_ok=True)
    if not args.source.exists():
        print('Downloading official global source into offline tooling cache only', flush=True)
        urllib.request.urlretrieve(URL, args.source)
    with zipfile.ZipFile(args.source) as archive:
        for member in archive.infolist():
            if pathlib.Path(member.filename).suffix.lower() in ['.shp','.shx','.dbf','.prj']:
                target = cache/pathlib.Path(member.filename).name
                if not target.exists() or target.stat().st_size != member.file_size:
                    print('Extracting',target.name,member.file_size,flush=True)
                    with archive.open(member) as src, target.open('wb') as out:
                        import shutil
                        shutil.copyfileobj(src,out,4*1024*1024)
    shp = next(cache.glob('*.shp'))
    reader = shapefile.Reader(str(shp))
    clip = box(*args.bbox)
    geometries = []
    repaired = 0
    for item in reader.iterShapes(bbox=args.bbox):
        geometry = shape(item.__geo_interface__)
        if not geometry.is_valid:
            geometry = make_valid(geometry); repaired += 1
        if not geometry.is_empty:
            geometry = geometry.intersection(clip)
            if not geometry.is_empty: geometries.append(geometry)
    if not geometries:
        raise RuntimeError('No genuine OSM polygons found in region; refusing to fabricate water')
    merged = make_valid(unary_union(geometries))  # merges split overlaps, retains genuine holes
    sha=hashlib.file_digest(args.source.open('rb'),'sha256').hexdigest()
    properties={'source_url':URL,'source_sha256':sha,'source_bytes':args.source.stat().st_size,
                'source_mtime_utc':__import__('datetime').datetime.fromtimestamp(args.source.stat().st_mtime,__import__('datetime').timezone.utc).isoformat(),
                'bbox_lon_lat':args.bbox,'origin_llh':ORIGIN,'input_polygon_parts':len(geometries),
                'repaired_inputs':repaired,'valid':merged.is_valid,
                'attribution':'© OpenStreetMap contributors; ODbL 1.0; https://www.openstreetmap.org/copyright',
                'bathymetry':'UNKNOWN: coastline is not a depth chart'}
    write_json(ROOT/'Data/SeaNavigation/halong_land.geojson',{'type':'FeatureCollection','features':[
        {'type':'Feature','properties':properties,'geometry':mapping(merged)}]})
    write_json(ROOT/'Data/SeaNavigation/source_manifest.json',properties)
    print('Clipped real OSM land',properties,flush=True)


if __name__ == '__main__': main()
