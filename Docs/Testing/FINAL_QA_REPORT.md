# Final stabilization QA

**Final verdict: DEMO_READY_WITH_LIMITATIONS.** Production source audit and safe refactors R1-R4 are verified. Certification covers the documented island-defense preset and original tested A/B composition, not arbitrary fleet/path combinations.

## Verified baseline and latest bug fixes

The original setup failure was reproduced using the mixed Medium/Small fleet. A third heavy Blue spawn could not fit the northern hint; native nearest-water selection also needed team/bounds filtering before selecting a candidate. The heavy southern/eastern hint and player-goal clearance fix passed the same preset on real terrain. Full AI uses six AIControllers and an independent spectator pawn.

Death now notifies the authoritative battle subsystem once, in addition to UI presentation. Duplicate deaths are guarded by weapon state and the subsystem fallen set. Small Red scouts store genuinely observed enemy positions and search validated water goals; hidden current positions are not relabelled as memory. Controlled lost-contact evidence records loss52.21s, search96.90s, reacquisition399.66s and a terminal result532.13s; four fixture deaths make that a controlled case, not a natural3v3 match.

Source refactoring consolidated the exact connectivity algorithm and shared water queries, separated mission definitions inside the same subsystem, and reused spectator view helpers after death. Endurance then exposed crossing priority bypassing following-distance braking and recovery exhaustion preventing future arbitration. The final navigator retains following separation and cooldown reassessment through its existing sole command pipeline. The wave/heave arrival hypothesis was rejected and working arrival logic was preserved.

## Natural full-match series

All runs use Blue Small/Small/Small and Red Small/Large/Medium,100HP/base10 damage, Normal difficulty and real time. No forced damage, target/pose changes, aim tuning, teleports or winner assignment. The implementations are incremental; earlier runs are not presented as identical final-DLL repetitions.

| Implementation | Seed | First contact / kill (s) | Duration (s) | Deaths | Native result |
|---|---|---|---|---|---|
| b9fb0e0 | 20261028 | 0.50 / 146.20 | 568.08 | 4 | BLUE WINS |
| 2f544a3 | 20261029 | 0.50 / 116.09 | 251.65 | 4 | BLUE WINS |
| ad862d1 | 20261030 | 0.51 / 172.50 | 605.36 | 4 | RED WINS |
| bb9b507 | 20261028 | 0.51 / 93.38 | 371.89 | 5 | BLUE WINS |

The latest bb9b507 run had five health-depletion deaths, all six original projectile paths firing, genuine retreat/return transitions and no recorded navigation failure. See NATURAL_MATCH_SERIES.json and the individual evidence files.

## Latest six-ship navigation and encounter

Distances below are sampled physical travel during the latest natural match, ending at death or match completion. A* failures are diagnostic reasons; stuck events are observed native counter transitions, not inferred from a slow waypoint.

| Ship (test identity) | Team / hull | Travel (m) | A* failures | Stuck events | Result |
|---|---|---|---|---|---|
| BP_ShipEnemy2 | Red / Small | 833 | 0 | 0 | Physical; AIController preserved |
| Friendly_AI_1 | Blue / Small | 2217 | 0 | 0 | Physical; AIController preserved |
| Friendly_AI_2 | Blue / Small | 1722 | 0 | 0 | Physical; AIController preserved |
| BP_ShipEnemy3 | Red / Large | 1040 | 0 | 0 | Physical; AIController preserved |
| BP_ShipEnemy | Red / Medium | 1365 | 0 | 0 | Physical; AIController preserved |
| BP_ShipSmallAI | Blue / Small | 621 | 0 | 0 | Physical; AIController preserved |

| Ship | Detection (s) | Valid LOS (s) | First actual shot (s) | Shots / hits |
|---|---|---|---|---|
| BP_ShipEnemy2 | 0.51 | 0.51 | 2.52 | 10 / 4 |
| Friendly_AI_1 | 0.51 | 0.51 | 2.02 | 33 / 17 |
| Friendly_AI_2 | 0.51 | 0.51 | 2.52 | 15 / 8 |
| BP_ShipEnemy3 | 0.51 | 0.51 | 16.11 | 27 / 8 |
| BP_ShipEnemy | 0.51 | 0.51 | 1.52 | 38 / 16 |
| BP_ShipSmallAI | 0.51 | 0.51 | 3.53 | 2 / 1 |

All six also had a valid in-range ballistic opportunity. After a real cold Editor restart, final-DLL all-six contact passed seeds20261028/29/30 in15.55/11.54/11.03s. Team tags and independent camera/controller checks passed; COLD_CONTACT_*.json and COLD_RESTART_SUITE.json.

## Navigation endurance and restrictions

