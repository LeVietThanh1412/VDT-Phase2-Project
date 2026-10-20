# Main map and Thanh naval integration

The merge already in progress was preserved: Thanh `45a2aa6` + main `f17ccef`.
`backup/Thanh-before-merge` still points to the original Thanh commit. The later user instruction explicitly requests publishing the integrated result into main.

## Conflict decisions

| Conflict | Identity and decision |
| --- | --- |
| `.gitignore` | Combine Thanh's TextBP/navigation cache exclusions and main's Python bytecode exclusions. |
| `.ignore` | Union both directory exclusion lists, including both MCP plugin intermediate directories. |
| `Content/Levels/MainMapLevel.umap` | Main baseline, retained byte-for-byte. SHA-256 `f7c8dbd001454d45abf22e85d2233c3230a3c54afc0007d5f8564ac180a82aef`. Keeps main's level script that creates the minimap. |
| `Content/Ship/BluePrint/BP_Ship.uasset` | Thanh gameplay baseline. Preserve its force filters, weapon entry, animation/aim, buoyancy, health and physics components. Add main's disabled CesiumOriginShift component. |
| ExternalActor `5/KI/KQB0BSM8A71UVZST3A1BWS` | WaterZone, GUID `11AAC4A54756901A90201C8DC263CF9A`. Main baseline: origin (0,0,0), identity rotation/scale; 8,000,000 cm ZoneExtent, 5,000,000 cm far mesh. Thanh had expanded these to 21,000,000 / 4,000,000 cm. Main's zone still covers the naval chart footprint. |
| ExternalActor `6/S6/QHE8Y6YCXVAR4IRIEALSEA` | Old PlayerStart, GUID `13E6946444CED675A853638686088F07`, main position (-848320,238700,3950) cm. Retain Thanh's intentional deletion. Its geographically anchored replacement is GUID `1FE83FE74383B18DB62E52B6FBD7BF17`; retain exactly one start. |
| ExternalActor `7/L6/7T25OG97O148W27EMK8JRX` | WaterBodyOcean, GUID `CD5DC56D40B80C1FD028488F518AB376`. Main baseline: (0,0,300) cm, identity rotation/scale, main coastline/spline, 8,000,000 cm ocean extents and existing Custom_Water_Material_Ocean/Custom_Waves_Ocean references. Thanh had expanded extents to 11,200,000 cm. Actual merged-scene buoyancy passed. |

The three conflicted external actors are native engine actors, owned by MainMapLevel.PersistentLevel, with no actor attachments. WaterZone contains WaterMesh/bounds; Ocean contains water spline, ocean component, collision and water-info meshes. Complete components, transforms, references and both branch exports are recorded in the adjacent JSON evidence.

## Retained features

Main's nine environment actors are retained, including QuangNinh_Terrain, Cesium geography, water, sky, camera manager and World Partition minimap. Main's replacement offline tileset remains authoritative; the legacy online tileset deleted by main remains deleted. The old main PlayerStart is replaced by Thanh's naval start.

Thanh's 19 naval actors are retained: five AI ships, their target points, the naval PlayerStart, SeaGraph and AntiShip setup. Their saved positions, roles, teams, zero patrol offsets, target actor references, timing, mass and damping match the pre-merge audit (position error below 1 cm). Five actor packages were resaved with the integrated inherited component layout, preserving their GUIDs and naval properties. GMBP spawns exactly one manual player ship at runtime.

Thanh's movement, A* chart/planner, single drive-command authority, combat, original projectiles, animation, profiles, damage, capsize and sinking implementations remain present. Their core movement/combat C++ was not replaced by main.

Main's minimap widgets/textures, FreeCameraPawn, CustomPlayerController, input mappings, GMBP and FunplayMCP are retained. Q now changes the view target instead of possessing another ship; F temporarily possesses the free camera and restores the original player. Five AI controllers remain independent. WBP_MainMinimap retains its authored layout, projection and timer and inherits a small native adapter for team colors and removing dead markers.

VisualStudioTools was disabled because it is unavailable on this installation. Water is explicitly enabled because the gameplay module depends on it. Optional NNERuntimeORT was disabled only on the editor launch command, not in project configuration.

## Validation

- C++ Development Editor build succeeded, including reflected code generation and final DLL link. Existing compiler/plugin deprecation warnings remain; see build.log.
- BP_Ship, BP_ShipEnemy, both original projectiles, WBP_ShipHUD, WBP_ShipHealthBar and GMBP explicitly compile; the preserved enemy graph passes its native structural validator. ABP_Ship and both minimap widgets also explicitly compile and run.
- Map check: 0 errors, 0 warnings. World Partition loads 28 actors with 28 distinct GUIDs and exactly one PlayerStart.
- Planner.Geometry and Planner.RealOSM automation tests both succeed.
- Repeated actual PIE / editor restarts: 20 integration checks pass, including physical manual/AI movement, correct possession, actual original manual firing, GunCamera activation, all five A* paths, route endpoints matching actual TargetPoints, camera changes, six moving markers, faction colors and death-marker removal.
- Small hull collision: 31 checks pass. Large hull collision: 33 checks pass. Real original projectiles cause bow/midship/stern damage 8/10/8, rotated hull damage works, friendly damage is zero, duplicate/self/unknown impacts are rejected, and lethal damage reports one death.
- These collision fixtures are raised 1 km above the sea to isolate projectile/hull collision from the map's water surface. The initial unraised fixture missed its target; its failure is retained separately and is not presented as a passed test.
- Destruction: 51 checks pass for health/capsize deaths, retained burning wrecks, active Niagara components, progressive 65 m geographic sinking, a georeference change during sinking, cleanup and stable battle counters. This verifies effect activation, not final artistic approval of fire/smoke visibility.
- The earlier full five-ship B/A/B patrol soak belongs to the Thanh baseline. A full 27-minute soak was not repeated for this merge; current validation covers saved route preservation, actual movement, planner geometry and exact endpoint coordinates.

## Existing external dependency and limitations

Main refers to files outside Git:

- `E:/UnrealTerrainSet/QuangNinh/Terrain/layer.json`
- `E:/UnrealTerrainSet/QuangNinh/Imagery/tilemapresource.xml`

Both are missing on this machine, and no local copy was found. Their authored map references are preserved. Water, ships, physics, UI and georeferencing run, but terrain/island rendering cannot be validated here until the dataset is installed. Cesium reports these missing-source errors; the pre-existing Cesium metadata-enum initialization error and water quad-tree warning also remain. This report does not claim a complete environment rendering pass.

## Git LFS

The four reported non-pointer files were the four conflicted LFS pointer files containing `<<<<<<<` / `=======` / `>>>>>>>` text. Exact pre-edit bytes are retained under Saved/MergeRecovery_20261020_180631/working. All their stage 1/2/3 originals are valid LFS pointers. Valid package versions were materialized through the existing LFS store; no binary text merge was used.

The selected packages have Unreal package magic `c1832a9e` and pass a byte-identical LFS clean/smudge round trip. Normal selective staging stores LFS pointers. Attributes and history are unchanged. There are also 250 legacy raw asset blobs common to both branch histories; those unrelated assets were not migrated.

## Publication

Publication hashes and final ref/status verification are recorded in publication.json after completion. Remote main will be advanced only by a normal non-force push containing both original histories. No existing PR is closed or recreated.
