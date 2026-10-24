# QuangNinh naval collision repair — validation in progress

Branch: `feature/quangninh-battle-integration`, based on merged `28b7f7b`. Main and Thanh remain unchanged. No merge, reset, plugin-source patch or source-dataset edit.

## Root cause

All five ships had valid geographic routes and intact controllers, but the restored terrain supported their deep collision hulls at roughly 7 m root height before their pontoons entered the ocean. Actual downward traces identified Cesium sea-level collision. An identical fresh PIE run with only terrain interaction temporarily isolated restored water contact and physical propulsion. This isolation was diagnostic only.

Evidence: `GROUNDING_DIAGNOSTIC.json`, `COLLISION_BASELINE.json`, `COLLISION_ISOLATED.json`. Those initial diagnostic runs were background-throttled; later acceptance runs use unattended editor automation without focus throttling. No time dilation, continuous teleport or velocity override is used by the correction.

## Preserved map and coordinate frame

The original URLs remain on the teammate's terrain/imagery actors, resolved through a safe compatibility junction. See `LOCAL_SETUP.md`. The dataset contains 919 quantized-mesh terrain tiles, 1110 PNG imagery tiles and two metadata files. All 2031 source hashes are unchanged (`DATASET_INTEGRITY.json`).

Main's origin remains (107.426513 longitude, 20.944219 latitude, 0 m). One georeference and one inherited anchor per ship resolve coherently. Coordinate round trips were below 0.000001 cm and target geographic/world conversion errors below 0.000001 m. The existing five AI, target identities and PlayerStart are saved in the adjacent bay within 3.3 km; origin itself is land. Planned routes clear the original coast polygons by at least 250 m. No global map shift was performed.

Multiple enabled shift components found after the original snapshot were set to Disabled, retaining the components/anchors. This compact fixed frame keeps the existing unanchored flat ocean aligned. Water remains at its authored world Z=300 cm. Gravity-direction deviation across 3.3 km is about 0.03 degrees.

[Official Cesium guidance](https://cesium.com/learn/unreal/unreal-placing-objects/) explains that culled tiles have no physical collision. Frustum/fog culling are disabled for this physics-critical tileset. The visible terrain and original source geometry are preserved. `Bay_patrol_PIE.png` shows the real rendered bay; it includes temporary debug overlays. The supplied imagery appears pale/topographic; no replacement imagery was downloaded.

## Selective collision implementation

`USeaTerrainCollisionComponent`, attached only to QuangNinh_Terrain, reads UE 5.6 Cesium `TriMeshGeometries`, not discarded GPU buffers. It derives invisible runtime collision from the actual original triangles. The final configuration removes only near-zero elevation surfaces (within 0.05 m) and their artificial vertical skirts in confirmed chart-water cells. A skirt is identified by a near-zero top vertex duplicated at the same longitude/latitude at a lower height. Other nonzero submerged geometry is retained. Original coast/land exclusions and out-of-chart geometry remain solid. Ambiguous zero-surface triangles are clipped along the existing chart boundary by interpolation on their original triangle plane.

A full tile-header inventory found 149 tiles with negative minima, down to -85.19 m (`TERRAIN_HEIGHT_INVENTORY.json`). These are not assumed to be invalid or certified bathymetry. The earlier 0.5 m candidate removed submerged geometry more broadly; the final filter was narrowed before publication to preserve those possible underwater surfaces. The final candidate's initial runtime telemetry retained 1104 nonzero submerged triangles while all five ships cruised near 19 m/s.

Filtering runs after Cesium tile selection and before physics. Collision is cooked synchronously before replacement. Land collision is retained independently of render-tile visibility/unload until match teardown; identical returned geometry reuses its proxy. This avoids a demonstrated LOD support hole. Cached LODs form a conservative union, so collision height may exceed a currently rendered lower LOD. This costs additional physics memory. No ship mesh, force graph, buoyancy, damage or A* algorithm was replaced. Navigation recognizes verified filtered open-water coverage rather than treating an intentionally absent sea-level floor as missing data.

## Runtime evidence

| Check | Type | Result/evidence |
|---|---|---|
| Native build and seven battle Blueprints | Compilation | Passed, including final 0.05 m filter |
| Geometry / RealOSM planner cases | Native automation | Both passed; run separately from PIE |
| 12 island drop probes, camera rotation and LOD changes | PIE physics | Passed five assertions: `LAND_COLLISION_PIE.json` (0.5 m candidate) |
| Actual possessed ship driven at an island | PIE original input + physics | Passed four assertions; positive input held while speed settled near zero: `SHIP_SHORE_COLLISION_PIE.json` (0.5 m candidate) |
| Five independent B/A/B patrols | PIE physics, 403 samples over 405.9 s | All five completed three arrivals, 3193–3662 m each, 15 valid switches, no failed/recovery states or land entries. `PATROL_0P5_BASELINE_SUMMARY.json` and compressed full telemetry preserve this candidate |
| Controls, firing, aiming, minimap, Q/F, possession | PIE functional | 20/20 passed: `../NavalCombat/QUANGNINH_INTEGRATION_PIE.json` (0.5 m candidate) |
| Small/Large 8/10/8 damage and guards | PIE real original projectiles | 33/33 each; airborne isolated hull-impact fixture: `QUANGNINH_SMALL_HULL_DAMAGE.json`, `QUANGNINH_LARGE_HULL_DAMAGE.json` under ../NavalCombat |
| Small/Large acceleration, turning, flotation | PIE real possessed-player input | Both passed: `QUANGNINH_PROFILE_*_PHYSICS.json`; 150/240 kg, acceleration 42.45/27.82 m/s², turn peaks 2.60/0.24 deg/s. Arcade placeholder differentiation, not realistic hydrodynamics |
| Final 0.05 m filter / submerged-geometry preservation | Saved fresh PIE | Full patrol recheck running; final acceptance pending |

The baseline soak measured cruise medians 18.5–18.9 m/s and moving-turn medians 12.5–13.5 m/s; the 180-degree turn-in-place phase is slower. Three isolated one-second water-contact misses occurred at wave crests (root heights about 4.6 m). No two consecutive sampled misses occurred; each ship retained water contact for over 99% of settled samples. The summary preserves these observations rather than claiming constant immersion.

A gentle-shore diagnostic showed physical beaching/sliding under sustained propulsion, not ghosting; a steep-island fixture then demonstrated the hull being physically stopped. AI tracks stayed in valid water. Bathymetric accuracy and arbitrary uncharted submerged hazards are not established by this land/coast dataset.

## Diagnostic corrections and remaining work

- A premature editor tick before BeginPlay caused an assertion during early development. Runtime-world/initialized-state guards and explicit editor-tick exclusion corrected it.
- Horizontal constraints were added to island drop probes so they test support rather than sliding off slopes. The subsequent real LOD hole is preserved in `LAND_COLLISION_LOD_FAILURE.json` and fixed by retained land collision.
- Editor-only planner automation stopped one PIE soak; the complete soak was rerun separately. An earlier editor closure also interrupted a run. Neither interrupted run is claimed as a pass.
- Repeated PIE exposed a test-harness bug: a global iterator selected an old input subsystem. Fixtures now resolve the subsystem from the active PlayerController; the 20-check rerun passed.
- Accuracy grading, tactical regression, visible destruction VFX, automatic teammate spectator, optional turns and pre-match configuration remain to be completed/verified after final collision acceptance. Existing sinking and gameplay systems are preserved.
