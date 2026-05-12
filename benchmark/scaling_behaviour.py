import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import glob

files = glob.glob("logs/*.csv")
df = pd.concat([pd.read_csv(f, sep=',') for f in files])

df.groupby(['backend', 'particle_count', 'phase'])['elapsed_us'].mean()

df = df[df['frame'] >= 100] #filter early frames out
total = df.groupby(['backend', 'particle_count', 'frame'])['elapsed_us'].sum().reset_index()

summary = total.groupby(['backend', 'particle_count'])['elapsed_us'].agg(['mean', 'std']).reset_index()
summary['elapsed_ms'] = summary['mean'] / 1000

parallel_data = summary[summary['backend'] == 'cpu-parallel']
sequential_data = summary[summary['backend'] == 'cpu-sequential']

#print(summary)

plt.rcParams.update({
    'font.family': 'Courier New',
    'font.size': 20,
    'axes.titlesize': 20,
    'axes.labelsize': 20,
    'xtick.labelsize': 20,
    'ytick.labelsize': 20,
    'figure.titlesize': 20
    })

fig, ax = plt.subplots(figsize=(10,10))
ax.set_xlabel("Particle Count")
ax.set_ylabel("Time (ms)")

plt.margins(x=0)
plt.xscale('log')
plt.yscale('log')

ax.grid(True, which='major', linestyle='-', linewidth=0.5, alpha=0.25)
ax.minorticks_on()
ax.grid(True, which='minor', linestyle='-', linewidth=0.5, alpha=0.25)
ax.set_axisbelow(True)

ax.plot(sequential_data['particle_count'], sequential_data['elapsed_ms'],
        linewidth = 3,
        color = "#778873"
        )
ax.errorbar(sequential_data['particle_count'], sequential_data['elapsed_ms'],
            yerr=sequential_data['std'] / 1000,
            linewidth = 1.5,
            color="#778873",
            label='cpu-sequential',
            capsize=3)

ax.plot(parallel_data['particle_count'], parallel_data['elapsed_ms'],
        linewidth = 3,
        color = "#A1BC98"
        )

ax.errorbar(parallel_data['particle_count'], parallel_data['elapsed_ms'],
            yerr=parallel_data['std'] / 1000,
            linewidth=1.5,
            color="#A1BC98",
            label='cpu-parallel',
            capsize=3)

x = np.array([100, 100000])
y_linear = x * (sequential_data['elapsed_ms'].iloc[0] / 100)
y_quadratic = x**2 * (sequential_data['elapsed_ms'].iloc[0] / 100**2)
ax.plot(x, y_linear, '--', color='#313647', linewidth=1, label='O(n)', alpha=0.5)
ax.plot(x, y_quadratic, ':', color='#435663', linewidth=1, label=r'O(n$^2$)', alpha=0.5)
ax.set_xlim(left=100)
ax.set_ylim(top=1000)

ax.legend(
    loc = 'upper center',
    bbox_to_anchor = (0.5, 1.15),
    ncol = 2,
    frameon = False
    )

plt.savefig("figures/scaling-behaviour.svg")

plt.show()


