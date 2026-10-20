"""Write measured accuracy evidence only after the physical fixture passes."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[2]
d=json.loads((root/'Docs/NavalCombat/ACCURACY_PIE.json').read_text())
assert d.get('status')=='ACCURACY_CHECKS_PASSED' and d['error'] is None
assert all(x['passed'] for x in d['checks'])
lines=['# Physical AI accuracy verification','',
       'PASSED. Six seeded scenarios used the original Blueprint projectile classes, animated muzzle, gravity, collision and recoil. Each scenario fired 24 shots. Results are observations for these fixtures, not promised gameplay hit probabilities.','',
       '| Target | Skill | Shots | Hull hits | Misses | Hit ratio | Peak target speed |',
       '|---|---|---:|---:|---:|---:|---:|']
for r in d['results']:
    lines.append(f"| {r['mode']} | {r['skill']} | {r['shots']} | {r['hits']} | {r['misses']} | {r['hit_ratio']:.1%} | {r['max_target_speed']:.2f} m/s |")
lines+=['',
        'The stationary target presents its narrow bow at approximately 900 m. Moving targets use real geographic A*, original propulsion and rudder. Their measured speeds vary during acceleration and turns; the moving cases do not assert identical speed distributions. Hard also outperformed Easy in the controlled stationary comparison.',
        '',
        'The fixture disables damage, isolates unrelated ships, fixes fixture placements at scenario boundaries and temporarily uses a 1-second AI cooldown to gather samples. It drains existing rounds for 10 seconds before each next scenario. Saved gameplay cooldowns and placements are unchanged.',
        '',
        'Every observed launch aligns with the actual animated muzzle (dot product > 0.999). Angular error changes the physical launch vector and joint aiming together. No hit results are edited. AI targeting and arc checks also conservatively reject geographic chart land when Cesium collision is not loaded.',
        '',
        'Current Normal defaults: AIAccuracy 0.6, AimErrorDegrees 2.2, tracking delay 0.35 s, update interval 0.4 s, acquisition 0.6 s. Easy and Hard use slower/wider and faster/narrower settings respectively. Custom preserves individual tuning. AccuracySeed makes each named ship stream repeatable.',
        '',
        'Evidence: ACCURACY_PIE.json. Earlier Normal tuning is retained in ACCURACY_PRIOR_NORMAL_TUNING.json and is superseded by this run.']
(root/'Docs/NavalCombat/ACCURACY_REPORT.md').write_text('\n'.join(lines)+'\n')
print('Physical accuracy report written')
