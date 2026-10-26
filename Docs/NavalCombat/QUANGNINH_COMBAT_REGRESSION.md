# QuangNinh combat and tactical regression

Branch: `feature/quangninh-battle-integration`. Terrain/cache baseline is published in `d4a8b38`, PR #7 (open, not merged by this agent). The tactical correction below passed its focused functional regressions; the full patrol's strict statistical qualification is retained explicitly.

## Confirmed tactical failure and correction

The current-map critical-health fixture rejected an island-blocked reverse and planned a valid forward escape, but failed departure in both 25 s and 90 s observations. The ship stayed afloat with original physics enabled. In 90 s it displaced only 24.85 m: losing gun visibility repeatedly caused Escape → ReturnToPatrol → Patrol → Escape and reversed rudder direction during the initial turn.

The gun still drops occluded targets immediately. Navigation now keeps its own weak threat reference until the bounded retreat ends. A forward escape must gain meaningful safe separation before returning, using the same 100 m increase required by the existing escape planner. Minimum/maximum retreat durations, death/team guards, original A/B references and the single native force-command path remain in force.

After correction, the same 90 s physical fixture passed all seven checks and displaced **190.67 m during Escape**. Steering remained committed before departure even when the gun target cleared; no blocked navigation cells were entered. See `QUANGNINH_ISLAND_REVERSE_PIE.json` and the preserved failures under `../QuangNinhIntegration/ISLAND_ESCAPE_*`.

| Test | Type | Current result | Evidence |
|---|---|---|---|
| Native tactical build | Compilation | PASSED, 12 actions / 20.86 s; full editor restart | RETREAT_COMMITMENT_BUILD.json |
| Seven affected battle Blueprints | Compilation | PASSED explicitly after restart | Live compile output; ledger |
| Critical island escape | Real PIE physics | PASSED 7/7, 190.67 m escape displacement | QUANGNINH_ISLAND_REVERSE_PIE.json |
| Reverse/fire at 30 HP, speed cap, restoration, rear-hull guard | Real PIE original projectiles/physics | PASSED 12/12 after correction; three shots at -4.24 to -4.29 m/s; minimum -4.327 m/s | QUANGNINH_BACKWARD_RETREAT_PIE.json |
| 1000 m island occlusion | Real PIE physical terrain ray and gun | PASSED 3/3; no acquired target or new shot through island | QUANGNINH_OCCLUSION_PIE.json |
| Saved 3v3 controls, cameras, AI and minimap | Real full-level PIE, 90 s | PASSED 20/20 after correction | QUANGNINH_FINAL_INTEGRATION_PIE.json |
| Passive/Fighter/Hunter roles | Real PIE | PASSED 20/20 after correction; added explicit visible/out-of-range fixture precondition | QUANGNINH_ROLES_PIE.json |
| Easy/Normal/Hard, stationary/moving | Real seeded projectile collision | PASSED 6/6 checks, 24 original shots per case / 144 total | QUANGNINH_ACCURACY_PIE.json |
| Fresh complete patrol after tactical change | Full gameplay soak | Five B/A/B loops, 15 arrivals / 402.77 s, 0 errors/land/Failed samples; 8/9 strict assertions. Contact metric 98.97% narrowly misses 99% for one ship; preserved as FAILED | QUANGNINH_TACTICAL_PATROL_SOAK.json; ../QuangNinhIntegration/PATROL_TACTICAL_SUMMARY.json |
| Frame-resolution flotation after tactical change | Real PIE physics, 65 s | PASSED 4/4; maximum continuous contact loss 0.384 s; hull heights 1.72–5.36 m around 3 m sea surface | ../QuangNinhIntegration/FLOTATION_TACTICAL_FIX.json |
| Small/Large input and damage | Real PIE | Earlier current-map baseline retained; final repeat pending | QUANGNINH_* profile/damage evidence |
| Destruction | Real PIE and visual inspection | PENDING on final configuration | To be recorded |

Fixtures only relocate/freeze isolation participants in disposable PIE worlds. The full-level integration and patrol tests use saved fleet placements and original physics. Parked actors have Blueprint ticks disabled to avoid AddForce warnings from intentionally disabled fixture physics. Accuracy translation preserves the original 900 m engagement and 400 m moving-target geometry in the actual QuangNinh operating bay; no saved actors are repositioned.

Compact JSON files retain checks and first/last samples; adjacent gzip files preserve all frame telemetry losslessly. No historical failed run is relabeled as passed.

## Seeded accuracy results

| Target | Easy hits / shots | Normal | Hard |
|---|---:|---:|---:|
| Stationary | 2 / 24 | 12 / 24 | 24 / 24 |
| Moving | 8 / 24 | 11 / 24 | 18 / 24 |
| Combined | 10 / 48 | 23 / 48 | 42 / 48 |

These are measured collisions, not assigned hit probabilities. The target uses original A*/physics at 8 m/s cruise in the moving cases. After the initial five-second settling window, measured speed ranges were 3.28–8.61, 5.49–8.52 and 6.91–8.42 m/s respectively; target range changed by 27.71, 17.94 and 22.19 m. The raw maximum speed includes the initial vertical fall and must not be reported as cruise speed. All 144 launches followed the animated muzzle (dot product >0.999). This is one seeded finite sample; it does not establish fixed population hit rates.

The first post-fix role repeat failed its Hunter range assertion because the fixture had not verified that the moved hostile remained visible. Hunter correctly cannot classify an unknown occluded target by range. The fixture now re-establishes and asserts a visible hostile beyond the tested 500 m limit before starting that case. All 20 assertions then passed. The failed fixture is preserved in `../QuangNinhIntegration/ROLES_RANGE_FIXTURE_FAILURE.json[.gz]`; no Hunter visibility rule was relaxed.

## Patrol and flotation qualification

The latest soak travelled 2866–3659 m per AI, cruise medians 18.46–18.92 m/s, and completed all required arrivals with exact live targets and intact independent AIControllers. The overall strict `passed` field remains **false** because Enemy2's one-second water-contact samples measured 98.9717%, below 99%. Its four misses were isolated; no consecutive sampled losses or grounding/stall occurred. The threshold was not changed and the test is not reported as 9/9.

A separate frame-resolution physical test on this same native build passed all four flotation checks. Continuous contact loss lasted at most 0.384 s, with normal bounded hull heights and physical propulsion. Frame contact ratios were 96.28–100%; constant immersion is not claimed. This supports functional wave/flotation acceptance while preserving the failed strict sampled statistic transparently. The earlier cache-fix soak's unchanged strict assertion passed 9/9; no repeated sampling was used here to replace the latest failure with a more favorable run.
