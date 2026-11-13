# Source reorganization plan

Branch: `refactor/code`; baseline `76025f7228b280f1009de309efa2c46351aa2596`. User formatting is preserved in `Saved/SourceRefactor/20261111/original`; the attached instructions are superseded only for branch creation, formatting repetition and manual gameplay testing.

All 60 C++ files are mapped below before any relocation. `.Build.cs` and target/module identities remain unchanged. Public contains existing exported C++ interfaces; private contains implementations and internal helpers/probes. No plugin includes an old game header. Existing Python/Blueprint calls use unchanged reflected identities.

| Original | Destination | Group | Risk |
|---|---|---|---|
| `Source/VDTPhase2Project/Actor/Pawn/FreeCameraPawn.cpp` | `Source/VDTPhase2Project/Private/Gameplay/Pawns/FreeCameraPawn.cpp` | gameplay | C++ include/dependency lookup |
| `Source/VDTPhase2Project/Actor/Pawn/FreeCameraPawn.h` | `Source/VDTPhase2Project/Public/Gameplay/FreeCameraPawn.h` | gameplay | UHT/header lookup |
| `Source/VDTPhase2Project/CustomPlayerController.cpp` | `Source/VDTPhase2Project/Private/Gameplay/CustomPlayerController.cpp` | gameplay | C++ include/dependency lookup |
| `Source/VDTPhase2Project/CustomPlayerController.h` | `Source/VDTPhase2Project/Public/Gameplay/CustomPlayerController.h` | gameplay | UHT/header lookup |
| `Source/VDTPhase2Project/NavalBattleUI.cpp` | `Source/VDTPhase2Project/Private/UI/NavalBattleUI.cpp` | ui | C++ include/dependency lookup |
| `Source/VDTPhase2Project/NavalBattleUI.h` | `Source/VDTPhase2Project/Public/UI/NavalBattleUI.h` | ui | UHT/header lookup |
| `Source/VDTPhase2Project/NavalBattlefieldSubsystem.cpp` | `Source/VDTPhase2Project/Private/Subsystems/NavalBattlefieldSubsystem.cpp` | gameplay | C++ include/dependency lookup |
| `Source/VDTPhase2Project/NavalBattlefieldSubsystem.h` | `Source/VDTPhase2Project/Public/Subsystems/NavalBattlefieldSubsystem.h` | gameplay | UHT/header lookup |
| `Source/VDTPhase2Project/NavalBattlefieldTactics.cpp` | `Source/VDTPhase2Project/Private/AI/Tactics/NavalBattlefieldTactics.cpp` | ai | C++ include/dependency lookup |
| `Source/VDTPhase2Project/NavalGameMode.cpp` | `Source/VDTPhase2Project/Private/Gameplay/NavalGameMode.cpp` | gameplay | C++ include/dependency lookup |
| `Source/VDTPhase2Project/NavalGameMode.h` | `Source/VDTPhase2Project/Public/Gameplay/NavalGameMode.h` | gameplay | UHT/header lookup |
| `Source/VDTPhase2Project/NavalMatchSettings.cpp` | `Source/VDTPhase2Project/Private/Gameplay/NavalMatchSettings.cpp` | gameplay | C++ include/dependency lookup |
| `Source/VDTPhase2Project/NavalMatchSettings.h` | `Source/VDTPhase2Project/Public/Gameplay/NavalMatchSettings.h` | gameplay | UHT/header lookup |
| `Source/VDTPhase2Project/NavalSectorSubsystem.cpp` | `Source/VDTPhase2Project/Private/Subsystems/NavalSectorSubsystem.cpp` | gameplay | C++ include/dependency lookup |
| `Source/VDTPhase2Project/NavalSectorSubsystem.h` | `Source/VDTPhase2Project/Public/Subsystems/NavalSectorSubsystem.h` | gameplay | UHT/header lookup |
| `Source/VDTPhase2Project/Private/NavalWaterQueries.cpp` | `Source/VDTPhase2Project/Private/Utilities/NavalWaterQueries.cpp` | utilities | C++ include/dependency lookup |
| `Source/VDTPhase2Project/Private/NavalWaterQueries.h` | `Source/VDTPhase2Project/Private/Utilities/NavalWaterQueries.h` | utilities | C++ include/dependency lookup |
| `Source/VDTPhase2Project/SeaNavigationMap.cpp` | `Source/VDTPhase2Project/Private/Navigation/SeaNavigationMap.cpp` | navigation | C++ include/dependency lookup |
| `Source/VDTPhase2Project/SeaNavigationMap.h` | `Source/VDTPhase2Project/Public/Navigation/SeaNavigationMap.h` | navigation | C++ include/dependency lookup |
| `Source/VDTPhase2Project/SeaTerrainCollisionComponent.cpp` | `Source/VDTPhase2Project/Private/Navigation/SeaTerrainCollisionComponent.cpp` | navigation | C++ include/dependency lookup |
| `Source/VDTPhase2Project/SeaTerrainCollisionComponent.h` | `Source/VDTPhase2Project/Public/Navigation/SeaTerrainCollisionComponent.h` | navigation | UHT/header lookup |
| `Source/VDTPhase2Project/ShipArchetypeEditor.cpp` | `Source/VDTPhase2Project/Private/Ships/Editor/ShipArchetypeEditor.cpp` | ships | C++ include/dependency lookup |
| `Source/VDTPhase2Project/ShipCombatComponent.cpp` | `Source/VDTPhase2Project/Private/Combat/ShipCombatComponent.cpp` | combat | C++ include/dependency lookup |
| `Source/VDTPhase2Project/ShipCombatComponent.h` | `Source/VDTPhase2Project/Public/Combat/ShipCombatComponent.h` | combat | UHT/header lookup |
| `Source/VDTPhase2Project/ShipCombatEditor.cpp` | `Source/VDTPhase2Project/Private/Combat/Editor/ShipCombatEditor.cpp` | combat | C++ include/dependency lookup |
| `Source/VDTPhase2Project/ShipDebugEditor.cpp` | `Source/VDTPhase2Project/Private/Combat/Editor/ShipDebugEditor.cpp` | combat | C++ include/dependency lookup |
| `Source/VDTPhase2Project/ShipDestructionEditor.cpp` | `Source/VDTPhase2Project/Private/Combat/Editor/ShipDestructionEditor.cpp` | combat | C++ include/dependency lookup |
| `Source/VDTPhase2Project/ShipEnemyNavigationComponent.cpp` | `Source/VDTPhase2Project/Private/Navigation/ShipEnemyNavigationComponent.cpp` | navigation | C++ include/dependency lookup |
| `Source/VDTPhase2Project/ShipEnemyNavigationComponent.h` | `Source/VDTPhase2Project/Public/Navigation/ShipEnemyNavigationComponent.h` | navigation | UHT/header lookup |
| `Source/VDTPhase2Project/ShipEnemyNavigationEditor.cpp` | `Source/VDTPhase2Project/Private/Navigation/Editor/ShipEnemyNavigationEditor.cpp` | navigation | C++ include/dependency lookup |
| `Source/VDTPhase2Project/ShipEnemyNavigationTests.cpp` | `Source/VDTPhase2Project/Private/Tests/ShipEnemyNavigationTests.cpp` | tests | C++ include/dependency lookup |
| `Source/VDTPhase2Project/ShipEnemyProjectile.cpp` | `Source/VDTPhase2Project/Private/Combat/Projectiles/ShipEnemyProjectile.cpp` | combat | C++ include/dependency lookup |
| `Source/VDTPhase2Project/ShipEnemyProjectile.h` | `Source/VDTPhase2Project/Public/Combat/ShipEnemyProjectile.h` | combat | UHT/header lookup |
| `Source/VDTPhase2Project/ShipMovementComponent.cpp` | `Source/VDTPhase2Project/Private/Ships/Movement/ShipMovementComponent.cpp` | ships | C++ include/dependency lookup |
| `Source/VDTPhase2Project/ShipMovementComponent.h` | `Source/VDTPhase2Project/Public/Ships/ShipMovementComponent.h` | ships | UHT/header lookup |
| `Source/VDTPhase2Project/ShipMovementEditor.cpp` | `Source/VDTPhase2Project/Private/Ships/Editor/ShipMovementEditor.cpp` | ships | C++ include/dependency lookup |
| `Source/VDTPhase2Project/ShipPhysicsProfile.cpp` | `Source/VDTPhase2Project/Private/Ships/Physics/ShipPhysicsProfile.cpp` | ships | C++ include/dependency lookup |
| `Source/VDTPhase2Project/ShipPhysicsProfile.h` | `Source/VDTPhase2Project/Public/Ships/ShipPhysicsProfile.h` | ships | UHT/header lookup |
| `Source/VDTPhase2Project/ShipProjectileDamageComponent.cpp` | `Source/VDTPhase2Project/Private/Combat/ShipProjectileDamageComponent.cpp` | combat | C++ include/dependency lookup |
| `Source/VDTPhase2Project/ShipProjectileDamageComponent.h` | `Source/VDTPhase2Project/Public/Combat/ShipProjectileDamageComponent.h` | combat | UHT/header lookup |
| `Source/VDTPhase2Project/ShipSetupWidget.cpp` | `Source/VDTPhase2Project/Private/UI/ShipSetupWidget.cpp` | ui | C++ include/dependency lookup |
| `Source/VDTPhase2Project/ShipSetupWidget.h` | `Source/VDTPhase2Project/Public/UI/ShipSetupWidget.h` | ui | UHT/header lookup |
| `Source/VDTPhase2Project/ShipWreckBillboardComponent.cpp` | `Source/VDTPhase2Project/Private/Combat/ShipWreckBillboardComponent.cpp` | combat | C++ include/dependency lookup |
| `Source/VDTPhase2Project/ShipWreckBillboardComponent.h` | `Source/VDTPhase2Project/Public/Combat/ShipWreckBillboardComponent.h` | combat | UHT/header lookup |
| `Source/VDTPhase2Project/Tests/ShipWeaponFailureProbe.cpp` | `Source/VDTPhase2Project/Private/Tests/ShipWeaponFailureProbe.cpp` | tests | C++ include/dependency lookup |
| `Source/VDTPhase2Project/Tests/ShipWeaponFailureProbe.h` | `Source/VDTPhase2Project/Private/Tests/ShipWeaponFailureProbe.h` | tests | UHT/header lookup |
| `Source/VDTPhase2Project/UI/ButtonTutorialsUI.cpp` | `Source/VDTPhase2Project/Private/UI/ButtonTutorialsUI.cpp` | ui | C++ include/dependency lookup |
| `Source/VDTPhase2Project/UI/ButtonTutorialsUI.h` | `Source/VDTPhase2Project/Public/UI/ButtonTutorialsUI.h` | ui | UHT/header lookup |
| `Source/VDTPhase2Project/UI/KillFeedWidget.cpp` | `Source/VDTPhase2Project/Private/UI/KillFeedWidget.cpp` | ui | C++ include/dependency lookup |
| `Source/VDTPhase2Project/UI/KillFeedWidget.h` | `Source/VDTPhase2Project/Public/UI/KillFeedWidget.h` | ui | UHT/header lookup |
| `Source/VDTPhase2Project/UI/LoadingScreenWidget.cpp` | `Source/VDTPhase2Project/Private/UI/LoadingScreenWidget.cpp` | ui | C++ include/dependency lookup |
| `Source/VDTPhase2Project/UI/LoadingScreenWidget.h` | `Source/VDTPhase2Project/Public/UI/LoadingScreenWidget.h` | ui | UHT/header lookup |
| `Source/VDTPhase2Project/UI/MatchSummaryWidget.cpp` | `Source/VDTPhase2Project/Private/UI/MatchSummaryWidget.cpp` | ui | C++ include/dependency lookup |
| `Source/VDTPhase2Project/UI/MatchSummaryWidget.h` | `Source/VDTPhase2Project/Public/UI/MatchSummaryWidget.h` | ui | UHT/header lookup |
| `Source/VDTPhase2Project/UI/ShipDetailsUI.cpp` | `Source/VDTPhase2Project/Private/UI/ShipDetailsUI.cpp` | ui | C++ include/dependency lookup |
| `Source/VDTPhase2Project/UI/ShipDetailsUI.h` | `Source/VDTPhase2Project/Public/UI/ShipDetailsUI.h` | ui | UHT/header lookup |
| `Source/VDTPhase2Project/UI/TeamCountBarWidget.cpp` | `Source/VDTPhase2Project/Private/UI/TeamCountBarWidget.cpp` | ui | C++ include/dependency lookup |
| `Source/VDTPhase2Project/UI/TeamCountBarWidget.h` | `Source/VDTPhase2Project/Public/UI/TeamCountBarWidget.h` | ui | UHT/header lookup |
| `Source/VDTPhase2Project/VDTPhase2Project.cpp` | `Source/VDTPhase2Project/Private/VDTPhase2Project.cpp` | module | C++ include/dependency lookup |
| `Source/VDTPhase2Project/VDTPhase2Project.h` | `Source/VDTPhase2Project/Private/VDTPhase2Project.h` | module | C++ include/dependency lookup |

The JSON manifest also records every included-by consumer, paired file, original digest, reflected/interface classification and compatibility note. Each group updates dependent includes before its incremental build. All implementation bodies, constants and APIs must match the saved formatted baseline after excluding include syntax and the necessary UHT metadata literal repair.
