#!/usr/bin/env python3
"""
Extract beam position and spread from PDF reports.
Creates two tables:
1. Detailed table with all beam conditions
2. Averaged table by energy
"""

import subprocess
import re
from pathlib import Path
from collections import defaultdict
import numpy as np

# Runs excluded from aggregation due to anomalous beam conditions
# (different collimator/target config, very low statistics, etc.)
EXCLUDED_RUNS = {
    ('20250826', 5.0),   # no collimator, avg clusters 0.76, opposite-sign Mean Y vs all other 5 GeV runs
}

def extract_beam_params(pdf_path):
    """Extract beam parameters from PDF first page."""
    result = subprocess.run(['pdftotext', '-f', '1', '-l', '1', str(pdf_path), '-'],
                          capture_output=True, text=True)
    text = result.stdout

    # Parse filename for date and energy
    match = re.match(r'(\d{8})_([+-]?\d+\.?\d*)GeV_report\.pdf', pdf_path.name)
    if not match:
        return None
    
    date = match.group(1)
    energy = float(match.group(2))

    if (date, energy) in EXCLUDED_RUNS:
        print(f"  [excluded] {pdf_path.name}: in manual exclusion list")
        return None

    # Extract beam parameters (now from "1 cluster/det" section)
    mean_x_match = re.search(r'Mean X:\s*([+-]?\d+\.?\d*)\s*mm', text)
    mean_y_match = re.search(r'Mean Y:\s*([+-]?\d+\.?\d*)\s*mm', text)
    sigma_x_match = re.search(r'σ X:\s*([+-]?\d+\.?\d*)\s*mm', text)
    sigma_y_match = re.search(r'σ Y:\s*([+-]?\d+\.?\d*)\s*mm', text)
    
    # Extract average clusters per event (D0-D2)
    avg_clusters_match = re.search(r'Avg \(D0-D2\):\s*([+-]?\d+\.?\d*)', text)
    
    # Extract beam settings
    target_match = re.search(r'Target:\s*(\w+)', text)
    cherenkov_match = re.search(r'Cherenkov:\s*(.*?)(?:Collimator|Notes|$)', text, re.DOTALL)
    collimator_match = re.search(r'Collimator:\s*([\+\-/\d\s]+)', text)
    
    if mean_x_match and mean_y_match and sigma_x_match and sigma_y_match:
        mean_x  = float(mean_x_match.group(1))
        mean_y  = float(mean_y_match.group(1))
        sigma_x = float(sigma_x_match.group(1))
        sigma_y = float(sigma_y_match.group(1))

        # Reject fits that wandered to unphysical values (bad truncated-Gaussian convergence,
        # typically caused by very low statistics). Active area is ~100mm wide/tall.
        if sigma_x > 80.0 or sigma_y > 80.0 or abs(mean_x) > 100.0 or abs(mean_y) > 100.0:
            print(f"  [skip] {pdf_path.name}: unphysical fit "
                  f"(μX={mean_x:.1f}, μY={mean_y:.1f}, σX={sigma_x:.1f}, σY={sigma_y:.1f})")
            return None

        return {
            'date': date,
            'energy': energy,
            'mean_x': mean_x,
            'mean_y': mean_y,
            'sigma_x': sigma_x,
            'sigma_y': sigma_y,
            'avg_clusters': float(avg_clusters_match.group(1)) if avg_clusters_match else 0.0,
            'target': target_match.group(1).strip() if target_match else '-',
            'cherenkov': cherenkov_match.group(1).strip() if cherenkov_match else '-',
            'collimator': collimator_match.group(1).strip() if collimator_match else '-'
        }

    return None

