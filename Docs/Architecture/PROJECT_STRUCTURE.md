# Project structure

Runtime is C++ plus preserved Blueprint force, turret, projectile and presentation graphs.

| Domain | Production classes / data |
|---|---|
| Navigation | SeaNavigationMap, ShipEnemyNavigationComponent, SeaTerrainCollisionComponent; Content/SeaNavigation chart/sector JSON |
| Fleet/mission | NavalBattlefieldSubsystem (setup/survey), NavalBattlefieldTactics.cpp (same subsystem mission methods), NavalSectorSubsystem, NavalMatchSettings |
| Combat | ShipCombatComponent (shared weapon+AI), ShipProjectileDamageComponent, ShipEnemyProjectile |
| Physics/equipment | ShipMovementComponent, ShipPhysicsProfile, Content/NavalBattle/Profiles and Equipment |
| Lifecycle/presentation | ShipWreckBillboardComponent, NavalBattleUI, UI/ShipDetailsUI, UI/ButtonTutorialsUI |
| Game/control | NavalGameMode, CustomPlayerController, Actor/Pawn/FreeCameraPawn |
| Authoring support | *Editor.cpp files; development-only Blueprint/asset tooling |
| Shared internal queries | Private/NavalWaterQueries.h/.cpp; common ocean lookup and water-actor exclusions |
| Native tests | ShipEnemyNavigationTests.cpp, Tests/ShipWeaponFailureProbe |

Source stays in the current game module; moving UCLASS files only for appearance would create unnecessary include/UHT risk. Existing UI, Actor/Pawn and Tests subfolders remain. Blueprint assets retain all package paths: BP_Ship, BP_ShipEnemy, ABP_Ship, dedicated ShipSmall children, GMBP, minimap and UI assets.

Tools/SeaNavigation contains shared chart/setup/bridge utilities. Tools/NavalCombat contains referenced operational fixtures. Tools/Testing/README.md is the current test index. Tools/Archive/NavalCombat and Docs/Archive/NavalCombat contain frozen experiments with a SHA256 migration manifest. Docs/Maintenance/CLEANUP_AUDIT.md records reference checks and decisions. Runtime terrain remains external under the user-supplied QuangNinh directory and is not added to Git.

One propulsion authority: native navigation produces intent through SetDriveCommand; original Blueprint AddForce nodes remain the physical propulsion/rudder application, filtered by ShipMovementComponent. Tactical mission code selects logical destinations. Normal navigation never writes ship position or linear velocity. Preserve this separation through the circuit/full-AI implementation and safe refactors.
