# User-authored layout and shoreline correction — 2026-11-02

Status: **PASSED for the documented setup, patrol, shoreline and handling scope**. Eight completed PIE scenarios on native B0E80908, separate from the earlier D0FE049B map acceptance. Compact evidence: `USER_LAYOUT_SHORELINE_PIE.json`.

## Preserved state and reproduced failure

The owner rearranged the main map before this task. `Saved/USER_LAYOUT_20261102.json` records its poses, LLH, marker locations and references. The main map plus all external actors were backed up to `Saved/AuthoredMapCheckpoints/20261102_205254_user_layout/`; its SHA-256 manifest covers 30 files. No authored ship, PlayerStart, target or georeference is moved by this correction.

`Saved/USER_LAYOUT_BEFORE_FIX.json` reproduces the exact setup error: BP_ShipEnemy2's start-to-TP_Red02_A route was rejected. The marker is in raw OSM water about 93.7 m from shore, inside the original 125 m navigation exclusion. No accepted approach previously fit its 80 m arrival radius. The reported lifted-player location (107.441498,21.024979) is also inside that buffer, about 116.7 m from raw land. Previously the same buffered classification could retain sea-level DEM as a solid coast apron.

## Corrections

- Preserve the original buffered A* grid. Add a conservative whole-cell **unbuffered 25 m coast mask** for DEM classification and the existing physical survey. Keep land and selected-LOD nonzero terrain collision. Regenerating navigation data includes this mask; a deterministic regeneration matched the shipped JSON without changing navigation payloads.
- Resolve a water marker to the nearest reachable accepted cell center within its original arrival radius, with a 10 m inset. Never move the marker or accept raw land. Validate all three directed legs. Cache the approach while its map/marker/radius are unchanged.
- At a standoff endpoint, require the physical hull to enter the actual marker radius, reach the checked approach and slow/dwell before switching. Limit only its terminal forecast to the checked endpoint; independent emergency sensors/brakes stay active.
- Reuse the player propulsion filter to sweep the actual hull/keel ahead. Brake through the original force actuator if grounding is imminent; retain rudder and reverse escape. AI keeps its existing navigation command pipeline. Show the shallow-water cue in the HUD.
- Pack identical coast-cell runs before triangle clipping, preserving the exact clipping union. The initial per-cell prototype caused a very long frame; packing removed that observed 31.8 s startup stall. A measured slow tile after packing took 290.4 ms. This is not a steady frame-rate benchmark.
- Rebind sector fingerprints after proving every original navigation JSON field and all 625 sector records are unchanged. The new optional mask changed the whole-file checksum, so generated mode correctly rejected stale metadata until updated. Protect both coupled JSON files from Git line-ending conversion. The mask-only utility refreshes matching old fingerprints; a changed buffered chart still requires sector regeneration.

## Build and tests

Native DLL: `b0e8090838449259c08806ff0c78aa4e7c99fea841a45353c038655a377ed384`.

- Win64 Development Editor C++ build succeeded (`Saved/USER_SHORELINE_BUILD.log`).
- All 39 Blueprint compile requests completed; IsolatedWorld, PublicState, Geometry and RealOSM native tests passed. Geometry includes standoff radius, raw-land and insufficient-radius rejection cases.
- Shoreline PIE passed for Medium and Small at the reported coast. Only initial temporary probe transforms were assigned; actual Enhanced Input and original actuators produced all tested motion. Physics and buoyancy stayed enabled, with 100% sampled water contact. Held-forward input activated the brake and stayed below 1.5 m/s in the final five seconds; reverse moved away over 90 m.

| Hull | Maximum absolute pitch / roll | Reverse travel |
|---|---:|---:|
| Medium | 2.136 / 0.505 degrees | 92.13 m |
| Small | 0.674 / 0.197 degrees | 96.45 m |

Raw evidence: `Saved/USER_SHORELINE_PHYSICS.json`. The initial fixture-only failure used a non-reflected Cesium Python method; it was corrected to the reflected load-progress property and rerun. No production failure is hidden by that correction.

## Physical patrol endurance