- Exact original fleet, Blue Medium/Small/Medium and Red Small/Small/Medium: all five AI physically completed B/A/B/A in667.86s on the final DLL. Earlier baseline took730.77s. Controllers, active live goals, physical bodies and sampled arrival radii passed. POST_R4_FIVE_AI_AB.json.
- All three Blue Small ships physically completed ten-point island circuits by749.64s with zero recovery retries after following-distance correction. ISLAND_CIRCUIT_AND_STRESS.json includes later stress telemetry; its combined stress status is deliberately not PASSED.
- Five100m development ship relocations and five live TargetPoint changes passed subsequent physical arrivals in DYNAMIC_FIVE_AI_PATROLS.json. One transient endpoint was not sampled before switching; this is explicitly identified rather than counted as an endpoint sample.
- Small passes the west/north route; Medium/Large use the eastern defensive arc. The original625 spawn sectors and terrain/georeference are preserved. A600m westward navigation-only survey extension accommodates the Small loop.
- FAILED: repeating the assault-disabled convoy endpoints with a heavy Red hull and the new-default legacy A/B fleet can stall in local avoidance. These artificial/alternative layouts are uncertified; use the documented island-defense preset. See LEGACY_AB_NEW_FLEET_FAILED.json and NAVIGATION_BACKOFF_ONLY_FAILED.json.

## Regression evidence

| Feature | Method / evidence | Result |
|---|---|---|
| Production C++ | Normal Development Editor rebuild after each R1-R4; current frozen DLL hash in evidence | PASS |
| Affected Blueprints | All39 project Blueprint/AnimBlueprint/WidgetBlueprint packages compiled; generated classes and compiler log checked | PASS |
| Native planner/component | Geometry, RealOSM, PublicState, IsolatedWorld;4 successes after final native build | PASS |
| Continuous A* / controllers | POST_R4_FIVE_AI_AB.json; ISLAND_CIRCUIT_AND_STRESS.json (circuit scope) | PASS in documented scope |
| Scout search / reacquisition | SCOUT_REACQUISITION_CONTROLLED.json; real observation-derived memory and native route progression | PASS; controlled case |
| Damage / death / sinking | CORE_GAMEPLAY_REGRESSION.json;8/10/8, self/friendly/duplicate guards, capsize, cleanup, single counter update | PASS |
| Ammo / reload | SHARED_MAGAZINE_REGRESSION.json;10 rounds,30s, target switch/spam/duplicate callback, cancellation on death | PASS |
| Normal player / input | PLAYER_INPUT_REGRESSION.json; actual Enhanced Input and original BP projectile | PASS |
| Player-team / fleet selection | FINAL_RED_PLAYER.json and FINAL_ASYMMETRIC.json (their incremental native hashes retained); original mixed setup fixed | PASS |
| Turn mode | POST_R4_TURNS.json; alternating10s actions and zero inactive propulsion after transition grace | PASS |
| Buoyancy / all3 hull classes | POST_R4_HULL_STABILITY.json;120s real water, roll/pitch/contact/vertical response/responsiveness | PASS |
| Full AI / camera / minimap | Independent free camera, six retained AIControllers, Q-cycle check, existing minimap rebuilt for late sixth spawn | PASS; global-scale minimap limitation |
| Seeded placement | FINAL_SEED_STARTUPS.json:7 additional native physical starts;3 contact seeds separately recorded | PASS startup; exact seed20261028 cold replay; not10 full combat certifications |
| Controlled reverse fire / return | TACTICAL_RETREAT_REGRESSION.json; real300m reverse, approximately-4.5m/s, actual shots and physical return with original targets/controller | PASS; controlled case |
| Final cold restart / tags / HUD | COLD_RESTART_SUITE.json and COLD_CONTACT_*.json; saved MainMapLevel, same DLL, production menu, six correct team tags, three contact seeds | PASS |

## Performance and remaining limits

Latest natural-match Slate callback mean17.19ms and p9518.74ms; sampled native A* search costs and plan counts are in POST_R4_NATURAL_MATCH.json. These are Editor/fixture observations, not packaged CPU/GPU performance certification. PROCESS_PROFILE.json shows a warmed process about5044–5138MiB RSS over611s before end-of-PIE release. A20-minute .uestats file is preserved under Saved/Profiling/UnrealStats. No speculative caches or planner-budget changes were justified.

The QuangNinh data stays external; actual streamed collision/keel clearance is checked, but ocean fallback depth is not certified bathymetry. No packaged build, multiplayer, platform matrix or guaranteed arbitrary fleet/seed/navigation layout is claimed. The existing35km minimap scale clusters ships and labels. The input fixture injected zoom but did not assert a resulting FOV/arm-length change, so zoom has no separate current certification. The pre-existing Cesium metadata enum startup diagnostic/toolchain deprecations remain; normal final gameplay has no new critical runtime error.

Architecture details: ../Architecture/CODEBASE_AUDIT.md and REFACTOR_REPORT.md. Demo instructions: ../Demo/DEMO_SETUP.md. Failures and interrupted/controlled fixtures are never relabelled as natural wins.

## Repository and delivery

Branch: feature/small-warship-seeded-battlefield. Tested production commit: bb9b507 (R1:2f544a3, R2:ad862d1, R3:b05711e). Main remains0078c96. Recovery ref backup/before-final-demo-20261031 and pre-existing stashes/release checkout are preserved. Documentation/evidence commits follow the frozen production commit. Push/PR status is verified during delivery and linked in the final response; no unverified merge or force push. UE was closed after final cold tests.
