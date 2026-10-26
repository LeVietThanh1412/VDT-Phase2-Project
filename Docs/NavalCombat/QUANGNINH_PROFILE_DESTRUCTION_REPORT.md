# QuangNinh ship profiles and destruction

## Large vertical damping correction

Large uses 240 kg versus Small's 150 kg. Its former 1.2x vertical damping multiplier reduced damping per kg despite 1.6x mass/displacement. A real 65 s mixed-class frame probe found Large Enemy2 briefly leaving wave contact for up to 0.656 s and reaching 7.079 m root height. Downward terrain probes during every loss found **no supporting terrain floor**; this was extra heave, not artificial grounding.

Changed only Large's vertical damping multiplier from 1.2 to 1.8 in the existing data asset and its native fallback default. Mass, inertia, COM, force scales, buoyancy/displacement, mesh, original force graph and patrol geometry are retained. The original asset is backed up under Saved/QuangNinhProfileDampingRecovery. `LARGE_VERTICAL_DAMPING_CHANGE.json` records all retained values. Native build passed (4 actions, 10.16 s); editor restarted and all seven battle Blueprints compiled.

The repeated frame probe passed 4/4: longest contact loss 0.467 s, peak root height 6.458 m and no terrain floor hits. Large Enemy2 frame contact was 95.84%; other Large Friendly2 remained in contact throughout. Constant immersion is not claimed. Ocean phase is not fixed by the test seed, so these are observed runs, not a controlled hydrodynamic study.

| Check | Type | Result | Evidence |
|---|---|---|---|
| Small player input / turning / flotation | Real original-input PIE | PASSED, 41.99 m/s² initial acceleration, 2.466 deg/s peak yaw in the input window | QUANGNINH_FINAL_PROFILE_SMALL_PHYSICS.json |
| Large after damping correction | Real original-input PIE | PASSED, 27.49 m/s² initial acceleration, 0.247 deg/s peak yaw | QUANGNINH_FINAL_PROFILE_LARGE_PHYSICS.json |
| Small actual bow/midship/stern damage and guards | Real original projectiles | PASSED 33/33, 8/10/8 damage, both shooter and victim Small | QUANGNINH_FINAL_SMALL_DAMAGE.json |
| Large actual bow/midship/stern damage and guards | Real original projectiles | PASSED 33/33, both shooter and victim Large; vertical-damping-only change does not alter this frozen impact fixture | QUANGNINH_FINAL_LARGE_DAMAGE.json |
| Two Large plus three Small AI, saved layout | 60.50 s real PIE physics | PASSED 7/7, 895–1040 m per AI, Large 920/980 m, independent controllers, valid chart water, no Failed states/persistent errors | QUANGNINH_FINAL_MIXED_SUMMARY.json |
| Mixed-class frame flotation after damping | 65 s real PIE physics | PASSED 4/4, maximum continuous loss 0.467 s | ../QuangNinhIntegration/FLOTATION_MIXED_AFTER_DAMPING.json |
| Health/capsize/sinking/georeference/cleanup | Real PIE lifecycle | PASSED 51/51 | QUANGNINH_DESTRUCTION_PIE.json |

These remain differentiated arcade physics profiles on the same placeholder ship geometry. The short yaw measurement is a response measurement, not a maximum sustained turning specification. Mixed navigation establishes physical progress and safe tracks, not completed Large-class B/A/B loops.

## Preserved destruction behavior

The native health/capsize death handler immediately stops navigation, weapons, buoyancy and rigid-body simulation, then owns the geographic wreck animation. Five simultaneous health/capsize victims retained their hulls during burn, progressively sank 65 m, preserved longitude/latitude and capsize orientation through a deliberate runtime georeference change, and cleaned up safely. A brief recoverable roll did not kill the remaining ship; team death counters stayed stable.

The first current-map fixture failed after actual actor cleanup because UE 5.6 could not marshal a stale Python Pawn wrapper into IsValid. A narrow TypeError guard now treats that already-destroyed wrapper as invalid; the native lifecycle is unchanged. The failure is retained in `../QuangNinhIntegration/DESTRUCTION_STALE_WRAPPER_FAILURE.json[.gz]`.

Active Niagara simulation and an unhidden wreck are **not visual proof of fire/smoke**. The 51-check lifecycle pass does not mark destruction presentation complete. A bounded camera-facing billboard fallback is being prepared separately. Original effects and working sinking remain intact.

Compact summaries link to lossless gzip telemetry. A first-frame Blueprint sensor warmup in the mixed run is retained separately from actual Failed states or persistent sensor errors; it is not hidden or counted as a pathfinding failure.
