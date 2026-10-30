# Final naval battle QA - integration pending

Pre-merge gameplay acceptance: **PASS**. Final merged-main acceptance: **NOT TESTED**.

| Test | Environment | Result | Evidence |
|---|---|---|---|
| Protected A*, sectors, ten-seed baseline and Blue defense | Prior verified real PIE + native reports | PRESERVED; not repeated | Commits68be150,ea5cd87,533f576,a8bd119,8bbad8f; existing Docs/NavalCombat and QuangNinhIntegration evidence |
| Scout/sequential convoy | UE5.6 real seeded menu/travel, combat disabled only in fixture | PASS10 checks; all three phase3; physical arrivals and >1km movement | RED_ASSAULT_NAVIGATION.json |
| Scout loss | Controlled hostile health death, real movement | PASS5 checks; promoted leader, both survivors >100m post-death | RED_ASSAULT_LEADER_LOSS.json |
| Follower loss | Controlled slot1 hostile health death, real movement | PASS5 checks; scout retained, both survivors >100m post-death | RED_ASSAULT_FOLLOWER_LOSS.json |
| Hull stability | Mixed Small/Large/Medium,120.053 actual game seconds each | PASS all limits on six ships; Large p99 vertical1.584m/s,100% contact | HULL_STABILITY_AFTER.json, PHYSICS_PONTOON_CONFIGURATION.json, HULL_STABILITY_COMPARISON.png |
| Shared magazine/HUD | Player+five AI, actual original BP projectiles | PASS10 checks;10 accepted rounds each;30.160s completion sample; spam/target/death/duplicate guards; inactive-turn timer | SHARED_MAGAZINE_PIE.json |
| Failed projectile completion | Transient neutral producer through actual weapon dispatch; no allocator failure injection | PASS two failed completions,10 rounds retained,0 shots, input gate restored | Embedded failure probe in magazine report |
| Tags | Mixed hull identity plus natural battle | PASS all required team/class names, all four roles and five states observed | IDENTITY_TAGS_MIXED_PIE.json, integrated raw rows |
| Combined battle and real menu restart | Fixed Candidate1,seed20261028,RT/Normal; playerMedium, BlueLarge+Medium, RedSmall+Small+Medium; native combat/damage active | PASS14 checks after180.001s plus restart | INTEGRATED_SECTOR_BATTLE.json |

Combined evidence includes native AI fire on both teams, accepted hostile damage, rejection of all six friendly-hit probes, shared reloads, bounded Blue interception, Red sequential release/phase2 assault, physical movement, all tag role/state names, a naturally destroyed Blue defender with zero drive commands/combat disabled, continued surviving missions, and fresh6-ship health/magazines after same-seed menu restart. Player trigger requests are explicitly ordinary input emulation; no HP/ammo/physics/AI target override in the combined fixture. The startup fixture enables combat/damage while respecting native destroyed state.

Measured final60-second physical displacement: Red scout159m, RedMedium229m, Red follower148m, BlueLarge44.5m; the Blue defender moved43m before its death. Player is intentionally idle except trigger requests. This is a bounded demonstration, not a claim that every battle outcome or every possible seed was newly tested.

Failures preserved: interrupted editor run; Python stale destroyed-actor wrapper failures; editor property reconstruction in the first magazine target-switch fixture; rejected Large damping trials1.8/2.8/3.6 and body-only tuning; first combined fixture incorrectly re-enabled combat on wrecks. Fixes retain thresholds and archives; passing reports replace neither raw failures nor their history. No navigator rewrite or historical A*/ten-seed rerun occurred.

Remaining before delivery: clean-checkout asset/reference validation; enable verified sector/tactics default flow; merge latest main into existing feature; preserve new main UI and integrate ammo into its selected-ship panel; post-sync build and combined QA; push/PR11 merge under repository policy; final remote-main build/smoke. A separate controlled terrain-impact recovery fixture has not been completed. Scene-only screenshot attempts do not prove visible UMG; final merged UI still needs visual inspection.
