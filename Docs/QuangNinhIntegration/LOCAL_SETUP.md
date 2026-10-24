# Local QuangNinh data setup

The saved MainMapLevel uses these original teammate-authored runtime URLs:

- Terrain: `file:///E:/UnrealTerrainSet/QuangNinh/Terrain/layer.json`
- QuangNinh_Imagery component: `file:///E:/UnrealTerrainSet/QuangNinh/Imagery/tilemapresource.xml`

The actual supplied dataset is `E:/LeVietThanh1412-Product/QuangNinh`. It contains 919 quantized-mesh terrain tiles, 1110 PNG imagery tiles and two metadata files. Terrain and imagery share EPSG:4326/TMS geographic bounds. These are external runtime dependencies, not editor-only import sources. Keep the source folder available when running the editor or a local packaged build. It is not bundled into Git or a packaged executable automatically.

Run from the project directory in PowerShell:

```powershell
./Tools/SeaNavigation/setup_quangninh.ps1 -DatasetRoot 'E:/LeVietThanh1412-Product/QuangNinh'
```

For another developer, pass their actual dataset folder instead. The script validates metadata and creates the absent legacy compatibility junction. It refuses an occupied, nonmatching destination or recursive mapping, and never copies, overwrites or reimports source tiles. The default compatibility location requires an E: volume. If that volume is unavailable, choose a valid local compatibility location and configure the terrain actor and imagery component URLs in that developer's local editor state accordingly; avoid committing a personal source path.

The map's URLs and visible terrain are unchanged. The correction is the saved `SeaTerrainCollisionComponent` on QuangNinh_Terrain. The built-in ProceduralMeshComponent plugin is required. Build VDTPhase2ProjectEditor for UE 5.6 before opening the modified level on another machine.

Main's Cesium origin remains longitude 107.426513, latitude 20.944219, height 0. The naval bay lies within 3.3 km. Existing ship/target anchors are retained and automatic origin shifting is Disabled so the flat WaterBodyOcean and its collision remain in this fixed frame. The world origin itself is land. The current five AI, their independent A/B targets and PlayerStart are saved in the adjacent navigable bay.

Terrain frustum and fog culling are disabled for physical coverage around the battle. Cached land collision remains for the match when a render tile changes LOD or unloads. It is a conservative union of actual loaded terrain surfaces, clipped only to remove artificial submerged geometry in navigable chart water. It can differ slightly from a currently rendered coarser LOD and costs additional physics memory. It is rebuilt for each fresh match; no external terrain file is modified.

The provided imagery appears pale/topographic in current screenshots; no satellite color imagery was downloaded or substituted. Dataset integrity is checked in `DATASET_INTEGRITY.json` (all 2031 files unchanged).

Open `/Game/Levels/MainMapLevel` and press Play. Default ships retain the existing 3 Blue (one player, two AI) and 3 Red AI configuration. Current validation status and remaining gameplay work are in `CESIUM_TERRAIN_COLLISION_FIX.md` and `../NavalBattleV2_Progress.md`.