`Saved/USER_LAYOUT_FINAL_LOOPS.json` **PASSED** after 1111.4 game seconds. Every ship completed A/B/A/B, including the first A from its authored start. Marker references, controller identities and horizontal spawn positions were preserved. No recovery retries or tactical destination overrides occurred; damage alone was disabled. All sampled arrivals were inside the original radii (placed ships under 79.72 m for an 80 m radius; flagship under 59.83 m for a 60 m radius).

| Ship | Completed legs |
|---|---:|
| BP_ShipEnemy2 | 4 |
| Friendly_AI_1 | 4 |
| Friendly_AI_2 | 7 |
| BP_ShipEnemy3 | 4 |
| BP_ShipEnemy | 4 |
| Blue Flagship | 6 |

Worst absolute sampled pitch/roll across the fleet: 2.862 / 2.774 degrees. Minimum sampled water contact: 99.952%. All six retained physical simulation. Missing turn motion is not hidden by teleportation, velocity assignment or angle locks.

Small and Medium open-water Enhanced Input checks passed with opposite left/right yaw, mouse aim and the original projectile. The initial longer Medium input sequence activated the shoreline brake at about 12 s, before its late left-turn window. Its diagnostic trace is preserved (`Saved/USER_SHORE_MEDIUM_GUARD_DIAGNOSTIC.json`); rudder commands were present. The final fixture completes its unchanged >5 degree turn assertions earlier in the staging lane, keeping the guard enabled. Separate held-thrust/reverse shoreline tests above verify the safety intervention itself.

## Remaining regressions completed

- Small/Medium Enhanced Input: right/left yaw 24.755 / -27.906 and 15.924 / -13.577 degrees; original projectile, mouse look, aim/release, physical simulation and player possession all passed.
- Stationary idle and firing phases each lasted 40 game seconds, all six physical hulls without commanded thrust/rudder. Each fired ten original rounds. Worst p95 roll 0.266 degrees; p95 pitch oscillation around trim 0.695 degrees; p99 vertical speed 0.660 m/s; minimum sampled water contact 99.5%. This trace passed even though its optional hidden-window screenshot failed; the report's older buoyancy image remains explicitly historical.
- Generated seed 20261028, Blue Small/Small/Small versus Red Small/Large/Medium: all six had a valid firing opportunity and fired by 18.5 game seconds. Damage was disabled. All five required native sector connections passed after the fingerprint refresh.
- Result/menu: a real desktop mouse click on **RETURN TO SETUP** passed current result/counters, retained selection, pause/cursor, idempotent return, Back to Game and a clean six-ship restart. An initial failed Slate capture was preserved separately; the fixture now retries failed paints with a bounded timeout rather than treating an `ERROR:` string as successful rendering. No production HUD rewrite was made for that diagnostic.
- Fresh **My Map / Player + AI**, Blue Small player with five Medium AI: correct dedicated Blueprint, faction, PlayerStart, retained seed and marker references; all five AI physically reached A by 441.8 game seconds, with no recovery retry. Medium remains the fresh native default. `Saved/USER_LAYOUT_PLAYER_SMALL.json` records this run.
- Integrity: all 30 user map/actor files exactly match the 20:52 checkpoint; all original navigation JSON fields and all 625 sector records remain unchanged. See `Saved/USER_SHORELINE_INTEGRITY.json`.

No new fatal/assertion, Blueprint runtime or Blueprint compiler errors were found. The pre-existing Cesium metadata enum-initialization diagnostic remains present at Editor startup; the four named navigation automation tests subsequently report Success. No new natural elimination, packaged-build, arbitrary-layout or steady-performance certification is claimed by these controlled tests. Terrain remains solid; unsafe land markers/starts are still rejected.

For demo: open the saved main map, Play, choose **USE MY MAP (A-B)** and **START NEW MATCH**. Select Small only in the PLAYER slot when wanted; a fresh selection defaults to Medium. In Full AI, the sixth ship still uses PlayerStart plus the uniquely tagged player A/B markers. All changes remain local; no Git commit or push was performed.
