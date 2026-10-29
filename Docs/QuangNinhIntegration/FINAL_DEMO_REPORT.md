# QuangNinh naval battle demo report

**Overall status: PARTIAL - not yet certified for the complete sector battle demo.**

Authoritative continuation: sector defense/assault task supplied on 2026-10-27.
Branch: feature/small-warship-seeded-battlefield. Main and teammate branches are preserved.

## Current verification

| Requirement | Actual status |
| --- | --- |
| Textured Small boat, separate player/AI Blueprints, fitted physical hull and working bow gun | Previously verified and pushed at 9f2bb3f; existing assets retained |
| Reproduce current first-arrival stop | Reproduced in current PIE with camera/LOD changes |
| Root cause | Artificial Cesium sea skirts missed by former 5 cm vertical-pair heuristic |
| Collision correction | Compiled; live metadata counters confirm generated skirts filtered, land/seabed retained |
| Five independent B/A/B loops after correction | PASS: fresh 392.8 s, all five B/A/B, exact endpoints/controllers/physics; NAVIGATION_FIXED_SAVED_PATROL.json |
| Development reposition recovery | PASS: reset/replan and physical B/A return, 242.15 s, targets/controllers preserved |
| Saved editor placement after restart | PASS: correct saved water position, >100 m real displacement, targets/controllers preserved |
| Island detour in selected field | PASS: native four-waypoint route, 2159 m physical B/A traversal, no land intersection |
| Retained island collision across LOD changes | PASS: physical island support, LOD changes and bounded cache |
| Fixed sector grid | PASS: candidate 1, 625 logical sectors, native connectivity and coordinates verified in PIE |
| Reproducible physical spawning | PASS for startup: 11 PIE starts, 10 distinct seeds, exact replay; longer seeded patrol and full battle pending |
| Blue defense / Red Small scout formation assault | Pending |
| Synchronized team/role/class tags | Pending |
| Measured buoyancy improvements across all classes | Pending |
| Shared ten-round magazine / 30-second reload / HUD | Pending |
| Multi-seed full combat, turns, spectator, saved restart | Pending |
| Final verified commits / PR | Small, navigation 68be150 and sector ea5cd87 pushed; draft PR #11 open |

## Fixed battlefield reference

Use SEEDED_FIELD_CANDIDATE_1.png, centered at chart EN (14500,3500) meters, width/height 2500 m, yaw 0. The image is 1000x1000 with 3000 m context; the white battlefield rectangle occupies pixels 83.333 to 916.667. Normalized battlefield coordinates map as East=13250+2500*u, North=4750-2500*v. Blue is left, Red right. These are fixed-chart meters, not Unreal centimeters. No map/terrain/georeference relocation is authorized or needed.

## Recovery and limitations

Before edits, 86 source/config/Blueprint/map actor files were SHA256 verified into the recovery path recorded by Saved/active_navigation_recovery.txt. Seven pre-existing binary modifications were retained. Friendly_AI_1's invalid A target was corrected individually to previously validated water.

The old 600-second failed run remains failure evidence. New compact reports distinguish actual physical arrivals from path-planning/compilation checks. Full transient samples stay in Saved. Generated collision skirts are filtered only over chart-certified water; source terrain files, real islands and nonzero seabed remain intact.

The selected field has no certified bathymetry dataset. WaterBodyOcean fallback depth is not proof of real-world depth. Final spawn acceptance must check physical underwater clearance and state that operational limitation explicitly.

Do not present the existing six visible ships, historical Small tests, compilation or isolated successful routes as proof of a completed sector tactics/ammunition battle.


## Current seeded startup evidence

SEEDED_SPAWN_SUITE.json passed eleven independent real menu/travel starts, ten distinct seeds and exact replay equality. Every run kept one player/five original AIControllers, all physical ships in water at acceptance, valid initial per-hull geographic A* routes, separate Blue patrol goals, correct fixed-field team halves and actual movement by all five AI. This is startup/initial travel evidence; it does not certify assault completion or a natural fighting battle.

The subsequent 125-second LAND_COLLISION_OPERATIONAL_LOD.json passed all nine actual island-support and retained-submerged-geometry/cache checks. The latest collision split retains coast/island support persistently while nonzero chart-water detail follows selected terrain LOD. Original source terrain and narrow 0.05 m filtering remain intact. The mixed-class and repeated seeded patrol fixtures are still running.

A read-only untuned physics baseline remains SECTOR_PHYSICS_BASELINE.json. Occasional buoyancy contact loss and up to 22.3 degrees roll/5.66 m/s vertical speed remain an explicit tuning task. Sector tactics, standardized tags, magazine/reload, final integrated combat and restart persistence are not yet complete. Development rollout remains disabled by default until integrated acceptance.
