# Geographic data pipeline

## Source and provenance

The actual source is the official OSM-derived split WGS84 land-polygon Shapefile:
https://osmdata.openstreetmap.de/download/land-polygons-split-4326.zip

Source details and known coastline processing limitations:
https://osmdata.openstreetmap.de/data/land-polygons.html

The downloaded source is 928,236,051 bytes with SHA-256 `6a2018a619460c97d00460f02a709bc2eb63db07866588978e857c6ad1e0dd48`. The global archive/extracted files are in ignored `Saved/SeaNavigationTooling`, outside Unreal Content. Only the regional clip, reproducible tooling and small runtime grid are delivered. `source_mtime_utc` records the local cached download's file timestamp, not a guarantee of underlying OSM edit freshness.

The selected clip is longitude 107.05–107.55°, latitude 20.75–21.23°. It contains 1,515 source parts; none required geometry repair in this run. Split overlaps are unioned, interior holes preserved, and polygon validity is checked. A missing source or empty clip fails explicitly rather than fabricating islands or water.

## Coordinate convention

Input/output geographic order is **longitude, latitude, ellipsoid height**. Angles are degrees unless explicitly converted to radians for axes. WGS84 ECEF is EPSG:4978, geographic input is EPSG:4979/4326 with PyProj `always_xy=True`.

The fixed chart origin is `(107.296416°,20.992597°,0 m)`:

```
ECEF origin = (-1771213.049345848, 5687963.5195040535, 2270629.8024308244) meters
East  = (-0.9547793992065426, -0.29731515072527565, 0)
North = ( 0.10651235644378607,-0.342047162566027,0.9336267222532364)
E,N = dot(ECEF - origin, East/North)
```

Runtime uses the same axes/origin stored in the versioned JSON. A route point is fixed EN meters; it is not Cesium-local or Unreal-world centimeters. For steering it is embedded in the fixed tangent chart and transformed through the current Cesium frame. Height does not participate in A* or arrival. Actual ship height, buoyancy, waves, gravity and force calculation remain in the existing physics system. Neither OSM's zero-height projection nor ellipsoid height is an MSL or bathymetric measurement.

## Raster and safety

The 40×40 km grid contains 160,000 cells: 41,161 coastline-clearance candidate water cells and 118,839 blocked cells. The cell size is 100 m. Cells that touch buffered land are blocked, and the outer grid border is blocked. Coverage is validated to extend beyond the requested grid and exclusion buffer. Missing/out-of-coverage regions cannot become water through runtime endpoint snapping.

The 200 m exclusion is **total hull plus shoreline/calibration clearance**, for hull radius at most 125 m. It is adjustable by rebuilding the data. Whole-cell rasterization adds conservative discretization, which can close narrow passages. The original TargetPoint is blocked under this policy. It was not relocated; `patrol_defaults.json` records an alternative connected water candidate 710.09 m from it for the new component's initial B field.

`build_navigation_map.py` exports:

- `Content/SeaNavigation/halong_navigation.json`: ~428 KB, version 1, axes/origin, dimensions, blocked/water and quantized clearance payloads, source hash, attribution and explicit unknown-depth status.
- `Data/SeaNavigation/halong_land.geojson`: clipped genuine OSM polygons.
- `Data/SeaNavigation/halong_buffered_enu.geojson`: diagnostic buffered geometry in EN meters, not standard lon/lat GeoJSON coordinates; its properties identify the custom chart.
- `navigation_grid.png`, `validated_routes.png`, source/validation/patrol manifests.

The Build.cs UFS runtime dependency stages only the runtime JSON. The game target receipt was checked and contains that dependency. No global source or network service is required during gameplay. A full packaged-game cook/run is separate from this receipt/build check.

## Executed geographic validation

The independent Shapely validator checked **147,563 accepted neighbor edges** against buffered land. All cleared it. Five real-data island-detour examples were routed by a Python reference search; native Unreal A* independently routes the same examples and reverse legs and validates each smoothed segment. The normal direct line crosses buffered land in all five examples, so these are actual detours rather than fabricated successful straight routes.

The fixed-chart geographic round trip measured a maximum horizontal projection difference of 0.03494 m at the selected regional samples. Actual installed Cesium Unreal→geographic→Unreal conversions measured a maximum 0.00002385 cm error at four positions.

Read-only streamed collision sampling used 16 real coastline normals and 112 vertical queries around the ship area. Twenty queries hit terrain. No observed high-terrain hit was 200 m or farther onto the expected water side. This does **not** provide a regional coastline error bound: 92 missing queries remain unknown, and ellipsoid height is not MSL. Full visual/physical alignment, narrow channels and unobserved regions remain final gameplay acceptance items. See `cesium_alignment_report.json` for actual samples, not a blanket alignment claim.

## Reproduction and attribution

Commands and dependency pins are in `Tools/SeaNavigation/README.md`. Regenerate after changing map region, coverage or safety settings; validate again before enabling ships. Do not silently substitute alternate CRS coordinates or a prototype SeaGraph.

All derived geographic files retain © OpenStreetMap contributors / ODbL 1.0 attribution. Distributions must preserve attribution and applicable database licensing obligations. See `Data/SeaNavigation/ATTRIBUTION.md`.
