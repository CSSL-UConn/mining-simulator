#!/usr/bin/env python3
"""
Compare 3-strategy rotation against three pure strategy baselines.

Usage:
    python compare_3strategy.py <pure1> <pure2> <pure3> <rotation_results>
"""

import sys
import pandas as pd
import numpy as np

def load_results(filename):
    """Load results file, skipping comment lines."""
    data = []
    with open(filename, 'r') as f:
        for line in f:
            if line.startswith('#') or line.strip() == '':
                continue
            if line.startswith('Gamma'):
                continue  # Skip header
            parts = line.strip().split(',')
            if len(parts) >= 5:
                data.append([float(x.strip()) for x in parts])
    
    df = pd.DataFrame(data, columns=['Gamma', 'ProfitFraction', 'HashRate', 'HonestHashRate', 'BlockFraction'])
    return df

def compare_strategies(pure1_df, pure2_df, pure3_df, rotation_df, name1, name2, name3):
    """Compare 3-strategy rotation against three pure strategies."""
    
    # Group by (Gamma, HashRate) and calculate mean profit fraction
    pure1_grouped = pure1_df.groupby(['Gamma', 'HashRate'])['ProfitFraction'].mean().reset_index()
    pure2_grouped = pure2_df.groupby(['Gamma', 'HashRate'])['ProfitFraction'].mean().reset_index()
    pure3_grouped = pure3_df.groupby(['Gamma', 'HashRate'])['ProfitFraction'].mean().reset_index()
    rotation_grouped = rotation_df.groupby(['Gamma', 'HashRate'])['ProfitFraction'].mean().reset_index()
    
    # Merge all
    merged = pure1_grouped.rename(columns={'ProfitFraction': name1})
    merged = merged.merge(pure2_grouped.rename(columns={'ProfitFraction': name2}), 
                          on=['Gamma', 'HashRate'])
    merged = merged.merge(pure3_grouped.rename(columns={'ProfitFraction': name3}), 
                          on=['Gamma', 'HashRate'])
    merged = merged.merge(rotation_grouped.rename(columns={'ProfitFraction': 'Rotation'}), 
                          on=['Gamma', 'HashRate'])
    
    # Calculate comparisons
    merged['BestPure'] = merged[[name1, name2, name3]].max(axis=1)
    merged['WorstPure'] = merged[[name1, name2, name3]].min(axis=1)
    merged['AvgPure'] = merged[[name1, name2, name3]].mean(axis=1)
    
    # Differences
    merged['RotationVsBest'] = merged['Rotation'] - merged['BestPure']
    merged['RotationVsWorst'] = merged['Rotation'] - merged['WorstPure']
    merged['RotationVsAvg'] = merged['Rotation'] - merged['AvgPure']
    
    # Categorize
    def categorize(row):
        if row['Rotation'] > row['BestPure']:
            return 'BETTER_THAN_ALL'
        elif row['Rotation'] < row['WorstPure']:
            return 'WORSE_THAN_ALL'
        else:
            return 'MODERATE'
    
    merged['Category'] = merged.apply(categorize, axis=1)
    
    return merged

