# Architecture reconstructed from main ce3074f

Source inventory covers 65 files under Source (63 module files plus two target definitions), 18,052 lines. It records declarations and implementation entrypoints rather than asserting every branch was executed. Gameplay and assets are unchanged by this report.

| System | Authority / dependencies / lifecycle |
|---|---|
| NavalMatchSettings | GameInstanceSubsystem; validates counts/team/class/seed; retains selections across OpenLevel, not a persistent SaveGame. |
| NavalGameMode | Selects original Medium pawn, dedicated Small player Blueprint, or independent spectator pawn for Full AI. |
| NavalBattleSubsystem (NavalBattleUI.cpp) | WorldSubsystem; roster, team/alive/Fallen, score, CanShipAct, 10-second turns and outcome; timer .1 s; deinitialization clears state. |
| NavalSectorSubsystem | Immutable chart reference, field coordinates and 625 logical sectors; validates grid and navigation fingerprint/connections. |
| NavalBattlefieldSubsystem | Authoring/seed layout, per-hull survey, settling/readiness and tactical mission intents. UpdateDefenders/UpdateAssault live in AI/Tactics, without another authority manager. |
| SeaNavigationMap | Immutable ENU chart, raw coast, connectivity, A*, temporary exclusion edges, conservative shortcuts. |
| ShipEnemyNavigationComponent | Resolve current marker/arrival goal; plan/replan, sensors, traffic priority, recovery; SetDriveCommand final AI throttle/rudder. Called synchronously by Blueprint arbitration. EndPlay clears pointers, georeference listeners and per-component tracking. |
| ShipMovementComponent | Apply class/equipment/profile; filter original Blueprint force; passive heave/side/angular water resistance before Chaos. No ordinary pose or velocity assignment to force route completion. |
| ShipCombatComponent | Hostility/LOS, observation, predictive aim, mechanical limits and role/tactical intents. Calls navigator for movement and shared weapon for shots. |
| ShipWeaponComponent | Shared firing/magazine, guarded damage/death, events and wreck lifecycle; authority informs battle subsystem once. |
| ShipProjectileDamageComponent | Cached source team/damage, victim claims and lifetime; same projectile can reach another victim but never claim one victim twice. |
| Identity/HUD/UI | Identity names/teams are presentation; numeric Team() stays authoritative. Setup/summary/loading/killfeed/selected-ship HUD read authority. |
| CustomPlayerController | View target without possessing AI; player/spectator/death camera; modal and minimap input ownership. |
| Editor integrations | Conditional editor modules inspect/install graphs, profiles and equipment. Inspection runs only read-only helpers; installers were not invoked. |

Fresh Unreal graph inspection confirms original parent and six legacy movement functions retained, exclusive navigator branch and one forward/rudder call pair. It dumps actual Blueprint graph nodes/pins and the Small/Medium/Large DataAsset values. Five placed AI actors have distinct live A/B references, radius 80 m and wait 3 s. The sixth Full-AI ship is created by match configuration, not an absent sixth saved actor.

Interfaces retained: original BP projectile/FX/recoil, skeletal turret driver, Enhanced Input and Water pontoons. SeaGraph/AntiShip assets alone do not establish an active primary system. SpecialAgent/FunplayMCP are developer tooling, not a gameplay network backend.
