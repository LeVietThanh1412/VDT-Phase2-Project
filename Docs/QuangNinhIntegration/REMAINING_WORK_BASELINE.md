# Remaining work baseline - 2026-10-30

Audit branch: `feature/small-warship-seeded-battlefield`, HEAD `8bbad8fbbf9a515653ef7064fca3c81dcc25cc06`; fetched main `42a830afb69e8ac00db3c0c72f10f44c8dd4c4bd`.
PR #11: open, draft, base main, same feature SHA. GitHub connector returned 404; existing local Git credential REST access succeeded. No check runs/status contexts returned. Protection endpoint returned 404; protection cannot be inferred from this response. No merge attempted.

| Feature | Status | Implementation / evidence | Remaining / change necessary |
|---|---|---|---|
| Geographic A* | COMPLETED | 68be150; ShipEnemyNavigationComponent, SeaTerrainCollisionComponent; Docs/NavalCombat/NAVIGATION_PHASE1_VERIFIED.json | Preserve; no historical suite repeat |
| Candidate 1 sector grid | COMPLETED | ea5cd87; NavalSectorSubsystem, quangninh_sectors.json; SECTOR_GRID_QA/PIE reports | Preserve fixed EN14500,3500,2500m,yaw0; 625 cells, main connected332 |
| Seeded full-hull spawning | COMPLETED | 533f576 / a8bd119; NavalBattlefieldSubsystem; SEEDED_SPAWN_SUITE.json (11 starts/10 seeds+replay), SEEDED_REPEATED_PATROL.json | Preserve; focused combined startup only |
| Blue defense | COMPLETED | 8bbad8f; UpdateDefenders, shared ShipCombatComponent; BLUE_DEFENSE_PIE.json (10 checks/314.95s) | Integrate without replacement |
| Red assault | PARTIAL | Uncommitted UpdateAssault, verify_red_assault.py; RED_ASSAULT_NAVIGATION.json first PASS: all three phase3, >1km actual movement each | Add guaranteed configurable sequential release; leader-loss, target-loss and combined combat validation |
| Small textured hull / separate Blueprints | COMPLETED | 9f2bb3f; BP_ShipSmall/BP_ShipSmallAI; Fab Standard user confirmation, source StaticMesh has no skeleton; working bow gun separate from decorative aft gun | Preserve asset bindings and original shared weapon driver |
| Buoyancy tuning | PARTIAL / NOT VERIFIED | ShipPhysicsProfile.cpp and DA profiles; aefaea6 preserved Large vertical damping1.8. SECTOR_PHYSICS_BASELINE.json has 60s untuned evidence | Measure and tune Small/Medium/Large independently; 120s/class plus acceleration/turning and plots |
| Standard team/class/AI tags | NOT IMPLEMENTED | Existing combat Faction and Weapon.Team() authority, actor Dead tag; no required tag registry found | Add tags without competing faction state; maintain roles/states; inspect runtime |
| Magazine/reload/HUD | NOT IMPLEMENTED | Existing shared ShipWeaponComponent firing/FinishShot and ShipHUDWidget | Add authoritative10/30, failed-spawn/death/spam guards, actual HUD and tests |
| Combined gameplay / restart | NOT VERIFIED | Existing component and staged mission evidence only | Actual combined Candidate1 battle, reload/damage/mission recovery/restart |
| Git integration | PARTIAL | Feature pushed8bbad8f, PR11 draft; remote main includes UI PR10 | Finish verified commits; clean main-to-feature merge, resolve source/binaries individually; build/combined QA, push, PR merge, verify final main |

## Protected local work
Six unrelated modified binary assets are deliberately excluded from feature commits: BP_Ship, BP_ShipSmall and external actors 5/S1/N3XR225AU972IEWKZ5VIKH, 8/BG/R5ML2YWL23227PXNQ95T99, B/8F/CHJKNRGBDUPAW44FPQ44PZ, E/M0/0V8JDM2KMQQMNKFS3GB6UL. Preserve and validate committed delivery in isolation before final main claims. Existing recovery snapshots remain under Saved.

## Remaining checklist
- [ ] Sequential convoy / bounded leadership recovery verified
- [ ] Measured buoyancy acceptance for all three hulls
- [ ] Standard tags and authoritative faction verified
- [ ] Shared magazine, world-time reload, real HUD verified
- [ ] Combined gameplay and restart verified with rollout flags enabled
- [ ] Main synchronized into clean feature, conflicts reviewed, post-sync QA
- [ ] Feature pushed, PR merged under repository policy, remote main build/smoke verified

Historical failures remain preserved alongside their later passing evidence. This baseline does not mark staged or isolated tests as natural full-battle acceptance.
