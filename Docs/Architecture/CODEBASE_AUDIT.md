# Production codebase audit

Date:2026-11-01. Source cleanup status: **IN_PROGRESS**, separate from the completed Tools/Docs archive checkpoint.

Baseline: `b9fb0e0`, after a real100HP3v3 match reached BLUE WINS in568.08 game seconds. The natural-match fixture did not change ship positions, targets, aim, damage or health. A separate controlled survival fixture deliberately killed four ships through guarded damage to isolate the Small scout/Blue flagship: contact was lost at52.21s, search began at96.90s, genuine firing-solution reacquisition occurred at399.66s and the two-survivor match ended at532.13s. That controlled case is not counted as a natural3v3 battle.

## Scope and ownership

The module inventory covers50 `.cpp/.h/.cs` files and9035 lines at the baseline, plus the two project/editor target definitions outside the module. The audit covers runtime code, editor Blueprint installers, test-only classes, profiles, native UI and their Blueprint entrypoints. The inventory records file hashes, declarations, includes, Tick implementations and planning/movement calls; assets and runtime call paths are checked before treating an apparent unused method as dead code.

| Area | Current responsibility | Findings / disposition |
|---|---|---|
| `SeaNavigationMap` | Immutable ECEF/ENU chart, connectivity, conservative A*, smoothing and budgets | Connectivity flood-fill is duplicated in the operational survey. Consolidate this exact algorithm; preserve neighbor order/corner rules and pathfinding. |
| `ShipEnemyNavigationComponent` | Sole final throttle/rudder command writer; live A/B/circuit goals, avoidance, recovery and retreat steering |1241-line implementation is complex. Keep the verified arrival and steering algorithms; remove header coupling and consolidate identical water-query exclusions instead of rewriting A*. |
| `NavalBattlefieldSubsystem` | Prematch survey/placement plus defense, convoy, observed-contact memory and scout search intents |929-line file mixes setup/collision work with tactical state transitions. Separate mission method definitions while retaining the same subsystem/API/state. No new manager or movement controller. |
| `ShipCombatComponent` / weapon | Target/LOS/ballistics, shared dispatch/magazine, guarded damage/death |841-line file holds several related native classes. Death now directly notifies the authoritative battle subsystem once; UI delegates remain presentation. Keep the public Blueprint classes and original fire graph. |
| `NavalBattleUI` | Authoritative roster, alive counters, result and alternating turns; health/minimap presentation bases | A duplicate death is rejected by both the weapon death guard and the subsystem's fallen set. Full AI gets a sixth production AI pawn/controller and a separate camera pawn. Rebuilding the existing minimap once includes the late sixth ship. |
| Physics/profile/equipment | Mass, buoyancy parameters, force filtering, visual/gun attachment | No independent AddForce or ordinary hull teleport is introduced. `bProfileApplied` appears write-only in C++ but is a public Blueprint-readable status; retain it. Profile assets and buoyancy supports remain unchanged. |
| `SeaTerrainCollisionComponent` | Dataset-specific sea-plane/skirt filtering, retained coast/collision proxies and coverage | Source meshes are never mutated. Weak source/proxy references, rebind-on-reload, reference-counted visibility and EndPlay restoration are present. Per-tick mesh enumeration/temporary maps are profiling candidates, not automatically safe to cache across Cesium streaming. |
| Projectile lifecycle | Cached source affiliation/damage and one-victim claims | Shared real BP rounds remain the six-ship path. The separate native legacy projectile ignores navigation pawns and uses its old radial model only when shared combat is absent; it is conditional compatibility code, not a second active damage path. Do not delete it or alter demo damage to match it. |
| Controller / HUD / spectator | Player input, non-possessing Q inspection, overview, selected-ship HUD, death spectator and setup | Full AI camera never possesses a ship. Current lifecycle is tested; preserve normal player mode, player-team swapping and10-second turns. Large controller/UI files remain technical debt rather than grounds for an untested rewrite. |
| Editor integration | Blueprint graph patches, profiles, small static hull/gun/material setup and validation | Editor dependencies are conditional in Build.cs; development helpers retain public callable names used by tools/assets. No ordinary filesystem moves/renames of Unreal packages. |
| Tags / identity | Team() authority with descriptive Actor.Tags and display names | Tags do not independently assign a team. Faction, display name and tag refresh during roster setup remain distinct by design. Source affiliation is captured before the shooter can be cleaned up. |

## Concrete refactors selected

1. **R1 — shared connectivity rebuild:** replace the two matching eight-neighbor flood fills with one `FSeaNavigationMap` method. This is duplicate algorithm removal, not an A* rewrite.
2. **R2 — mission/query separation:** move the battlefield's existing mission methods into a tactical implementation file, and share identical water-actor query exclusions / ocean lookup. Keep one subsystem and the same public reflected types. Remove transitive map-header dependencies where forward declarations suffice.

Each change gets a normal Editor rebuild, Blueprint compilation, native automation, relevant actual PIE regression and its own verified commit. No source cleanup is certified merely because files were inventoried or split.

## Movement and lifecycle contract

Mission logic may request a legal goal through the existing navigator. Combat retreat/return has priority over defense/convoy/search goals. The navigator resolves that intent into one final drive command; original Blueprint force nodes actuate the physical body, and movement filters apply class/turn/death gates. No mission module independently sets hull velocity or writes another force path. Development repositioning invalidates derived navigation state; wreck animation starts only after rigid-body simulation/buoyancy are disabled.

Contact memory records transforms/velocity only after the existing hostile visibility query succeeds. Memory has a configurable lifetime; an occluded target's live transform is not used as a saved sighting. The scout searches validated Small-hull corridor points with bounded retries and advances or returns after stale search exhaustion. Medium/Large escorts retain accessible routes.

## Performance review and remaining debt

The baseline captures Slate callback frame deltas, planning diagnostics, actual ship travel, projectiles and lifecycle events. These measurements are editor/fixture observations, not packaged GPU benchmarks. Profile before optimizing: repeated target-space transforms, water-actor iteration/string lookup, mission candidate arrays, A* work buffers, and Cesium per-tick temporary containers are candidates. None is yet a demonstrated frame-time bottleneck, so this pass does not add speculative caches or change planner budgets.

Remaining debt includes the large steering/combat/controller files, legacy conditional helpers, explicit asset paths, and compact state-transition code. Public reflected functions/fields and conditional compatibility paths are retained unless dependency evidence proves removal safe. External terrain/packaging, multiplayer and broad platform performance remain separate acceptance boundaries.
