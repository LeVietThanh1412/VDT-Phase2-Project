# Red assault: verified mission movement

UNavalBattlefieldSubsystem::UpdateAssault uses existing controllers, hull physics and Geographic A*. No ship transform/rotation/velocity or force is supplied by the convoy manager. It only assigns logical goals and cruise-speed intent through the established navigator. SetAssaultGoal moves the existing B marker (not the ship), validates the native route and starts the same patrol graph.

The selected Small Red slot0 leaves first. Each follower waits for its usable lower-slot predecessor to travel80m and for FormationDepartureIntervalSeconds (default3s) since its release. After20 active seconds a stalled predecessor no longer blocks motion, but the interval still applies. FormationSpacingM defaults240m (editable180..350). Followers slow independently when too close/ahead; the leader slows if a usable follower falls farther than2.5 spacings. Ships independently route around unsafe formations.

Mission phases:0 initial approach,1 central crossing,2 Blue contact region,3 safe flank/reposition around actual hostile contact. Each advance requires actual arrival and low physical speed. Temporary combat retreat retains the objective and original A/B actor identities. Leadership excludes dead ships, failed navigation, repeated recovery, retreat and exhausted mission retries. Replacement prefers the most advanced phase then lowest slot. Missing targets fall back to recent contact (15s) or the fixed Blue core. Goal failures retry at bounded15s intervals, three attempts, then report explicit failure; phase waiting is bounded20 active seconds. Failed routes are not turned into teleportation.

Configuration currently remains development-gated by NavalMatchSettings use_sector_battlefield and enable_sector_tactics; enable_red_assault defaults true. Production rollout waits for combined gameplay acceptance.

Actual real-editor evidence:
- RED_ASSAULT_NAVIGATION.json: PASS10 checks, final t245.21s; released slot0 at10.2744, slot1 at19.1892, slot2 at24.4121. Actual20m departures16.0655,21.8179,26.9613. All three phase3, travelled1043m/1038m/1121m, all measured arrivals within83m.
- RED_ASSAULT_LEADER_LOSS.json: PASS5 checks, staged hostile health death at~42.9s, native leader promotion and both survivors physically continuing>100m after death; ended132.83s. Original controllers and simulation retained.

These are deliberately combat-disabled mission tests. They do not claim natural full-battle, ammo, stability or final-main acceptance. First interrupted sequential run and stale destroyed-actor Python wrapper failures are preserved under Saved. The fixed sampler queries currently live actors instead of dereferencing GC-invalid wrappers. No gameplay acceptance threshold was relaxed.
