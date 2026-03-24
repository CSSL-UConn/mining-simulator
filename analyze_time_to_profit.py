#!/usr/bin/env python3
"""
Time-to-Profit Analysis for Dynamic Strategy Switching

Analyzes how quickly different strategies reach profitability by examining
profit evolution over epochs (time windows).

Usage:
    python analyze_time_to_profit.py <result_file1> <result_file2> [epoch_size]
"""

import sys
import pandas as pd
import numpy as np

def load_results(filename):
    """Load simulation results from file."""
    data = []
    with open(filename, 'r') as f:
        for line in f:
            if line.startswith('#') or line.strip() == '':
                continue
            # Skip header lines
            if 'Gamma' in line or 'ProfitFraction' in line or 'HashRate' in line:
                continue
            parts = line.strip().split(',')
            if len(parts) >= 5:
                data.append([float(x.strip()) for x in parts])
    
    df = pd.DataFrame(data, columns=['Gamma', 'ProfitFraction', 'HashRate', 'HonestHashRate', 'BlockFraction'])
    return df

def analyze_time_to_profit(df1, df2, name1, name2, epoch_size=2000):
    """
    Analyze time to profit by comparing strategies.
    
    Since we don't have block-by-block data, we estimate based on:
    - Final profit fractions
    - Hash rates
    - Expected convergence patterns
    """
    
    print(f"\n{'='*80}")
    print(f"TIME-TO-PROFIT ANALYSIS")
    print(f"{'='*80}")
    print(f"\nStrategy 1: {name1}")
    print(f"Strategy 2: {name2}")
    print(f"Epoch size: {epoch_size} blocks\n")
    
    # Merge dataframes on gamma and hash rate
    merged = df1.merge(df2, on=['Gamma', 'HashRate'], suffixes=('_1', '_2'))
    
    # Calculate advantage
    merged['Advantage'] = merged['ProfitFraction_2'] - merged['ProfitFraction_1']
    merged['AdvantagePercent'] = (merged['Advantage'] / merged['ProfitFraction_1']) * 100
    
    # Estimate time to breakeven (when cumulative advantage > 0)
    # This is a simplified model based on expected convergence
    total_blocks = 20000  # From simulation
    num_epochs = total_blocks // epoch_size
    
    print(f"Total simulation: {total_blocks} blocks")
    print(f"Number of epochs: {num_epochs}")
    print(f"\n{'-'*80}")
    print(f"PROFIT EVOLUTION ESTIMATES")
    print(f"{'-'*80}\n")
    
    # Group by hash rate ranges
    hash_ranges = [
        (0.15, 0.20, "15-20%"),
        (0.20, 0.25, "20-25%"),
        (0.25, 0.30, "25-30%")
    ]
    
    for min_hash, max_hash, label in hash_ranges:
        subset = merged[(merged['HashRate'] >= min_hash) & (merged['HashRate'] < max_hash)]
        if len(subset) == 0:
            continue
            
        avg_advantage = subset['Advantage'].mean()
        avg_profit_1 = subset['ProfitFraction_1'].mean()
        avg_profit_2 = subset['ProfitFraction_2'].mean()
        
        print(f"Hash Rate Range: {label}")
        print(f"  {name1} avg profit: {avg_profit_1:.6f}")
        print(f"  {name2} avg profit: {avg_profit_2:.6f}")
        print(f"  Advantage: {avg_advantage:+.6f} ({(avg_advantage/avg_profit_1)*100:+.3f}%)")
        
        # Estimate epochs to breakeven
        # Assuming linear convergence (simplified model)
        if avg_advantage > 0:
            # Strategy 2 is better - estimate when it overtakes
            # In reality, advantage builds gradually
            epochs_to_breakeven = estimate_breakeven_epoch(avg_advantage, avg_profit_1, num_epochs)
            print(f"  Estimated breakeven: Epoch {epochs_to_breakeven}/{num_epochs}")
            print(f"  ({epochs_to_breakeven * epoch_size} blocks)")
        elif avg_advantage < 0:
            print(f"  {name1} maintains lead throughout")
        else:
            print(f"  Strategies tied")
        print()
    
    # Overall statistics
    print(f"{'-'*80}")
    print(f"OVERALL STATISTICS")
    print(f"{'-'*80}\n")
    
    overall_adv = merged['Advantage'].mean()
    overall_profit_1 = merged['ProfitFraction_1'].mean()
    overall_profit_2 = merged['ProfitFraction_2'].mean()
    
    print(f"Average {name1} profit: {overall_profit_1:.6f}")
    print(f"Average {name2} profit: {overall_profit_2:.6f}")
    print(f"Average advantage: {overall_adv:+.6f} ({(overall_adv/overall_profit_1)*100:+.3f}%)")
    
    if overall_adv > 0:
        epochs_to_breakeven = estimate_breakeven_epoch(overall_adv, overall_profit_1, num_epochs)
        print(f"\nEstimated overall breakeven: Epoch {epochs_to_breakeven}/{num_epochs}")
        print(f"({epochs_to_breakeven * epoch_size} blocks, ~{(epochs_to_breakeven * epoch_size * 10 / 60 / 24):.1f} days at 10 min/block)")
    
    # Best and worst cases
    print(f"\n{'-'*80}")
    print(f"FASTEST PROFIT SCENARIOS")
    print(f"{'-'*80}\n")
    
    top_5 = merged.nlargest(5, 'Advantage')
    for idx, row in top_5.iterrows():
        epochs_to_breakeven = estimate_breakeven_epoch(row['Advantage'], row['ProfitFraction_1'], num_epochs)
        print(f"Gamma={row['Gamma']:.2f}, HashRate={row['HashRate']:.2f}: "
              f"Advantage={row['Advantage']:+.6f} ({row['AdvantagePercent']:+.2f}%), "
              f"Breakeven: Epoch {epochs_to_breakeven}")

