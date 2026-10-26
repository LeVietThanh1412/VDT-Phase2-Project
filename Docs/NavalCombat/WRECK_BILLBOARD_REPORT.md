# Wreck fire and smoke fallback

The existing Niagara death effect and geographic sinking sequence remain intact.
Five hidden `ShipWreckBillboardComponent` instances were added to the shared
BP_Ship: three fire patches and two smoke plumes. They use two unlit surface
materials, existing project textures, no collision, no navigation influence and
no shadow. They activate from the existing shared death event, follow the wreck
along geographic up even after capsize, hide at Submerged, and are owned and
cleaned with the wreck. There is no additional destruction timer or scheduler.

The first prototype passed activation checks but failed visual inspection:
flames were effectively invisible in this daylight scene. Render diagnostics
confirmed unit scale, populated materials, visible owner and main-pass rendering.
Increasing emissive intensity from 8 to 25000 made fire visibly appear in the
capsized-wreck screenshots. Smoke uses a soft radial opacity fallback because
the original packed smoke texture's selected channel did not show clearly.
Fire was subsequently tinted orange. Final saved-component visual verification
passed: `WRECK_BILLBOARD_HEALTH_A00002.png` visibly shows orange deck flames and
dark rising smoke in daylight. `WRECK_BILLBOARD_CAPSIZE_A00000.png` records the
earlier red-emission prototype on a rolled wreck. This is a lightweight billboard
presentation, not volumetric smoke. Existing red Blueprint sensor debug lines
remain visible in these evidence images; final debug cleanup is a separate task.

`WRECK_BILLBOARD_SETUP.json` records the protected BP backup, before/after hashes,
five saved component names, material paths and original force-graph validation.
The two materials are local project assets; external terrain data is untouched.
`WRECK_BILLBOARD_BUILD.json` records the successful normal native build.

The saved-level `SPECTATOR_HEALTH.json` runtime test passed: five effects hidden
alive, active and visible after real health death, hidden when submerged, and
all Blue wreck actors cleaned while the independent spectator camera survives.
Actual screenshots were checked separately from these visibility flags.

The final saved-component five-wreck lifecycle regression passed **66/66**:
health and confirmed capsize deaths, Large/Small victims, shared one-time death,
five active non-colliding fallback components per wreck, progressive 65 m sink,
explicit geographic rebase, hiding after submersion and cleanup of all wrecks.
`WRECK_SAVED_DESTRUCTION.json[.gz]` contains summary and lossless telemetry.
The initial short-roll fixture failed because a slow frame skipped its 0.22–0.3 s
upright-reset window, leaving the test subject capsized. The saved failure is
`WRECK_SHORT_ROLL_FIXTURE_FAILURE.json`. The fixture now resets upright on the
first frame after 0.22 s, once, with no narrow upper bound. The gameplay capsize
threshold was unchanged; the repeated complete fixture passed.
