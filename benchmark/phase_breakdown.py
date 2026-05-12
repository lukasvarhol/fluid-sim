import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import glob

files = glob.glob("logs/*.csv")
df = pd.concat([pd.read_csv(f, sep=',') for f in files])

df.groupby(['backend', 'particle_count', 'phase'])['elapsed_us'].mean()

df = df[df['frame'] >= 100] #filter early frames out

phase_df = df[df['particle_count'] == 20000].copy()
mask = ~((phase_df['phase'] == 'build_neighbours') & (phase_df['elapsed_us'] < 40))
phase_df = phase_df[mask]

phase_summary = phase_df.groupby(['backend', 'phase'])['elapsed_us'].agg(['mean', 'std']).reset_index()
phase_summary['elapsed_ms'] = phase_summary['mean'] / 1000
phase_summary = phase_summary.sort_values(by='elapsed_ms', ascending=True)

parallel_data = phase_summary[phase_summary['backend'] == 'cpu-parallel']
sequential_data = phase_summary[phase_summary['backend'] == 'cpu-sequential']

#print(phase_summary)

plt.rcParams.update({
    'font.family': 'Courier New',
    'font.size': 20,
    'axes.titlesize': 20,
    'axes.labelsize': 20,
    'xtick.labelsize': 20,
    'ytick.labelsize': 12,
    'figure.titlesize': 20,
    'figure.facecolor': "#282828",
    'axes.facecolor': "#282828",
    'axes.edgecolor': "#4c4c4c",
    'grid.color': "#4c4c4c4c",
    'text.color': '#CCCCCC',
    'axes.labelcolor': '#CCCCCC',
    'xtick.color': '#CCCCCC',
    'ytick.color': '#CCCCCC',
    'legend.facecolor': '#282828',
    'legend.edgecolor': '#4c4c4c'
    })

fig, ax = plt.subplots(figsize=(11,10))
ax.set_xlabel("Time (ms)")
ax.set_ylabel("Phase")

phases = phase_summary['phase'].unique()
x = np.arange(len(phases))
width = 0.35

ax.barh(x - width/2, sequential_data['elapsed_ms'], width,
        xerr=sequential_data['std']/1000,
        label="cpu-sequential", color="#7550E3",
        ecolor="#cccccc", capsize=4, alpha=0.8)
ax.barh(x + width/2, parallel_data['elapsed_ms'], width,
        xerr=parallel_data['std']/1000,
        label="cpu-parallel", color="#50C7BB",
        ecolor="#cccccc", capsize=4, alpha=0.8)
ax.set_yticks(x)
ax.set_yticklabels(phases)
ax.set_xscale('log')

ax.legend(
    loc = 'upper center',
    bbox_to_anchor = (0.5, 1.10),
    ncol = 2,
    frameon = False,
    markerscale = 5
    )

plt.tight_layout()

plt.savefig("figures/per-phase-breakdown.svg")
plt.show()
