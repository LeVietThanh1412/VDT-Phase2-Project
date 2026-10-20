# Physical AI accuracy verification

PASSED. Six seeded scenarios used the original Blueprint projectile classes, animated muzzle, gravity, collision and recoil. Each scenario fired 24 shots. Results are observations for these fixtures, not promised gameplay hit probabilities.

| Target | Skill | Shots | Hull hits | Misses | Hit ratio | Peak target speed |
|---|---|---:|---:|---:|---:|---:|
| stationary | Easy | 24 | 1 | 23 | 4.2% | 0.00 m/s |
| stationary | Normal | 24 | 12 | 12 | 50.0% | 0.00 m/s |
| stationary | Hard | 24 | 24 | 0 | 100.0% | 0.00 m/s |
| moving | Easy | 24 | 5 | 19 | 20.8% | 14.80 m/s |
| moving | Normal | 24 | 9 | 15 | 37.5% | 14.62 m/s |
| moving | Hard | 24 | 19 | 5 | 79.2% | 14.55 m/s |

The stationary target presents its narrow bow at approximately 900 m. Moving targets use real geographic A*, original propulsion and rudder. Their measured speeds vary during acceleration and turns; the moving cases do not assert identical speed distributions. Hard also outperformed Easy in the controlled stationary comparison.

The fixture disables damage, isolates unrelated ships, fixes fixture placements at scenario boundaries and temporarily uses a 1-second AI cooldown to gather samples. It drains existing rounds for 10 seconds before each next scenario. Saved gameplay cooldowns and placements are unchanged.

Every observed launch aligns with the actual animated muzzle (dot product > 0.999). Angular error changes the physical launch vector and joint aiming together. No hit results are edited. AI targeting and arc checks also conservatively reject geographic chart land when Cesium collision is not loaded.

Current Normal defaults: AIAccuracy 0.6, AimErrorDegrees 2.2, tracking delay 0.35 s, update interval 0.4 s, acquisition 0.6 s. Easy and Hard use slower/wider and faster/narrower settings respectively. Custom preserves individual tuning. AccuracySeed makes each named ship stream repeatable.

Evidence: ACCURACY_PIE.json. Earlier Normal tuning is retained in ACCURACY_PRIOR_NORMAL_TUNING.json and is superseded by this run.
