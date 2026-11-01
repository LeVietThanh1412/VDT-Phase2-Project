# QuangNinh final demo

**DEMO_READY_WITH_LIMITATIONS** (2026-11-01). Production audit and safe refactors R1-R4 are verified independently from the earlier Tools/Docs archive milestone.

Use [DEMO_SETUP](../Demo/DEMO_SETUP.md). The authoritative full results, per-ship tables, regression matrix and failed configurations are in [FINAL_QA_REPORT](../Testing/FINAL_QA_REPORT.md). Source findings and actual changes: [CODEBASE_AUDIT](../Architecture/CODEBASE_AUDIT.md), [REFACTOR_REPORT](../Architecture/REFACTOR_REPORT.md).

## Recommended preset

MainMapLevel, seed20261028, Full AI Demo/spectator,3v3, Real-Time, Normal, Standard100HP/base10 damage, Island defense. Blue Small/Small/Small; Red Small/Large/Medium. Blue patrols the island, Red advances southwest and its Small scout searches genuinely observed last-known positions. Heavy Blue hulls use the eastern arc.

Terrain/imagery remains external under E:/LeVietThanh1412-Product/QuangNinh through E:/UnrealTerrainSet/QuangNinh. Globe frame, original625 spawn sectors, Blueprint paths, meshes and physics profiles are preserved. The600m western extension is navigation-only. Small has a static mesh without a skeleton; dedicated player/AI Blueprints, materials and its fitted gun rig are retained.

## Actual verification

- Four natural3v3 outcomes on incremental builds: Blue568.08s, Blue251.65s, Red605.36s and final production bb9b507 Blue371.89s. Earlier builds are identified separately, not called final-DLL repetitions.
- Final natural match: all6 fire, zero navigation failures, five health-depletion deaths, native BLUE WINS.
- Exact original five-AI B/A/B/A fleet passed667.86s; three Blue Small physical island circuits passed749.64s.
- Cold Editor restart passed all-six contact on seeds20261028/29/30 in15.55/11.54/11.03s, correct team tags and preserved AIControllers. Seed20261028 initial placement/heading/routes replayed exactly.
- Seven additional native physical startups passed; this is not ten completed combat certifications.
- C++,39 Blueprints,4 native tests, player controls,8/10/8 damage, single death counters, cached projectile after shooter cleanup,10-round/30s reload, capsize/sinking,120s alternating turns and120s mixed-class stability passed.
- Controlled300m reverse fire reached-4.51m/s and returned physically to original patrol with its original controller/targets. Controlled and natural cases are distinguished.

## Explicit limits

New-fleet legacy A/B and assault-disabled heavy-convoy reverse-flow stress can stall; use the recommended island-defense preset. The original global minimap scale clusters markers/labels. Bathymetry, packaging, multiplayer and broad platform performance are not certified. Existing Cesium enum/toolchain diagnostics remain. Failed/interrupted cases are retained.

UE was closed after final testing. Main and teammate/recovery data remain preserved; delivery uses the existing feature branch.
