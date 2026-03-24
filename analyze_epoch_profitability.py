#!/usr/bin/env python3
"""
Analyze when switching strategies becomes profitable compared to baseline.

Shows exact epoch when cumulative profit overtakes baseline.
"""

import sys
import pandas as pd
import numpy as np

def load_epoch_results(filename):
    """Load epoch tracking results."""
    data = []
    with open(filename, 'r') as f:
        for line in f:
            if line.startswith('#') or line.strip() == '':
                continue
            if 'Gamma' in line and 'HashRate' in line:
                continue  # Skip header
            parts = line.strip().split(',')
            if len(parts) >= 6:
                data.append([float(x.strip()) for x in parts])
    
    df = pd.DataFrame(data, columns=['Gamma', 'HashRate', 'GameNum', 'Epoch', 'ProfitFraction', 'BlockFraction'])
    return df

def analyze_profitability(baseline_df, switching_df):
    """Analyze when switching becomes profitable."""
    
    print(f"\n{'='*80}")
    print(f"EPOCH-BY-EPOCH PROFITABILITY ANALYSIS")
    print(f"{'='*80}\n")
    
    # Merge on Gamma, HashRate, GameNum, Epoch
    merged = baseline_df.merge(switching_df, 
                               on=['Gamma', 'HashRate', 'GameNum', 'Epoch'],
                               suffixes=('_baseline', '_switching'))
    
    merged['Advantage'] = merged['ProfitFraction_switching'] - merged['ProfitFraction_baseline']
    
    # Group by Gamma, HashRate, Epoch and average across games
    grouped = merged.groupby(['Gamma', 'HashRate', 'Epoch']).agg({
        'ProfitFraction_baseline': 'mean',
        'ProfitFraction_switching': 'mean',
        'Advantage': 'mean'
    }).reset_index()
    
    # Find breakeven epoch for each Gamma/HashRate combination
    print("BREAKEVEN ANALYSIS (when switching first becomes profitable):\n")
    print(f"{'Gamma':<8} {'HashRate':<10} {'Breakeven Epoch':<16} {'Advantage at Epoch':<20}")
    print("-" * 80)
    
    breakeven_summary = []
    
    for gamma in sorted(grouped['Gamma'].unique()):
        for hashrate in sorted(grouped['HashRate'].unique()):
            subset = grouped[(grouped['Gamma'] == gamma) & (grouped['HashRate'] == hashrate)]
            subset = subset.sort_values('Epoch')
            
            # Find first epoch where advantage > 0
            profitable = subset[subset['Advantage'] > 0]
            
            if len(profitable) > 0:
                breakeven_epoch = int(profitable.iloc[0]['Epoch'])
                advantage = profitable.iloc[0]['Advantage']
                breakeven_summary.append({
                    'Gamma': gamma,
                    'HashRate': hashrate,
                    'BreakevenEpoch': breakeven_epoch,
                    'Advantage': advantage
                })
                
                if hashrate >= 0.15 and hashrate <= 0.30:  # Focus on interesting range
                    print(f"{gamma:<8.2f} {hashrate:<10.3f} {breakeven_epoch:<16} {advantage:+.6f}")
    
    breakeven_df = pd.DataFrame(breakeven_summary)
    
    if len(breakeven_df) > 0:
        print(f"\n{'='*80}")
        print("SUMMARY STATISTICS")
        print(f"{'='*80}\n")
        
        print(f"Configurations where switching becomes profitable: {len(breakeven_df)}")
        print(f"Average breakeven epoch: {breakeven_df['BreakevenEpoch'].mean():.1f}")
        print(f"Median breakeven epoch: {breakeven_df['BreakevenEpoch'].median():.1f}")
        print(f"Earliest breakeven: Epoch {breakeven_df['BreakevenEpoch'].min()}")
        print(f"Latest breakeven: Epoch {breakeven_df['BreakevenEpoch'].max()}")
        
        # Breakdown by hash rate
        print(f"\n{'='*80}")
        print("BREAKEVEN BY HASH RATE RANGE")
        print(f"{'='*80}\n")
        
        hash_ranges = [
            (0.15, 0.20, "15-20%"),
            (0.20, 0.25, "20-25%"),
            (0.25, 0.30, "25-30%")
        ]
        
        for min_hash, max_hash, label in hash_ranges:
            subset = breakeven_df[(breakeven_df['HashRate'] >= min_hash) & 
                                 (breakeven_df['HashRate'] < max_hash)]
            if len(subset) > 0:
                print(f"{label}: Avg breakeven = Epoch {subset['BreakevenEpoch'].mean():.1f}, "
                      f"Avg advantage = {subset['Advantage'].mean():+.6f}")
    
    # Save detailed results
    breakeven_df.to_csv('epoch_breakeven_analysis.csv', index=False)
    print(f"\nDetailed results saved to: epoch_breakeven_analysis.csv")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python analyze_epoch_profitability.py <baseline_file> <switching_file>")
        print("\nExample:")
        print("  python analyze_epoch_profitability.py results/epoch_pure_selfish.txt results/epoch_switching.txt")
        sys.exit(1)
    
    baseline_file = sys.argv[1]
    switching_file = sys.argv[2]
    
    print(f"\nLoading epoch results...")
    print(f"  Baseline: {baseline_file}")
    print(f"  Switching: {switching_file}")
    
    baseline_df = load_epoch_results(baseline_file)
    switching_df = load_epoch_results(switching_file)
    
    print(f"\nLoaded {len(baseline_df)} epoch snapshots for baseline")
    print(f"Loaded {len(switching_df)} epoch snapshots for switching")
    
    analyze_profitability(baseline_df, switching_df)
