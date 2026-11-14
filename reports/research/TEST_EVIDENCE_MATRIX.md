# Test evidence matrix

| Evidence | Type / version | Actual result | Boundary |
|---|---|---|---|
| NATURAL_MATCH_PIE.json | Autonomous lethal 3v3, seed 20261028, DLL bf8b893a... | BLUE WINS 802.999 s; all six fired; Blue 3/Red 0 | Historical final candidate; no forced damage; not current-main fresh soak |
| AUTHORED_SIX_SHIP_PIE.json | Saved six Medium A/B endurance, DLL 9eb9db98... | 1350.934 s; >=4 arrivals each; minimum water 0.999742; max pitch/roll 3.055/4.486 deg | Controlled patrol, not natural elimination |
| SMALL_WAVE_FOLLOWUP.json | Existing ocean, five physical states, DLL 17881a6b... | Idle peak pitch 2.178 deg; reverse 2.673; reverse-fire 2.521 and 10 shots | Not every axis reaches 2-3 deg; measured parameters, not angle clamp |
| CORE_GAMEPLAY_REGRESSION.json | Controlled faults | Regional 8/10/8; self/friendly/duplicate guards; role/capsize/death/reload/spectator checks pass | Injected faults are not a natural battle |
| TACTICAL_RETREAT_REGRESSION.json | Controlled tactical fixture | Native retreat/reverse-fire/resume passed | Isolated behavior; manual A/B deliberately keeps route |
| MERGED_MAIN_VERIFICATION.json | Original project main 9b915b0 | Native build,39 BP,4 nav tests, UI/player/return pass | Earlier source version, not newly rerun |
| UI_BUOYANCY_REGRESSION.md / PIE_EVIDENCE_SUMMARY.json | Cumulative integration | 10 menu cases; turn modes; relocations; result lifecycle | Retains diagnostic failures/interrupted sessions separately |
| Saved/MinimapInput/20261114/GreenAutomation/index.json | Current minimap native regression | 1 succeeded,0 failed | Widget-level pointer test, not actual OS input |
| Saved/MinimapInput/20261114/PIE_INPUT.json | Current Enhanced Input + menu PIE | Eight checks pass; shots0->1, rounds10->9, damage enabled | IA_Aim/IA_Fire injected, not direct FirePlayer |
| Saved/DynamicMinimap/20261114/FOLLOW_PIE.json | Current follow PIE | Five checks;93 samples; max map-space centering error0 | Particular viewport/rendered BP markers |
| Saved/MinimapInput/20261114/PHYSICAL_DIAGNOSTIC.json | Desktop mouse tool | INCONCLUSIVE; zero LMB-down frames sampled | Never count as physical mouse firing pass |
| UNREAL_INSPECTION.json | Fresh read-only commandlet, source ce3074f | Graph, input, profiles, Small skeleton/type and5 saved AI refs inspected | Not a gameplay test; no asset saves |

EVIDENCE_INDEX.json contains compact summaries and SHA-256 of 62 selected records. Failed navigation stress/backoff diagnostics remain in Docs/Testing and are not relabeled as passes. No packaged/GPU/FPS benchmark was run for this report.
