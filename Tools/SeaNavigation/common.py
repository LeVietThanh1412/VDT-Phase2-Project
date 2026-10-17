"""WGS84 ellipsoid -> fixed local ENU chart. All navigation distances are meters."""
import json
import math
import pathlib
import numpy as np
from pyproj import Transformer
from shapely.ops import transform

ROOT = pathlib.Path(__file__).resolve().parents[2]
ORIGIN = [107.296416, 20.992597, 0.0]  # longitude degrees, latitude degrees, ellipsoid meters
TO_ECEF = Transformer.from_crs(4979, 4978, always_xy=True)
FROM_ECEF = Transformer.from_crs(4978, 4979, always_xy=True)
O = np.array(TO_ECEF.transform(*ORIGIN))
lon, lat = map(math.radians, ORIGIN[:2])
E = np.array([-math.sin(lon), math.cos(lon), 0.0])
N = np.array([-math.sin(lat)*math.cos(lon), -math.sin(lat)*math.sin(lon), math.cos(lat)])
U = np.cross(E, N)


def ll_to_en(x, y, z=None):
    xyz = np.array(TO_ECEF.transform(x, y, np.zeros_like(x, dtype=float)))
    delta = xyz - O.reshape((3,) + (1,)*(xyz.ndim-1))
    return tuple(np.tensordot(axis, delta, axes=1) for axis in (E, N))


def en_to_ll(x, y, z=None):
    xyz = O.reshape((3,) + (1,)*np.ndim(x)) + np.multiply.outer(E, x) + np.multiply.outer(N, y)
    lon, lat, _ = FROM_ECEF.transform(*xyz)
    return lon, lat


def metric_geometry(geometry):
    return transform(ll_to_en, geometry)


def read_json(path):
    return json.loads(pathlib.Path(path).read_text(encoding='utf-8'))


def write_json(path, value):
    path = pathlib.Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, separators=(',', ':'), allow_nan=False)+'\n', encoding='utf-8')
