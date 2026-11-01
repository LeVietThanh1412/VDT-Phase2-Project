# Quang Ninh demo setup

Verified verdict: **DEMO_READY_WITH_LIMITATIONS**. Four natural3v3 wins are recorded across incremental builds; the final production build won naturally in371.89s. Final-DLL cold contacts, original repeated A/B patrols, physical island circuits and gameplay regression results are in [FINAL_QA_REPORT.md](../Testing/FINAL_QA_REPORT.md).

## Start the demonstration

1. Run `powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\RunDemo.ps1` from the repository, or open `VDTPhase2Project.uproject` in Unreal5.6 and open `/Game/Levels/MainMapLevel`. The launcher checks engine, terrain and imagery paths and avoids opening a duplicate Editor.
2. Ensure the local terrain/imagery dataset is available through `E:/UnrealTerrainSet/QuangNinh` (the local junction points to `E:/LeVietThanh1412-Product/QuangNinh`). Terrain uses `file:///E:/UnrealTerrainSet/QuangNinh/Terrain/layer.json`; imagery uses the dataset's `Imagery/tilemapresource.xml`.
3. Press Play. The initial setup menu opens before the match proceeds.
4. Choose three ships per team, **Full AI Demo / spectator**, **Real-Time**, **Normal**, **Standard demo:100 HP /10 damage**, and **Island defense demo**.
5. Use seed **20261028**. The recommended default fleet is Blue: Small/Small/Small; Red: Small/Large/Medium. Red's selected Small hull leads reconnaissance; native mission sorting does not replace the selected hulls.
6. Start New Match and allow terrain surveying/physical settling to finish. Combat is gated during preparation. The spectator then opens the battlefield overview.

The three Small Blue defenders patrol the same left island group. Red starts in the northeast and advances southwest through a shared encounter corridor on the island's eastern side. Intersecting routes mean combat opportunities, not forced ramming. The600 m western navigation extension is required by actual hull clearance; the original spawn sectors and terrain remain intact. Medium/Large Blue selections use the eastern defensive arc because the narrow west/north passages fail their hull connectivity checks.

Island-demo daylight is fixed at10:30 solar time with daylight-saving adjustment disabled for a clear presentation. This is a runtime scenario setting; it does not relocate the sky, globe or terrain.

## Show the audience

Start in overview to explain Blue defense and Red's incoming column. Press Q to inspect ships without taking their AI controllers. Show the colored Small hull and working bow gun, live health, team/mission state and ammunition. Point out the10-round magazine and30-second reload. Return to overview with F and watch Red close on the eastern patrol. Real projectiles, hit location, retreat and staged wrecks remain active. A completed Full AI match must show BLUE WINS or RED WINS from the native alive counters.

| Key | Action |
|---|---|
| Q | Cycle ship views; never possess an AI ship |
| F | Battlefield overview / recent ship view |
| I | Ship details panel |
| P | Pause and open match setup; start a fresh match or return |
| / | Control tutorial |
| Tab | Teammate/death spectator cycling; Full AI cycles inspected ships |

**Player + AI** keeps the original controllable ship and five independent AI in3v3. Select either player team, fleet counts, classes, difficulty and Real-Time or alternating10-second Turn-Based mode. Full AI uses a camera pawn and six independently controlled ships; no ship is possessed by the spectator.

## Manual patrol demonstration

To demonstrate moving ships or A/B markers in the editor, select **Placed ships and targets / Player + AI**. Assign valid distinct A/B TargetPoints on each navigator, keep the optional circuit array empty, and place the hull and endpoints in chart-water with enough room for turning. The generator otherwise deliberately replaces initial placements from the chosen seed. Full AI uses the authored island scenario; the placed-target option is for the existing player/five-AI map roster.

A failed setup reports its reason and keeps combat gated. Correct the path/streamed collision/clearance issue; do not keep restarting until one run happens to look acceptable. The final QA report names the tested seed/configurations and any remaining limitations. Packaging is a separate acceptance boundary from editor PIE; external terrain is still required.

## Verified scope and fallback

Use the recommended Small/Small/Small Blue versus Small/Large/Medium Red island-defense preset. Medium/Large Blue ships use the eastern arc. Do not demonstrate the unverified assault-disabled heavy-convoy reverse A/B stress layout or assume any newly selected legacy fleet/path combination is safe. For the verified original A/B demonstration use Blue Medium/Small/Medium and Red Small/Small/Medium, seed20261028, sector tactics/assault disabled in the development fixture. That exact fleet completed B/A/B/A on all five AI in667.86s; it differs from the new Full AI fleet.

The final build keeps existing user-owned mesh, Blueprint and physics assets. Small uses a static model with no skeleton; its visible gun follows the retained gun rig. Use Q to inspect the Small hull and gun before returning to overview. The global minimap scale is preserved and its labels can cluster. This is Windows Editor/PIE evidence; packaging and external-dataset portability are not certified.

## After a match

When either team wins, click **RETURN TO SETUP** on the result card. The existing setup menu keeps your selections. Choose **START NEW MATCH** to reset the match, or **BACK TO GAME** to view the finished battle; the result button remains clickable. This also works after player Victory/Defeat and wreck cleanup. See ../Testing/RETURN_TO_SETUP_QA.md for the six verified result/restart cases.
