#!/usr/bin/env python3
"""
Compare results from three B5 simulation runs:
1. Pure stubborn-fork
2. Pure stubborn-trail-fork  
3. Dynamic switching (fork ↔ stubborn-trail-fork)

Usage:
    python compare_results.py results_b5_pure_fork.txt results_b5_pure_stubborn_trail_fork.txt results_b5_dynamic_switching.txt
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

def compare_strategies(fork_df, trail_fork_df, dynamic_df):
    """Compare the three strategies and generate summary statistics."""
    
    # Group by (Gamma, HashRate) and calculate mean profit fraction
    fork_grouped = fork_df.groupby(['Gamma', 'HashRate'])['ProfitFraction'].mean().reset_index()
    trail_fork_grouped = trail_fork_df.groupby(['Gamma', 'HashRate'])['ProfitFraction'].mean().reset_index()
    dynamic_grouped = dynamic_df.groupby(['Gamma', 'HashRate'])['ProfitFraction'].mean().reset_index()
    
    # Merge all three
    merged = fork_grouped.rename(columns={'ProfitFraction': 'Fork'})
    merged = merged.merge(trail_fork_grouped.rename(columns={'ProfitFraction': 'TrailFork'}), 
                          on=['Gamma', 'HashRate'])
    merged = merged.merge(dynamic_grouped.rename(columns={'ProfitFraction': 'Dynamic'}), 
                          on=['Gamma', 'HashRate'])
    
    # Calculate comparisons
    merged['BestStatic'] = merged[['Fork', 'TrailFork']].max(axis=1)
    merged['WorstStatic'] = merged[['Fork', 'TrailFork']].min(axis=1)
    
    # Classify dynamic performance
    merged['DynamicVsBest'] = merged['Dynamic'] - merged['BestStatic']
    merged['DynamicVsWorst'] = merged['Dynamic'] - merged['WorstStatic']
    
    # Categorize
    def categorize(row):
        if row['Dynamic'] > row['BestStatic']:
            return 'BETTER_THAN_BOTH'
        elif row['Dynamic'] < row['WorstStatic']:
            return 'WORSE_THAN_BOTH'
        else:
            return 'MODERATE'
    
    merged['Category'] = merged.apply(categorize, axis=1)
    
    return merged

def print_summary(comparison_df):
    """Print summary statistics."""
    print("\n" + "="*80)
    print("DYNAMIC STRATEGY SWITCHING ANALYSIS")
    print("="*80)
    
    # Overall statistics
    total_configs = len(comparison_df)
    better_count = len(comparison_df[comparison_df['Category'] == 'BETTER_THAN_BOTH'])
    moderate_count = len(comparison_df[comparison_df['Category'] == 'MODERATE'])
    worse_count = len(comparison_df[comparison_df['Category'] == 'WORSE_THAN_BOTH'])
    
    print(f"\nTotal configurations tested: {total_configs}")
    print(f"  Dynamic BETTER than both static: {better_count} ({100*better_count/total_configs:.1f}%)")
    print(f"  Dynamic MODERATE (between): {moderate_count} ({100*moderate_count/total_configs:.1f}%)")
    print(f"  Dynamic WORSE than both static: {worse_count} ({100*worse_count/total_configs:.1f}%)")
    
    # Average improvements
    avg_vs_best = comparison_df['DynamicVsBest'].mean()
    avg_vs_worst = comparison_df['DynamicVsWorst'].mean()
    
    print(f"\nAverage profit difference:")
    print(f"  Dynamic vs Best Static: {avg_vs_best:+.6f} ({100*avg_vs_best:.3f}%)")
    print(f"  Dynamic vs Worst Static: {avg_vs_worst:+.6f} ({100*avg_vs_worst:.3f}%)")
    
    # Best cases for dynamic
    print("\n" + "-"*80)
    print("TOP 5 CASES WHERE DYNAMIC SWITCHING WORKS BEST:")
    print("-"*80)
    top_dynamic = comparison_df.nlargest(5, 'DynamicVsBest')
    for idx, row in top_dynamic.iterrows():
        print(f"Gamma={row['Gamma']:.2f}, HashRate={row['HashRate']:.2f}: "
              f"Dynamic={row['Dynamic']:.4f}, Fork={row['Fork']:.4f}, TrailFork={row['TrailFork']:.4f} "
              f"(+{100*row['DynamicVsBest']:.2f}% vs best static)")
    
    # Worst cases for dynamic
    print("\n" + "-"*80)
    print("TOP 5 CASES WHERE DYNAMIC SWITCHING WORKS WORST:")
    print("-"*80)
    worst_dynamic = comparison_df.nsmallest(5, 'DynamicVsBest')
    for idx, row in worst_dynamic.iterrows():
        print(f"Gamma={row['Gamma']:.2f}, HashRate={row['HashRate']:.2f}: "
              f"Dynamic={row['Dynamic']:.4f}, Fork={row['Fork']:.4f}, TrailFork={row['TrailFork']:.4f} "
              f"({100*row['DynamicVsBest']:.2f}% vs best static)")
    
    # Breakdown by gamma
    print("\n" + "-"*80)
    print("BREAKDOWN BY GAMMA (CONNECTIVITY):")
    print("-"*80)
    for gamma in sorted(comparison_df['Gamma'].unique()):
        gamma_data = comparison_df[comparison_df['Gamma'] == gamma]
        better = len(gamma_data[gamma_data['Category'] == 'BETTER_THAN_BOTH'])
        total = len(gamma_data)
        avg_improvement = gamma_data['DynamicVsBest'].mean()
        print(f"Gamma {gamma:.2f}: {better}/{total} better ({100*better/total:.1f}%), "
              f"avg improvement: {100*avg_improvement:+.3f}%")
    
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
            better = len(hr_data[hr_data['Category'] == 'BETTER_THAN_BOTH'])
            total = len(hr_data)
            avg_improvement = hr_data['DynamicVsBest'].mean()
            print(f"Hash Rate {hr_range}: {better}/{total} better ({100*better/total:.1f}%), "
                  f"avg improvement: {100*avg_improvement:+.3f}%")

def save_detailed_comparison(comparison_df, output_file='comparison_detailed.csv'):
    """Save detailed comparison to CSV."""
    comparison_df.to_csv(output_file, index=False)
    print(f"\n\nDetailed comparison saved to: {output_file}")

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Usage: python compare_results.py <fork_results> <trail_fork_results> <dynamic_results>")
        print("\nExample:")
        print("  python compare_results.py results_b5_pure_fork.txt results_b5_pure_stubborn_trail_fork.txt results_b5_dynamic_switching.txt")
        sys.exit(1)
    
    fork_file = sys.argv[1]
    trail_fork_file = sys.argv[2]
    dynamic_file = sys.argv[3]
    
    print("Loading results...")
    print(f"  Fork: {fork_file}")
    print(f"  Trail-Fork: {trail_fork_file}")
    print(f"  Dynamic: {dynamic_file}")
    
    try:
        fork_df = load_results(fork_file)
        trail_fork_df = load_results(trail_fork_file)
        dynamic_df = load_results(dynamic_file)
        
        print(f"\nLoaded {len(fork_df)} fork results")
        print(f"Loaded {len(trail_fork_df)} trail-fork results")
        print(f"Loaded {len(dynamic_df)} dynamic results")
        
        comparison_df = compare_strategies(fork_df, trail_fork_df, dynamic_df)
        print_summary(comparison_df)
        save_detailed_comparison(comparison_df)
        
    except FileNotFoundError as e:
        print(f"\nError: Could not find file - {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
