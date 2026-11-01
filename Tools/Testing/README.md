# Naval QA entrypoints

Use one Editor and one PIE fixture at a time. Freeze the loaded native DLL during physical tests. Wait for native automation to finish before PIE; it can end an active play world. Execute through Tools/SeaNavigation/mcp_client.py at the existing local bridge. Import/launch/compilation alone is not a pass.

| Purpose | Fixture |
|---|---|
| Result to setup / Back to Game / clean match restart | verify_return_to_setup.py;6 controlled result cases; optional real mouse click |
| Full AI contact / natural winner / menu variants / turns | verify_final_demo.py; KIND=nonlethal/lethal/patrol/turns |
| Serial matrix, stop on first failure | run_final_regressions.py; optional TESTS list |
| Damage / roles / death / capsize / spectator / shooter cleanup | verify_core_gameplay.py; controlled damage/capsize |
| Real Enhanced Input and original BP projectile | verify_player_input.py |
| Controlled300m reverse fire / original patrol return | verify_tactical_retreat.py; other fixture ticks/physics disabled, subject remains physical |
| Physical Small circuit and additional stress | verify_navigation_soak.py; combined reverse-flow stress is uncertified; inspect actual status |
| Last-known search / true reacquisition | verify_scout_reacquisition.py; four fixture deaths, not a natural3v3 |
| Live marker / explicit development ship relocation | verify_dynamic_patrol.py |
| Exact original five-AI A/B fleet | ../NavalCombat/test_seeded_spawn.py; HULL_CLASSES=[2,0,2,0,0,2],REQUIRED_LEGS=4,ENABLE_TACTICS=False,ENABLE_ASSAULT=False |
| Shared10-round/30s magazine | ../NavalCombat/verify_shared_magazine.py |
|120s all-class flotation/response | ../NavalCombat/verify_hull_stability.py |
| Read-only process samples | monitor_process.py Saved/PROBE.json Saved/PROCESS.json |

Results/restrictions: Docs/Testing/FINAL_QA_REPORT.md. Final production bb9b507; earlier natural wins use incremental builds. Legacy NavalCombat fixtures can reference old UI/startup assumptions: inspect before reuse. Some write into Docs/QuangNinhIntegration; preserve new raw output in Saved and publish compact current summaries without overwriting history. Historical mutators remain quarantined under Tools/Archive/NavalCombat.
