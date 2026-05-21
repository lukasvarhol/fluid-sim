import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import glob

files = glob.glob("logs/*.csv")
df = pd.concat([pd.read_csv(f, sep=',') for f in files])
df = df[df['phase'].isin(['collision_sdf', 'collision_tri_brute'])]
df = df[df['collider_count'] == 10] 
df = df[df['frame'] >= 100] #filter early frames out

per_frame = df.groupby(['phase', 'particle_count', 'frame'])['elapsed_us'].sum().reset_index()

summary = per_frame.groupby(['phase', 'particle_count'])['elapsed_us'].agg(['mean','std']).reset_index()
summary['elapsed_ms'] = summary['mean'] / 1000

sdf_data = summary[summary['phase'] == 'collision_sdf'].sort_values('particle_count')
tri_data = summary[summary['phase'] == 'collision_tri_brute'].sort_values('particle_count')

print(summary)

plt.rcParams.update({
    'font.family': 'Courier New',
    'font.size': 20,
    'axes.titlesize': 20,
    'axes.labelsize': 20,
    'xtick.labelsize': 20,
    'ytick.labelsize': 20,
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

fig, ax = plt.subplots(figsize=(10,10))
ax.set_xlabel("Particle Count")
ax.set_ylabel("Time (ms)")

plt.margins(x=0)
plt.xscale('log')
plt.yscale('log')

ax.grid(True, which='major', linestyle='-', linewidth=0.5, alpha=1.0)
ax.minorticks_on()
ax.grid(True, which='minor', linestyle='-', linewidth=0.5, alpha=1.0)
ax.set_axisbelow(True)

ax.plot(tri_data['particle_count'], tri_data['elapsed_ms'],
        linewidth = 3,
        color = "#7550E3"
        )
ax.errorbar(tri_data['particle_count'], tri_data['elapsed_ms'],
            yerr=tri_data['std'] / 1000,
            linewidth = 1.5,
            color="#7550E3",
            label='Triangle Collisions',
            capsize=3)

ax.plot(sdf_data['particle_count'], sdf_data['elapsed_ms'],
        linewidth = 3,
        color = "#50C7BB"
        )

ax.errorbar(sdf_data['particle_count'], sdf_data['elapsed_ms'],
            yerr=sdf_data['std'] / 1000,
            linewidth=1.5,
            color="#50C7BB",
            label='SDF Collisions',
            capsize=3)

x = np.array([tri_data['particle_count'].min(), tri_data['particle_count'].max()])
y_linear = x * (tri_data['elapsed_ms'].iloc[0] / tri_data['particle_count'].iloc[0])

ax.plot(x, y_linear, '--', color='#cccccc', linewidth=1, label='O(n)', alpha=1.0)
ax.set_xlim(left=200)
ax.set_xlim(right=50000)
#ax.set_ylim(top=1000)

ax.legend(
    loc = 'upper center',
    bbox_to_anchor = (0.5, 1.15),
    ncol = 3,
    frameon = False
    )

plt.savefig("figures/colision-cost-particle-count.svg")

plt.show()

