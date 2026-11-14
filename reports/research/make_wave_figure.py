from pathlib import Path
import json
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
p=Path('Docs/Testing/SMALL_WAVE_FOLLOWUP.json')
d=json.loads(p.read_text(encoding='utf-8'))
labels=['Idle','Cruise','Turn','Reverse','Reverse-fire'];states=list(d['states'].values())
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':10,'pdf.fonttype':42})
fig,ax=plt.subplots(figsize=(7.3,3.2));x=list(range(5))
ax.bar([i-.18 for i in x],[r['pitch_p95'] for r in states],width=.34,color='#006c73',label='Pitch P95')
ax.bar([i+.18 for i in x],[r['roll_p95'] for r in states],width=.34,color='#ee0033',label='Roll P95')
ax.set(xticks=x,xticklabels=labels,ylabel='Absolute angle (degrees)',ylim=(0,3.1))
ax.spines[['top','right']].set_visible(False);ax.grid(axis='y',alpha=.18);ax.set_axisbelow(True);ax.legend(frameon=False,ncol=2,loc='upper left')
fig.tight_layout();fig.savefig('reports/images/small_wave_metrics.pdf');plt.close(fig)
print('Measured wave figure generated from',p)
