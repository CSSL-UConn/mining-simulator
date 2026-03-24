#!/usr/bin/env python3
"""
Compare varying gamma results against fixed gamma baseline.

Usage:
    python compare_gamma_varying.py <fixed_gamma_results> <varying_gamma_results>
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
            if line.startswith('Gamma') or line.startswith('InitialGamma'):
                continue  # Skip header
            parts = line.strip().split(',')
            if len(parts) >= 5:
                data.append([float(x.strip()) for x in parts])
    
    df = pd.DataFrame(data, columns=['Gamma', 'ProfitFraction', 'HashRate', 'HonestHashRate', 'BlockFraction'])
    return df

def compare_gamma_strategies(fixed_df, varying_df):
    """Compare fixed gamma vs varying gamma."""
    
    # Group by HashRate and calculate mean profit fraction
    fixed_grouped = fixed_df.groupby('HashRate')['ProfitFraction'].mean().reset_index()
    varying_grouped = varying_df.groupby('HashRate')['ProfitFraction'].mean().reset_index()
    
    # Merge
    merged = fixed_grouped.rename(columns={'ProfitFraction': 'FixedGamma'})
    merged = merged.merge(varying_grouped.rename(columns={'ProfitFraction': 'VaryingGamma'}), 
                          on='HashRate')
    
    # Calculate difference
    merged['Difference'] = merged['VaryingGamma'] - merged['FixedGamma']
    merged['PercentDiff'] = 100 * merged['Difference'] / merged['FixedGamma']
    
    return merged

def print_comparison(comparison_df):
    """Print comparison summary."""
    print("\n" + "="*80)
    print("VARYING GAMMA vs FIXED GAMMA COMPARISON")
    print("="*80)
    
    total_configs = len(comparison_df)
    varying_better = len(comparison_df[comparison_df['Difference'] > 0])
    fixed_better = len(comparison_df[comparison_df['Difference'] < 0])
    tied = len(comparison_df[comparison_df['Difference'] == 0])
    
    print(f"\nTotal hash rates tested: {total_configs}")
    print(f"  Varying Gamma BETTER: {varying_better} ({100*varying_better/total_configs:.1f}%)")
    print(f"  Fixed Gamma BETTER: {fixed_better} ({100*fixed_better/total_configs:.1f}%)")
    print(f"  TIED: {tied} ({100*tied/total_configs:.1f}%)")
    
    avg_diff = comparison_df['Difference'].mean()
    avg_pct_diff = comparison_df['PercentDiff'].mean()
    
    print(f"\nAverage profit difference:")
    print(f"  Varying vs Fixed: {avg_diff:+.6f} ({avg_pct_diff:+.3f}%)")
    
    if avg_diff > 0:
        print(f"\n>>> VARYING GAMMA is BETTER on average <<<")
        print("Network adaptation provides an advantage!")
    elif avg_diff < 0:
        print(f"\n>>> FIXED GAMMA is BETTER on average <<<")
        print("Stable network conditions are more profitable")
    else:
        print(f"\n>>> Both perform EQUALLY on average <<<")
    
    # Best cases for varying gamma
    print("\n" + "-"*80)
    print("TOP 5 HASH RATES WHERE VARYING GAMMA WORKS BEST:")
    print("-"*80)
    top_varying = comparison_df.nlargest(5, 'Difference')
    for idx, row in top_varying.iterrows():
        print(f"HashRate={row['HashRate']:.2f}: "
              f"Varying={row['VaryingGamma']:.4f}, Fixed={row['FixedGamma']:.4f} "
              f"(+{row['PercentDiff']:.2f}%)")
    
    # Best cases for fixed gamma
    print("\n" + "-"*80)
    print("TOP 5 HASH RATES WHERE FIXED GAMMA WORKS BEST:")
    print("-"*80)
    worst_varying = comparison_df.nsmallest(5, 'Difference')
    for idx, row in worst_varying.iterrows():
        print(f"HashRate={row['HashRate']:.2f}: "
              f"Fixed={row['FixedGamma']:.4f}, Varying={row['VaryingGamma']:.4f} "
              f"({row['PercentDiff']:.2f}%)")
    
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
            varying_wins = len(hr_data[hr_data['Difference'] > 0])
            total = len(hr_data)
            avg_diff = hr_data['PercentDiff'].mean()
            winner = "Varying" if avg_diff > 0 else "Fixed"
            print(f"Hash Rate {hr_range}: Varying wins {varying_wins}/{total} ({100*varying_wins/total:.1f}%), "
                  f"avg diff: {avg_diff:+.3f}% (favors {winner})")
    
    # Summary statistics
    print("\n" + "-"*80)
    print("SUMMARY STATISTICS:")
    print("-"*80)
    print(f"Fixed Gamma - Mean profit: {comparison_df['FixedGamma'].mean():.6f}")
    print(f"Varying Gamma - Mean profit: {comparison_df['VaryingGamma'].mean():.6f}")
    print(f"Difference: {comparison_df['Difference'].mean():+.6f}")
    print(f"Std deviation of difference: {comparison_df['Difference'].std():.6f}")

def save_detailed_comparison(comparison_df, output_file='gamma_comparison.csv'):
    """Save detailed comparison to CSV."""
    comparison_df.to_csv(output_file, index=False)
    print(f"\n\nDetailed comparison saved to: {output_file}")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python compare_gamma_varying.py <fixed_gamma_results> <varying_gamma_results>")
        print("\nExample:")
        print("  python compare_gamma_varying.py results/results_b5_asymmetric_3000_1000.txt results/results_b5_gamma_varying.txt")
        sys.exit(1)
    
    fixed_file = sys.argv[1]
    varying_file = sys.argv[2]
    
    print("Loading results...")
    print(f"  Fixed Gamma: {fixed_file}")
    print(f"  Varying Gamma: {varying_file}")
    
    try:
        fixed_df = load_results(fixed_file)
        varying_df = load_results(varying_file)
        
        print(f"\nLoaded {len(fixed_df)} results for fixed gamma")
        print(f"Loaded {len(varying_df)} results for varying gamma")
        
        comparison_df = compare_gamma_strategies(fixed_df, varying_df)
        print_comparison(comparison_df)
        save_detailed_comparison(comparison_df)
        
    except FileNotFoundError as e:
        print(f"\nError: Could not find file - {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
