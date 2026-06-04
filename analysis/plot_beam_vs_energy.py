#!/usr/bin/env python3
"""
Plot beam parameters (average clusters, sigma X, sigma Y) as a function of energy.
Uncertainty bars show the standard deviation across runs at the same energy.
"""

import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
from collections import defaultdict


def read_detailed_data(filepath):
    """Read per-run detailed table and group by energy."""
    groups = defaultdict(lambda: {'sigma_x': [], 'sigma_y': [], 'avg_clusters': []})

    with open(filepath, 'r') as f:
        lines = f.readlines()

    for line in lines[4:]:
        if not line.strip() or line.startswith('Total:') or line.startswith('=') or line.startswith('-'):
            continue
        parts = line.split('|')
        if len(parts) < 7:
            continue
        try:
            energy = float(parts[1].replace('GeV', '').strip())
            sx     = float(parts[4].strip())
            sy     = float(parts[5].strip())
            ac     = float(parts[6].strip())
            if 1.0 <= energy < 10.0:
                groups[energy]['sigma_x'].append(sx)
                groups[energy]['sigma_y'].append(sy)
                groups[energy]['avg_clusters'].append(ac)
        except (ValueError, IndexError):
            continue

    energies, sx_mean, sx_err, sy_mean, sy_err, ac_mean, counts = [], [], [], [], [], [], []
    for e in sorted(groups):
        g = groups[e]
        n = len(g['sigma_x'])
        energies.append(e)
        counts.append(n)
        sx_mean.append(np.mean(g['sigma_x']))
        sy_mean.append(np.mean(g['sigma_y']))
        ac_mean.append(np.mean(g['avg_clusters']))
        # std dev across runs (0 if only one run)
        sx_err.append(np.std(g['sigma_x'], ddof=1) if n > 1 else 0.0)
        sy_err.append(np.std(g['sigma_y'], ddof=1) if n > 1 else 0.0)

    return (np.array(energies), np.array(counts),
            np.array(sx_mean), np.array(sx_err),
            np.array(sy_mean), np.array(sy_err),
            np.array(ac_mean))


def main():
    data_file = Path("beam_positions_detailed.txt")
    energies, counts, sx_mean, sx_err, sy_mean, sy_err, ac_mean = read_detailed_data(data_file)

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 9))
    fig.suptitle('Beam Parameters vs Energy (1-8 GeV)', fontsize=16, fontweight='bold')

    # --- Top: average clusters ---
    ax1.plot(energies, ac_mean, 'o-', color='#2E86AB', linewidth=2, markersize=8, label='Avg Clusters (D0-D2)')
    ax1.set_xlabel('Energy [GeV]', fontsize=12)
    ax1.set_ylabel('Average Clusters per Event', fontsize=12)
    ax1.set_title('Average Number of Clusters (Detectors 0-2)', fontsize=13)
    ax1.grid(True, alpha=0.3)
    ax1.legend(fontsize=11)

    # --- Bottom: sigma X and sigma Y with error bars ---
    ax2.errorbar(energies, sx_mean, yerr=sx_err, fmt='s-', color='#A23B72',
                 linewidth=2, markersize=8, capsize=4, capthick=1.5, elinewidth=1.5,
                 label='σ_X (Horizontal)')
    ax2.errorbar(energies, sy_mean, yerr=sy_err, fmt='^-', color='#F18F01',
                 linewidth=2, markersize=8, capsize=4, capthick=1.5, elinewidth=1.5,
                 label='σ_Y (Vertical)')

    ax2.set_xlabel('Energy [GeV]', fontsize=12)
    ax2.set_ylabel('Beam Spread [mm]', fontsize=12)
    ax2.set_title('Beam Spread (σ_X and σ_Y)', fontsize=13)
    ax2.grid(True, alpha=0.3)
    ax2.legend(fontsize=11)

    plt.tight_layout()

    output_file = Path("beam_parameters_vs_energy.png")
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"✓ Saved plot to: {output_file}")

    output_pdf = Path("beam_parameters_vs_energy.pdf")
    plt.savefig(output_pdf, bbox_inches='tight')
    print(f"✓ Saved plot to: {output_pdf}")

    print(f"\nSummary Statistics (1-8 GeV):")
    print(f"  Energy range: {energies.min():.1f} - {energies.max():.1f} GeV")
    for e, n, sx, dsx, sy, dsy in zip(energies, counts, sx_mean, sx_err, sy_mean, sy_err):
        print(f"  {e:.1f} GeV (n={n}): σX={sx:.1f}±{dsx:.1f}  σY={sy:.1f}±{dsy:.1f} mm")


if __name__ == "__main__":
    main()
