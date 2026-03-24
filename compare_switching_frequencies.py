#!/usr/bin/env python3
"""
Compare two different switching frequencies to see which performs better.

Usage:
    python compare_switching_frequencies.py results_b5_honest_selfish_5000.txt results_b5_honest_selfish_2500.txt
"""

import sys
import pandas as pd

def load_results(filename):
    """Load results file, skipping comment lines."""
    data = []
    with open(filename, 'r') as f:
        for line in f:
            if line.startswith('#') or line.strip() == '':
                continue
            # Skip header lines (check if line contains column names)
            if 'Gamma' in line or 'ProfitFraction' in line or 'HashRate' in line:
                continue
            parts = line.strip().split(',')
            if len(parts) >= 5:
                data.append([float(x.strip()) for x in parts])
    
    df = pd.DataFrame(data, columns=['Gamma', 'ProfitFraction', 'HashRate', 'HonestHashRate', 'BlockFraction'])
    return df

def compare_frequencies(freq1_df, freq2_df, freq1_name, freq2_name):
    """Compare two switching frequencies."""
    
    # Group by (Gamma, HashRate) and calculate mean profit fraction
    freq1_grouped = freq1_df.groupby(['Gamma', 'HashRate'])['ProfitFraction'].mean().reset_index()
    freq2_grouped = freq2_df.groupby(['Gamma', 'HashRate'])['ProfitFraction'].mean().reset_index()
    
    # Merge
    merged = freq1_grouped.rename(columns={'ProfitFraction': 'Freq1'})
    merged = merged.merge(freq2_grouped.rename(columns={'ProfitFraction': 'Freq2'}), 
                          on=['Gamma', 'HashRate'])
    
    # Calculate difference
    merged['Difference'] = merged['Freq2'] - merged['Freq1']
    merged['PercentDiff'] = 100 * merged['Difference'] / merged['Freq1']
    
    return merged

def print_comparison(comparison_df, freq1_name, freq2_name):
    """Print comparison summary."""
    print("\n" + "="*80)
    print(f"SWITCHING FREQUENCY COMPARISON: {freq1_name} vs {freq2_name}")
    print("="*80)
    
    total_configs = len(comparison_df)
    freq2_better = len(comparison_df[comparison_df['Difference'] > 0])
    freq1_better = len(comparison_df[comparison_df['Difference'] < 0])
    tied = len(comparison_df[comparison_df['Difference'] == 0])
    
    print(f"\nTotal configurations tested: {total_configs}")
    print(f"  {freq2_name} BETTER: {freq2_better} ({100*freq2_better/total_configs:.1f}%)")
    print(f"  {freq1_name} BETTER: {freq1_better} ({100*freq1_better/total_configs:.1f}%)")
    print(f"  TIED: {tied} ({100*tied/total_configs:.1f}%)")
    
    avg_diff = comparison_df['Difference'].mean()
    avg_pct_diff = comparison_df['PercentDiff'].mean()
    
    print(f"\nAverage profit difference:")
    print(f"  {freq2_name} vs {freq1_name}: {avg_diff:+.6f} ({avg_pct_diff:+.3f}%)")
    
    if avg_diff > 0:
        print(f"\n>>> {freq2_name} is BETTER on average <<<")
    elif avg_diff < 0:
        print(f"\n>>> {freq1_name} is BETTER on average <<<")
    else:
        print(f"\n>>> Both frequencies perform EQUALLY on average <<<")
    
    # Best cases for freq2
    print("\n" + "-"*80)
    print(f"TOP 5 CASES WHERE {freq2_name} OUTPERFORMS {freq1_name}:")
    print("-"*80)
    top_freq2 = comparison_df.nlargest(5, 'Difference')
    for idx, row in top_freq2.iterrows():
        print(f"Gamma={row['Gamma']:.2f}, HashRate={row['HashRate']:.2f}: "
              f"{freq2_name}={row['Freq2']:.4f}, {freq1_name}={row['Freq1']:.4f} "
              f"(+{row['PercentDiff']:.2f}%)")
    
    # Best cases for freq1
    print("\n" + "-"*80)
    print(f"TOP 5 CASES WHERE {freq1_name} OUTPERFORMS {freq2_name}:")
    print("-"*80)
    worst_freq2 = comparison_df.nsmallest(5, 'Difference')
    for idx, row in worst_freq2.iterrows():
        print(f"Gamma={row['Gamma']:.2f}, HashRate={row['HashRate']:.2f}: "
              f"{freq1_name}={row['Freq1']:.4f}, {freq2_name}={row['Freq2']:.4f} "
              f"({row['PercentDiff']:.2f}%)")
    
    # Breakdown by gamma
    print("\n" + "-"*80)
    print("BREAKDOWN BY GAMMA (CONNECTIVITY):")
    print("-"*80)
    for gamma in sorted(comparison_df['Gamma'].unique()):
        gamma_data = comparison_df[comparison_df['Gamma'] == gamma]
        freq2_wins = len(gamma_data[gamma_data['Difference'] > 0])
        total = len(gamma_data)
        avg_diff = gamma_data['PercentDiff'].mean()
        winner = freq2_name if avg_diff > 0 else freq1_name
        print(f"Gamma {gamma:.2f}: {freq2_name} wins {freq2_wins}/{total} ({100*freq2_wins/total:.1f}%), "
              f"avg diff: {avg_diff:+.3f}% (favors {winner})")
    
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
            freq2_wins = len(hr_data[hr_data['Difference'] > 0])
            total = len(hr_data)
            avg_diff = hr_data['PercentDiff'].mean()
            winner = freq2_name if avg_diff > 0 else freq1_name
            print(f"Hash Rate {hr_range}: {freq2_name} wins {freq2_wins}/{total} ({100*freq2_wins/total:.1f}%), "
                  f"avg diff: {avg_diff:+.3f}% (favors {winner})")

def save_detailed_comparison(comparison_df, freq1_name, freq2_name, output_file='frequency_comparison.csv'):
    """Save detailed comparison to CSV."""
    comparison_df.to_csv(output_file, index=False)
    print(f"\n\nDetailed comparison saved to: {output_file}")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python compare_switching_frequencies.py <freq1_results> <freq2_results>")
        print("\nExample:")
        print("  python compare_switching_frequencies.py results_b5_honest_selfish_5000.txt results_b5_honest_selfish_2500.txt")
        sys.exit(1)
    
    freq1_file = sys.argv[1]
    freq2_file = sys.argv[2]
    
    # Extract frequency names from filenames
    freq1_name = "5000-block" if "5000" in freq1_file else freq1_file
    freq2_name = "2500-block" if "2500" in freq2_file else freq2_file
    
    print(f"Loading results...")
    print(f"  Frequency 1: {freq1_file}")
    print(f"  Frequency 2: {freq2_file}")
    
    try:
        freq1_df = load_results(freq1_file)
        freq2_df = load_results(freq2_file)
        
        print(f"\nLoaded {len(freq1_df)} results for {freq1_name}")
        print(f"Loaded {len(freq2_df)} results for {freq2_name}")
        
        comparison_df = compare_frequencies(freq1_df, freq2_df, freq1_name, freq2_name)
        print_comparison(comparison_df, freq1_name, freq2_name)
        save_detailed_comparison(comparison_df, freq1_name, freq2_name)
        
    except FileNotFoundError as e:
        print(f"\nError: Could not find file - {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