def main():
    reports_dir = Path("/eos/project-e/ep-nu/evilla/np02-beam-monitor/reports")
    
    # Get all summary PDFs
    summary_pdfs = sorted([f for f in reports_dir.glob("202508*GeV_report.pdf")] +
                         [f for f in reports_dir.glob("202509*GeV_report.pdf")])
    
    print(f"Found {len(summary_pdfs)} summary PDF reports\n")
    
    # Extract all data
    all_data = []
    for pdf in summary_pdfs:
        params = extract_beam_params(pdf)
        if params:
            all_data.append(params)
    
    # Sort by date and energy
    all_data.sort(key=lambda x: (x['date'], x['energy']))
    
    # Print detailed table
    print("=" * 150)
    print("TABLE 1: Detailed Beam Positions for All Beam Conditions")
    print("=" * 150)
    print(f"{'Date':<12} | {'Energy':>8} | {'X (mm)':>10} | {'Y (mm)':>10} | {'σX (mm)':>10} | {'σY (mm)':>10} | {'Avg Clust':>10} | {'Target':<10} | {'Collimator':<12}")
    print("-" * 150)
    
    for d in all_data:
        print(f"{d['date']:<12} | {d['energy']:>7.1f} GeV | {d['mean_x']:>10.2f} | {d['mean_y']:>10.2f} | "
              f"{d['sigma_x']:>10.2f} | {d['sigma_y']:>10.2f} | {d['avg_clusters']:>10.2f} | {d['target']:<10} | {d['collimator']:<12}")
    
    print(f"\nTotal: {len(all_data)} beam conditions\n")
    
    # Group by energy and calculate averages
    energy_groups = defaultdict(list)
    for d in all_data:
        energy_groups[d['energy']].append(d)
    
    print("\n" + "=" * 105)
    print("TABLE 2: Averaged Beam Positions by Energy")
    print("=" * 105)
    print(f"{'Energy':>8} | {'Count':>5} | {'X (mm)':>10} | {'Y (mm)':>10} | {'σX (mm)':>10} | {'σY (mm)':>10} | {'Avg Clust':>10}")
    print("-" * 105)
    
    averaged_data = []
    for energy in sorted(energy_groups.keys()):
        data_list = energy_groups[energy]
        avg_x = np.mean([d['mean_x'] for d in data_list])
        avg_y = np.mean([d['mean_y'] for d in data_list])
        avg_sigma_x = np.mean([d['sigma_x'] for d in data_list])
        avg_sigma_y = np.mean([d['sigma_y'] for d in data_list])
        avg_clusters = np.mean([d['avg_clusters'] for d in data_list])
        
        averaged_data.append({
            'energy': energy,
            'count': len(data_list),
            'mean_x': avg_x,
            'mean_y': avg_y,
            'sigma_x': avg_sigma_x,
            'sigma_y': avg_sigma_y,
            'avg_clusters': avg_clusters
        })
        
        print(f"{energy:>7.1f} GeV | {len(data_list):>5} | {avg_x:>10.2f} | {avg_y:>10.2f} | "
              f"{avg_sigma_x:>10.2f} | {avg_sigma_y:>10.2f} | {avg_clusters:>10.2f}")
    
    print(f"\nTotal: {len(averaged_data)} unique energies\n")
    
    # Save tables to files
    output_dir = Path("/afs/cern.ch/work/e/evilla/private/dune/beam-monitor/oca-pDUNE-dataAnalyzer/analysis")
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Save detailed table
    with open(output_dir / "beam_positions_detailed.txt", 'w') as f:
        f.write("Detailed Beam Positions for All Beam Conditions\n")
        f.write("=" * 150 + "\n")
        f.write(f"{'Date':<12} | {'Energy':>8} | {'X (mm)':>10} | {'Y (mm)':>10} | {'σX (mm)':>10} | {'σY (mm)':>10} | {'Avg Clust':>10} | {'Target':<10} | {'Collimator':<12}\n")
        f.write("-" * 150 + "\n")
        for d in all_data:
            f.write(f"{d['date']:<12} | {d['energy']:>7.1f} GeV | {d['mean_x']:>10.2f} | {d['mean_y']:>10.2f} | "
                   f"{d['sigma_x']:>10.2f} | {d['sigma_y']:>10.2f} | {d['avg_clusters']:>10.2f} | {d['target']:<10} | {d['collimator']:<12}\n")
    
    # Save averaged table
    with open(output_dir / "beam_positions_by_energy.txt", 'w') as f:
        f.write("Averaged Beam Positions by Energy\n")
        f.write("=" * 105 + "\n")
        f.write(f"{'Energy':>8} | {'Count':>5} | {'X (mm)':>10} | {'Y (mm)':>10} | {'σX (mm)':>10} | {'σY (mm)':>10} | {'Avg Clust':>10}\n")
        f.write("-" * 105 + "\n")
        for d in averaged_data:
            f.write(f"{d['energy']:>7.1f} GeV | {d['count']:>5} | {d['mean_x']:>10.2f} | {d['mean_y']:>10.2f} | "
                   f"{d['sigma_x']:>10.2f} | {d['sigma_y']:>10.2f} | {d['avg_clusters']:>10.2f}\n")
    
    print(f"✓ Saved detailed table to: {output_dir / 'beam_positions_detailed.txt'}")
    print(f"✓ Saved averaged table to: {output_dir / 'beam_positions_by_energy.txt'}")

if __name__ == "__main__":
    main()