def print_summary(comparison_df, name1, name2, name3):
    """Print summary statistics."""
    print("\n" + "="*80)
    print("3-STRATEGY ROTATION ANALYSIS")
    print("="*80)
    print(f"Pure Strategies: {name1}, {name2}, {name3}")
    print(f"Rotation: {name1} → {name2} → {name3} (1000 blocks each)")
    
    # Overall statistics
    total_configs = len(comparison_df)
    better_count = len(comparison_df[comparison_df['Category'] == 'BETTER_THAN_ALL'])
    moderate_count = len(comparison_df[comparison_df['Category'] == 'MODERATE'])
    worse_count = len(comparison_df[comparison_df['Category'] == 'WORSE_THAN_ALL'])
    
    print(f"\nTotal configurations tested: {total_configs}")
    print(f"  Rotation BETTER than all pure: {better_count} ({100*better_count/total_configs:.1f}%)")
    print(f"  Rotation MODERATE (between): {moderate_count} ({100*moderate_count/total_configs:.1f}%)")
    print(f"  Rotation WORSE than all pure: {worse_count} ({100*worse_count/total_configs:.1f}%)")
    
    # Average improvements
    avg_vs_best = comparison_df['RotationVsBest'].mean()
    avg_vs_worst = comparison_df['RotationVsWorst'].mean()
    avg_vs_avg = comparison_df['RotationVsAvg'].mean()
    
    print(f"\nAverage profit difference:")
    print(f"  Rotation vs Best Pure: {avg_vs_best:+.6f} ({100*avg_vs_best:.3f}%)")
    print(f"  Rotation vs Worst Pure: {avg_vs_worst:+.6f} ({100*avg_vs_worst:.3f}%)")
    print(f"  Rotation vs Average Pure: {avg_vs_avg:+.6f} ({100*avg_vs_avg:.3f}%)")
    
    if avg_vs_best > 0:
        print(f"\n>>> 3-STRATEGY ROTATION BEATS ALL PURE STRATEGIES! <<<")
    elif avg_vs_avg > 0:
        print(f"\n>>> 3-STRATEGY ROTATION BEATS AVERAGE OF PURE STRATEGIES <<<")
    else:
        print(f"\n>>> Pure strategies perform better on average <<<")
    
    # Best cases for rotation
    print("\n" + "-"*80)
    print("TOP 5 CASES WHERE ROTATION WORKS BEST:")
    print("-"*80)
    top_rotation = comparison_df.nlargest(5, 'RotationVsBest')
    for idx, row in top_rotation.iterrows():
        print(f"Gamma={row['Gamma']:.2f}, HashRate={row['HashRate']:.2f}: "
              f"Rotation={row['Rotation']:.4f}, Best Pure={row['BestPure']:.4f} "
              f"(+{100*row['RotationVsBest']:.2f}%)")
    
    # Worst cases for rotation
    print("\n" + "-"*80)
    print("TOP 5 CASES WHERE ROTATION WORKS WORST:")
    print("-"*80)
    worst_rotation = comparison_df.nsmallest(5, 'RotationVsBest')
    for idx, row in worst_rotation.iterrows():
        print(f"Gamma={row['Gamma']:.2f}, HashRate={row['HashRate']:.2f}: "
              f"Rotation={row['Rotation']:.4f}, Best Pure={row['BestPure']:.4f} "
              f"({100*row['RotationVsBest']:.2f}%)")
    
    # Breakdown by gamma
    print("\n" + "-"*80)
    print("BREAKDOWN BY GAMMA (CONNECTIVITY):")
    print("-"*80)
    for gamma in sorted(comparison_df['Gamma'].unique()):
        gamma_data = comparison_df[comparison_df['Gamma'] == gamma]
        better = len(gamma_data[gamma_data['Category'] == 'BETTER_THAN_ALL'])
        total = len(gamma_data)
        avg_improvement = gamma_data['RotationVsBest'].mean()
        print(f"Gamma {gamma:.2f}: {better}/{total} better ({100*better/total:.1f}%), "
              f"avg vs best: {100*avg_improvement:+.3f}%")
    
    # Breakdown by hash rate ranges
    print("\n" + "-"*80)
    print("BREAKDOWN BY HASH RATE RANGE:")
    print("-"*80)
    comparison_df['HashRateRange'] = pd.cut(comparison_df['HashRate'], 
                                             bins=[0.14, 0.20, 0.25, 0.31],
                                             labels=['15-20%', '20-25%', '25-30%'])
    for hr_range in ['15-20%', '20-25%', '25-30%']:
        hr_data = comparison_df[comparison_df['HashRateRange'] == hr_range]
        if len(hr_data) > 0:
            better = len(hr_data[hr_data['Category'] == 'BETTER_THAN_ALL'])
            total = len(hr_data)
            avg_improvement = hr_data['RotationVsBest'].mean()
            print(f"Hash Rate {hr_range}: {better}/{total} better ({100*better/total:.1f}%), "
                  f"avg vs best: {100*avg_improvement:+.3f}%")

def save_detailed_comparison(comparison_df, output_file='comparison_3strategy.csv'):
    """Save detailed comparison to CSV."""
    comparison_df.to_csv(output_file, index=False)
    print(f"\n\nDetailed comparison saved to: {output_file}")

if __name__ == '__main__':
    if len(sys.argv) != 5:
        print("Usage: python compare_3strategy.py <pure1> <pure2> <pure3> <rotation_results>")
        print("\nExample:")
        print("  python compare_3strategy.py results_b5_pure_honest.txt results_b5_pure_selfish.txt results_b5_pure_stubborn_lead.txt results_b5_3strat_rotation.txt")
        sys.exit(1)
    
    pure1_file = sys.argv[1]
    pure2_file = sys.argv[2]
    pure3_file = sys.argv[3]
    rotation_file = sys.argv[4]
    
    # Extract strategy names from filenames
    name1 = "Honest" if "honest" in pure1_file else "Strategy1"
    name2 = "Selfish" if "selfish" in pure2_file else "Strategy2"
    name3 = "Stubborn-Lead" if "stubborn" in pure3_file else "Strategy3"
    
    print("Loading results...")
    print(f"  Pure 1: {pure1_file}")
    print(f"  Pure 2: {pure2_file}")
    print(f"  Pure 3: {pure3_file}")
    print(f"  Rotation: {rotation_file}")
    
    try:
        pure1_df = load_results(pure1_file)
        pure2_df = load_results(pure2_file)
        pure3_df = load_results(pure3_file)
        rotation_df = load_results(rotation_file)
        
        print(f"\nLoaded {len(pure1_df)} results for {name1}")
        print(f"Loaded {len(pure2_df)} results for {name2}")
        print(f"Loaded {len(pure3_df)} results for {name3}")
        print(f"Loaded {len(rotation_df)} results for rotation")
        
        comparison_df = compare_strategies(pure1_df, pure2_df, pure3_df, rotation_df, name1, name2, name3)
        print_summary(comparison_df, name1, name2, name3)
        save_detailed_comparison(comparison_df)
        
    except FileNotFoundError as e:
        print(f"\nError: Could not find file - {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
