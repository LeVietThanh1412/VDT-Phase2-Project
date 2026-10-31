# Cleanup audit - 2026-10-31

Baseline: clean feature/small-warship-seeded-battlefield at9be1d79; main0078c96; same file tree. Recovery ref backup/before-final-demo-20261031. PR11/12 are merged; no open PR. Existing three map stashes and the separate clean release checkout are preserved. No Unreal Editor process was running at audit.

## Classification

- KEEP: Source52 production/editor/test files, Content340 assets/data, Config4, Plugins89. No C++/Unreal asset is moved for visual consistency. Runtime chart/sector data and all collision/physics code remain intact.
- REFACTOR: root README and operational tool/document indexes; the README incorrectly calls the C++ module basic. Current tests remain at their existing paths until callers are migrated.
- CONSOLIDATE: new test entrypoint/index will choose existing fixtures rather than duplicate their gameplay logic.
- ARCHIVE:13 unreferenced historical scripts and15 unreferenced diagnostic reports listed below. All bytes and failures retained.
- REMOVE_CANDIDATE: none proven safe enough to delete. No removal is performed.

## Reference checks

Every proposed script move has zero filename/import references in tracked source, config, tools and documentation. All tracked Unreal packages were additionally scanned for the UTF8/UTF16 script filename: no match. Reports have no incoming text references. No current runner inputs or setup guides depend on the selected files. No asset path changes are involved. Existing long patrol, seeded, combat, reload, physics, destruction and dynamic-target fixtures remain operational. Archived scripts are frozen experiments and may have obsolete paths; they are not advertised as runnable regression entrypoints.

| File | Decision | Destination |
|---|---|---|
| Tools/NavalCombat/cleanup_sensor_debug.py | ARCHIVE | Tools/Archive/NavalCombat/cleanup_sensor_debug.py |
| Tools/NavalCombat/configure_demo.py | ARCHIVE | Tools/Archive/NavalCombat/configure_demo.py |
| Tools/NavalCombat/configure_quangninh_collision.py | ARCHIVE | Tools/Archive/NavalCombat/configure_quangninh_collision.py |
| Tools/NavalCombat/configure_quangninh_origin.py | ARCHIVE | Tools/Archive/NavalCombat/configure_quangninh_origin.py |
| Tools/NavalCombat/configure_small_game_mode.py | ARCHIVE | Tools/Archive/NavalCombat/configure_small_game_mode.py |
| Tools/NavalCombat/diagnose_quangninh_collision.py | ARCHIVE | Tools/Archive/NavalCombat/diagnose_quangninh_collision.py |
| Tools/NavalCombat/diagnose_return_patrol.py | ARCHIVE | Tools/Archive/NavalCombat/diagnose_return_patrol.py |
| Tools/NavalCombat/plan_patrol_v2.py | ARCHIVE | Tools/Archive/NavalCombat/plan_patrol_v2.py |
| Tools/NavalCombat/plan_quangninh_origin.py | ARCHIVE | Tools/Archive/NavalCombat/plan_quangninh_origin.py |
| Tools/NavalCombat/probe_destruction_fx.py | ARCHIVE | Tools/Archive/NavalCombat/probe_destruction_fx.py |
| Tools/NavalCombat/render_patrol_v2.py | ARCHIVE | Tools/Archive/NavalCombat/render_patrol_v2.py |
| Tools/NavalCombat/repair_demo_positions.py | ARCHIVE | Tools/Archive/NavalCombat/repair_demo_positions.py |
| Tools/NavalCombat/repair_small_material.py | ARCHIVE | Tools/Archive/NavalCombat/repair_small_material.py |
| Docs/NavalCombat/DEMO_POSITION_AUDIT_BEFORE.json | ARCHIVE | Docs/Archive/NavalCombat/DEMO_POSITION_AUDIT_BEFORE.json |
| Docs/NavalCombat/DEMO_RUNTIME_FLEET.json | ARCHIVE | Docs/Archive/NavalCombat/DEMO_RUNTIME_FLEET.json |
| Docs/NavalCombat/FINAL_BATTLE_EDITOR_MEMORY.json | ARCHIVE | Docs/Archive/NavalCombat/FINAL_BATTLE_EDITOR_MEMORY.json |
| Docs/NavalCombat/HULL_PROFILE_AUDIT.json | ARCHIVE | Docs/Archive/NavalCombat/HULL_PROFILE_AUDIT.json |
| Docs/NavalCombat/MERGE_HULL_DAMAGE_PIE.json | ARCHIVE | Docs/Archive/NavalCombat/MERGE_HULL_DAMAGE_PIE.json |
| Docs/NavalCombat/PATROL_CLOSE_SEPARATION_DIAGNOSTIC.json | ARCHIVE | Docs/Archive/NavalCombat/PATROL_CLOSE_SEPARATION_DIAGNOSTIC.json |
| Docs/NavalCombat/PATROL_EMERGENCY_TURN_DIAGNOSTIC.json | ARCHIVE | Docs/Archive/NavalCombat/PATROL_EMERGENCY_TURN_DIAGNOSTIC.json |
| Docs/NavalCombat/PATROL_SPACING400_PRE_PHASE_DIAGNOSTIC.json | ARCHIVE | Docs/Archive/NavalCombat/PATROL_SPACING400_PRE_PHASE_DIAGNOSTIC.json |
| Docs/NavalCombat/PATROL_STATIONARY_SHIP_DIAGNOSTIC.json | ARCHIVE | Docs/Archive/NavalCombat/PATROL_STATIONARY_SHIP_DIAGNOSTIC.json |
| Docs/NavalCombat/PATROL_TRAFFIC_DIAGNOSTIC.json | ARCHIVE | Docs/Archive/NavalCombat/PATROL_TRAFFIC_DIAGNOSTIC.json |
| Docs/NavalCombat/PIE_SUITE_PRIOR_DIAGNOSTIC.json | ARCHIVE | Docs/Archive/NavalCombat/PIE_SUITE_PRIOR_DIAGNOSTIC.json |
| Docs/NavalCombat/SMALL_ACCEPTANCE_INITIAL_PLACEMENT_FAILURE.json | ARCHIVE | Docs/Archive/NavalCombat/SMALL_ACCEPTANCE_INITIAL_PLACEMENT_FAILURE.json |
| Docs/NavalCombat/SMALL_DAMAGE_FIXTURE_INTERCEPTION.json | ARCHIVE | Docs/Archive/NavalCombat/SMALL_DAMAGE_FIXTURE_INTERCEPTION.json |
| Docs/NavalCombat/SMALL_GUN_INITIAL_PLACEMENT_FAILURE.json | ARCHIVE | Docs/Archive/NavalCombat/SMALL_GUN_INITIAL_PLACEMENT_FAILURE.json |
| Docs/NavalCombat/SMALL_INITIAL_TURN_TUNING.json | ARCHIVE | Docs/Archive/NavalCombat/SMALL_INITIAL_TURN_TUNING.json |

## Baseline limitations

Full AI Demo is absent in current source. The prior battle evidence is bounded180s player+five AI, not an all-six lethal winner. New requirement requests a left island circuit and shared contact approach. Arrival currently gates on total3D velocity<0.5m/s for2s; wave-induced heave is a hypothesis for intermittent arrival stalls, not yet a confirmed runtime root cause. Blueprint registry/load/compile and current saved target transforms will be audited in the editor. Cleanup acceptance still requires a current C++ build, Blueprint compile and map load.
