"""Render actual before/after samples; never synthesize missing data."""
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path(__file__).resolve().parents[2]
runs=[('Before: vertical1.8 / body0.55',root/'Saved/HULL_STABILITY_BEFORE.json','#aa5264'),('After: vertical3.6 / body0.9 / 6 supports',root/'Saved/HULL_STABILITY_AFTER.json','#178892')]
labels=[('Small','BP_ShipEnemy2'),('Medium','BP_ShipEnemy'),('Large','Friendly_AI_1')]
fig,axs=plt.subplots(4,3,figsize=(13,10),sharex='col');metrics=[('roll','Roll (deg)'),('vz_mps','Vertical velocity (m/s)'),('heave_m','Root above wave (m)'),('speed_mps','Speed (m/s)')]
for title,path,color in runs:
 d=json.loads(path.read_text(encoding='utf-8'));assert d['rows'];rows=d['rows'];t0=rows[0]['t']
 for col,(kind,label) in enumerate(labels):
  points=[(r['t']-t0,s) for r in rows for s in r['ships'] if s['ship']==label]
  assert len(points)>500
  for row,(key,ylabel) in enumerate(metrics):
   axs[row,col].plot([t for t,s in points],[s[key] for t,s in points],lw=.75,alpha=.85,color=color,label=title)
   axs[row,col].grid(alpha=.18)
   if col==0:axs[row,col].set_ylabel(ylabel)
  axs[0,col].set_title(kind+' / '+label,fontsize=11);axs[-1,col].set_xlabel('Measured game seconds')
axs[0,0].legend(frameon=False,fontsize=9);fig.suptitle('QuangNinh fixed-seed physical patrol: 120 seconds per hull\nLarge damping and central supports tuned; original drive, waves and collision retained',fontsize=13)
fig.tight_layout(rect=[0,0,1,.94]);p=root/'Docs/QuangNinhIntegration/HULL_STABILITY_COMPARISON.png';fig.savefig(p,dpi=140);print(p)
