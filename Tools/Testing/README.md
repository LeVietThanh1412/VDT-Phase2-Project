# Naval QA entrypoints

Use one Unreal Editor and one PIE fixture at a time. A running soak owns its loaded native DLL: compile/restart only after it ends. Current fixtures use the existing SpecialAgent bridge at localhost8767 through Tools/SeaNavigation/mcp_client.py. Load MainMapLevel, then execute one fixture through that bridge. Each fixture must report its own terminal status; launch/import/compile is not a pass.

| Purpose | Current fixture |
|---|---|
| Seeded startup / repeated patrol | Tools/NavalCombat/test_seeded_spawn.py (REQUIRED_LEGS, ENABLE_TACTICS=false, ENABLE_ASSAULT=false) |
| Saved targets / runtime ship reposition | Tools/NavalCombat/verify_navigation_repair.py |
| Invalid/destroyed targets | Tools/NavalCombat/verify_temporary_patrol_failure.py and verify_target_death_patrol_return.py |
| Island detour | Tools/NavalCombat/verify_navigation_island_pie.py |
| Magazine / visible HUD | Tools/NavalCombat/verify_shared_magazine.py |
| Blue defense | Tools/NavalCombat/verify_blue_defense.py |
| Red convoy / leader/follower death | Tools/NavalCombat/verify_red_assault.py |
| Mixed-class120s stability | Tools/NavalCombat/verify_hull_stability.py |
| Bounded player+5AI battle/restart | Tools/NavalCombat/verify_integrated_sector_battle.py |

The last row is not a completed Full AI battle. Full AI encounter/winner fixtures are outstanding in this stabilization task. Earlier tests/results are evidence to inspect, not proof of current acceptance. The legacy NavalCombat directory remains because fixtures refer to one another by path; migration requires updating those callers together. Historical mutators/diagnostics are quarantined in Tools/Archive/NavalCombat.
