# Chapter-to-evidence mapping

| Chapter | Features | Primary source | Evidence |
|---|---|---|---|
|1|All|Source inventory; Build.cs; public headers; Git main/package/Thanh|SOURCE_INVENTORY; UNREAL_INSPECTION|
|2|F01-04,07,09|SeaNavigationMap; ShipEnemyNavigation; SeaTerrainCollision; NavalBattlefield/Sector|AUTHORED_SIX_SHIP; USER_LAYOUT_SHORELINE; STATIONARY_TRAFFIC|
|3|F05-09|ShipPhysicsProfile; ShipMovement; ShipArchetypeEditor|UNREAL_INSPECTION profiles/mesh/bones; SMALL_WAVE_FOLLOWUP|
|4|F10-18|ShipCombat; ShipProjectileDamage; NavalBattlefieldTactics; battle subsystem|CORE_GAMEPLAY; TACTICAL_RETREAT; NATURAL_MATCH|
|5|F18-22|ShipSetupWidget; MatchSettings; GameMode; Controller; UI widgets|MATCH_UI; MERGED_MAIN; current MinimapInput and DynamicMinimap|
|6|F01-23|Recorded source/DLL and fixture metadata|TEST_EVIDENCE_MATRIX; exact JSON metrics and retained failures|
|7|F03-09,15,18-22|Saved actor refs/profiles; setup validation and input mappings|Fresh UNREAL_INSPECTION and source call paths|
|8|All|Scope constraints|KNOWN_LIMITATIONS; evidence hierarchy|

Report source paths use current Public/Private structure; old documentation's flat paths are historical. No advanced method is inferred from a class/asset name.