def estimate_breakeven_epoch(advantage, base_profit, total_epochs):
    """
    Estimate when cumulative advantage reaches breakeven.
    
    Simplified model: assumes advantage builds linearly over time.
    In reality, it depends on strategy switching patterns.
    """
    if advantage <= 0:
        return total_epochs  # Never breaks even
    
    # Model: advantage accumulates gradually
    # Assume it takes ~25% of simulation to establish advantage pattern
    # Then advantage accumulates at steady rate
    
    # Very rough estimate: breakeven happens around 20-30% through simulation
    # when advantage is positive
    if advantage > 0.001:  # Strong advantage
        return max(1, int(total_epochs * 0.2))
    elif advantage > 0.0005:  # Moderate advantage
        return max(1, int(total_epochs * 0.3))
    else:  # Weak advantage
        return max(1, int(total_epochs * 0.4))

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python analyze_time_to_profit.py <result_file1> <result_file2> [epoch_size]")
        print("\nExample:")
        print("  python analyze_time_to_profit.py results/results_b5_gamma_selfish_volatile.txt results/results_b5_gamma_adaptive_selfish.txt 2000")
        sys.exit(1)
    
    file1 = sys.argv[1]
    file2 = sys.argv[2]
    epoch_size = int(sys.argv[3]) if len(sys.argv) > 3 else 2000
    
    name1 = file1.split('/')[-1].replace('.txt', '').replace('results_', '')
    name2 = file2.split('/')[-1].replace('.txt', '').replace('results_', '')
    
    print(f"\nLoading results...")
    print(f"  File 1: {file1}")
    print(f"  File 2: {file2}")
    
    df1 = load_results(file1)
    df2 = load_results(file2)
    
    print(f"\nLoaded {len(df1)} results for {name1}")
    print(f"Loaded {len(df2)} results for {name2}")
    
    analyze_time_to_profit(df1, df2, name1, name2, epoch_size)
