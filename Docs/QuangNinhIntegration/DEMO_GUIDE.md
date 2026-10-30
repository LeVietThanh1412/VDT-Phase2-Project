# QuangNinh naval demo setup

Status: Small-ship milestone under final verification. The saved old ship
positions are not the final battlefield. The randomized island field and final
multi-seed/loop/combat acceptance are still being implemented. Do not present
this document as a claim that the complete battle demo has passed.

## Inspect the Small ship

Open `Content/ShipSmall/BluePrint/BP_ShipSmall`. This inherits the existing
BP_Ship gameplay. Its AI counterpart is `BP_ShipSmallAI`, inheriting BP_ShipEnemy.
Both have the grey source texture, their own visible boat/gun components, a
compact physics hull, and the Small data profile. No manual Blueprint wiring
is required.

The source boat's built-in aft gun is static decoration. The active weapon is
the separate bow gun. `SmallGunYaw` and `SmallGunPitch` identify its two moving
parts. They follow the original animated gun driver and share its muzzle and
projectile event. This model does not contain a hidden native Small skeleton.

## Select a Small player and mixed fleet

1. Open `MainMapLevel` and start PIE normally, without holding Ctrl.
2. Press **P** to open Match Setup.
3. Select 3 Blue and 3 Red ships. Blue slot 1 is always the player.
4. For a Small showcase, choose Blue `[Small, Medium, Small]` and Red
   `[Small, Large, Medium]`, Normal difficulty and Real-Time mode.
5. Click **START NEW MATCH**. This travels to a fresh level and spawns the
   dedicated BP_ShipSmall for the player. Expect **38 / 38 HP** on the HUD.
6. Use **P** again to change ship counts, classes, difficulty or mode; Return
   closes the menu without applying its draft selections.

The full mixed battle demonstration must use the final validated battlefield
configuration, which is pending. The Small showcase screenshots and isolated
PIE tests do not certify the old saved AI spawn locations.

## Controls

These ship controls were read from the actual `IMC_Ship` asset.

| Control | Action |
| --- | --- |
| W / S | Forward thrust / braking and reverse |
| A / D | Steer |
| Mouse | Look/aim direction |
| Right mouse button | Gun aiming view |
| Left mouse button | Fire |
| Mouse wheel | Zoom |
| P | Match setup |
| Q | Cycle ship cameras while preserving AI possession |
| F | Toggle free camera |
| Tab / Q after player death | Next surviving Blue teammate |
| M after player death | Battlefield overview |

The native Q/F/P/Tab/M bindings were source-inspected and their handlers tested.
Physical keyboard/mouse delivery to the hidden automation editor is not claimed
as a separate hardware test.

## Explain the Small archetype

- Approximately 67 m visual length versus the current 158 m hull; actor scale 1.
- One 66 m compact simulated hull, four fitted pontoons, 38 HP.
- Attacker damage 6; actual bow/middle/stern results 5/6/5. Medium/Large keep
  100 HP and 8/10/8 positional damage.
- Actual speed about 28.8 m/s versus Medium 25.7 m/s in the controlled run.
- Actual class-turn comparison and final socket tuning are recorded in
  `SMALL_RESPONSE_SMALL.json` / `SMALL_RESPONSE_MEDIUM.json`.
- Original AI controller, A*, team rules, aiming, hit-and-run and death pipeline.
- Source model: YourAverageGamer, “Sweeden Military Boat”, user-confirmed Fab
  Standard. See `Docs/NavalCombat/SMALL_SHIP_INTEGRATION.md` for modifications.

## Final demonstration sequence to verify

After the remaining battlefield work passes, demonstrate setup, reproducible
seed, Blue/Red spawn halves, a Small player, independent AI patrols around
islands, combat/retreat/damage, a Small wreck sinking, spectator following,
minimap markers, and a separate 10-second Turn-Based match. The final report
must list the actual tested seeds, new full A/B loop soak, failures/fixes,
performance measurements and reopened-editor result.
