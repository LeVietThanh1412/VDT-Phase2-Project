# Blue western-island defense

Status: VERIFIED in the staged current-field test; native compilation and all four navigation automation tests passed. Sector tactics remain disabled by default during development. No claim of completed Red assault or full demo readiness.

Each Blue AI keeps its original A/B patrol markers and AIController. Its defense anchor is the midpoint of its seeded patrol endpoints. The configurable DefensiveLeashM defaults to 800 m. The human player keeps manual movement and aim.

The existing shared target scanner applies faction and island LOS checks plus a defender policy: enemies must be west of chart East 14750 m and within the anchor leash plus a 250 m interception buffer. A target outside that region is not retained at the expense of an incoming local threat. This does not replace team identity or the final damage guard.

The battlefield subsystem checks mission intent every 0.5 seconds. It searches safe firing positions through the same per-hull geographic A*, bounded by the defensive region, with up to 150 m extra route allowance for safe island detours. It does not apply forces or alter ship transforms/velocity. The existing Blueprint propulsion and rudder remain the only drive actuators.

Interception goals use the actual shared combat thresholds, rather than assumed C++ defaults. The current saved minimum retreat distance is 450 m and safe re-engagement distance 800 m. Desired stand-off is the larger of minimum retreat distance plus 150 m or 60 percent of gun engagement range; candidates remain at least minimum retreat distance plus 80 m from the threat. Replanning is checked every five seconds, but an unchanged goal within 100 m retains its current route and recovery state. A failed interception returns to patrol and waits 30 seconds before another attempt.

Shared proximity/health retreat has priority over mission interception. Target loss, death, out-of-region movement and low health release the temporary mission goal and preserve original patrol references. Navigation startup/sensors, hull sweeps, traffic avoidance, weapon animation, ballistic LOS, projectile/damage guards and physics are shared with the existing ship implementation.

The staged verify_blue_defense.py fixture uses real menu/travel and sends two retained Red ships along native validated routes. It never teleports or directly drives any ship. It deliberately applies hostile low-health damage and target health deaths through the existing damage authority. It checks both defenders' interception, shared fire/retreat, bounded region behavior, controller/physics continuity and physical patrol return. These controlled damage/death stages are not natural full-battle evidence.

The first diagnostic exposed an invalid staged Red destination, an incompatible assumed 400 m stand-off, and out-of-region target retention. Its FAILED result is preserved in BLUE_DEFENSE_FIRST_DIAGNOSTIC.json; raw samples and the second close-approach diagnostic remain in Saved. Later acceptance must be a fresh actual PIE result.


## Verified current-field result

BLUE_DEFENSE_PIE.json passed all ten checks over 314.95 game seconds (618 observed samples). The Small defender intercepted once and the Medium defender twice; both fired through the shared gun (2 and 9 accepted shots). The Small ship had one prior proximity retreat; after explicit hostile damage to 11.4/38 HP, its shared retreat counter increased again. Both staged hostile health deaths used the existing damage authority, leaving the third Red ship alive so the test did not terminate on victory.

Both original A/B references, AIControllers and continuous physical simulation were preserved. After disengagement, both had valid original patrol routes and traveled at least 25 m physically. Maximum measured distances from their defense anchors were 692.82 m and 281.60 m, within the 800 m leash. Raw samples were hash-verified into Saved/BLUE_DEFENSE_PIE_RAW.json; the compact report retains checks, summaries and first/final observations. This is controlled behavioral acceptance, not natural full-combat or buoyancy-tuning evidence.
