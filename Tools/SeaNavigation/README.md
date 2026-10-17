# Sea navigation tooling

The active MainMapLevel enemy now uses the two saved Target Point actor references in A-first Patrol mode. Moving either target does not require rebuilding map data or editing geographic patrol defaults. See [the current physical validation report](../../Docs/ShipEnemyNavigation/TWO_TARGET_COMPLETION.md). Rebuild the grid only when coastline/exclusion geometry, coverage or vessel clearance changes.

Requires Python 3.13 on this workstation. Dependencies are isolated under ignored `Saved/SeaNavigationTooling/venv`.

```powershell
python -m venv Saved/SeaNavigationTooling/venv
Saved/SeaNavigationTooling/venv/Scripts/python.exe -m pip install -r Tools/SeaNavigation/requirements.txt
Saved/SeaNavigationTooling/venv/Scripts/python.exe Tools/SeaNavigation/prepare_osm.py
Saved/SeaNavigationTooling/venv/Scripts/python.exe Tools/SeaNavigation/build_navigation_map.py --cell-size 100 --safety-margin 125 --half-extent 20000
Saved/SeaNavigationTooling/venv/Scripts/python.exe Tools/SeaNavigation/validate_navigation_map.py
# Requires running editor/SpecialAgent; reads collision and coordinates only:
Saved/SeaNavigationTooling/venv/Scripts/python.exe Tools/SeaNavigation/inspect_alignment.py
```

`prepare_osm.py --source <official-zip> --bbox W S E N` accepts an existing local official source copy. The ~928 MB global download and extracted Shapefile live only in the ignored offline cache. Neither is imported into Unreal or packaged. The committed clip is ~1.8 MB; the runtime grid is ~428 KB.

Only the compact `Content/SeaNavigation/halong_navigation.json` is loaded by gameplay, through Unreal's UFS file layer. The Build.cs runtime dependency stages it into packaged builds without changing project packaging settings.

The fixed chart uses WGS84 ECEF projected onto East/North axes at longitude 107.296416°, latitude 20.992597°, ellipsoid height 0 m. See `Docs/ShipEnemyNavigation/OSM_DATA_PIPELINE.md`. Do not substitute UTM coordinates or Unreal centimeters.

All water cells are **coastline-clearance candidates with unknown bathymetry**, never certified maritime navigation. Out-of-coverage is blocked. Absence of streamed collision is unknown, not water. Rebuild the data for a different region, larger hull, or larger clearance. The prototype BP_SeaGraph is neither used nor changed.

`validate_navigation_map.py` writes legacy geographic patrol-default candidates to `Data/SeaNavigation/patrol_defaults.json`; it never relocates a Target Point or changes the level. The active placed enemy instead reads its actor references on each new run and tracks live actor movement. `verify_two_target_geometry.py` audits the current saved target evidence and grid without changing those defaults.

`mcp_client.py` serializes direct HTTP requests to this project's SpecialAgent endpoint. It intentionally makes no assumptions about native MCP registration or tools from the separate unreal-mcp plugin.

OSM-derived files: © OpenStreetMap contributors, Open Database License (ODbL) 1.0. Include `Data/SeaNavigation/ATTRIBUTION.md` with distributions and retain attribution in the runtime JSON. Preprocessing source code is part of this project; OSM data has its own license.
