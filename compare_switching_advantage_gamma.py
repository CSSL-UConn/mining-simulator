#!/usr/bin/env python3
"""
Compare if dynamic switching provides more advantage with varying gamma vs fixed gamma.

Tests hypothesis: Does strategy switching work better when network conditions change?

Usage:
    python compare_switching_advantage_gamma.py <pure_fixed> <switching_fixed> <pure_varying> <switching_varying>
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
            if line.startswith('Gamma') or line.startswith('InitialGamma'):
                continue  # Skip header
            parts = line.strip().split(',')
            if len(parts) >= 5:
                data.append([float(x.strip()) for x in parts])
    
    df = pd.DataFrame(data, columns=['Gamma', 'ProfitFraction', 'HashRate', 'HonestHashRate', 'BlockFraction'])
    return df

def analyze_switching_advantage(pure_fixed_df, switching_fixed_df, pure_varying_df, switching_varying_df):
    """Analyze if switching provides more advantage with varying gamma."""
    
    # Group by HashRate
    pure_fixed = pure_fixed_df.groupby('HashRate')['ProfitFraction'].mean().reset_index()
    switching_fixed = switching_fixed_df.groupby('HashRate')['ProfitFraction'].mean().reset_index()
    pure_varying = pure_varying_df.groupby('HashRate')['ProfitFraction'].mean().reset_index()
    switching_varying = switching_varying_df.groupby('HashRate')['ProfitFraction'].mean().reset_index()
    
    # Merge all
    merged = pure_fixed.rename(columns={'ProfitFraction': 'PureFixed'})
    merged = merged.merge(switching_fixed.rename(columns={'ProfitFraction': 'SwitchingFixed'}), on='HashRate')
    merged = merged.merge(pure_varying.rename(columns={'ProfitFraction': 'PureVarying'}), on='HashRate')
    merged = merged.merge(switching_varying.rename(columns={'ProfitFraction': 'SwitchingVarying'}), on='HashRate')
    
    # Calculate advantages
    merged['AdvantageFixed'] = merged['SwitchingFixed'] - merged['PureFixed']
    merged['AdvantageVarying'] = merged['SwitchingVarying'] - merged['PureVarying']
    
    # Calculate if varying gamma increases switching advantage
    merged['AdvantageDiff'] = merged['AdvantageVarying'] - merged['AdvantageFixed']
    merged['PercentDiff'] = 100 * merged['AdvantageDiff'] / abs(merged['AdvantageFixed'] + 0.0001)
    
    return merged

def print_analysis(analysis_df):
    """Print analysis results."""
    print("\n" + "="*80)
    print("SWITCHING ADVANTAGE: VARYING GAMMA vs FIXED GAMMA")
    print("="*80)
    print("\nHypothesis: Does dynamic switching work better when network conditions vary?")
    
    # Overall statistics
    total = len(analysis_df)
    varying_better = len(analysis_df[analysis_df['AdvantageDiff'] > 0])
    fixed_better = len(analysis_df[analysis_df['AdvantageDiff'] < 0])
    
    print(f"\nTotal hash rates tested: {total}")
    print(f"  Varying gamma increases switching advantage: {varying_better} ({100*varying_better/total:.1f}%)")
    print(f"  Fixed gamma has better switching advantage: {fixed_better} ({100*fixed_better/total:.1f}%)")
    
    avg_advantage_fixed = analysis_df['AdvantageFixed'].mean()
    avg_advantage_varying = analysis_df['AdvantageVarying'].mean()
    avg_diff = analysis_df['AdvantageDiff'].mean()
    
    print(f"\nAverage switching advantage:")
    print(f"  With FIXED gamma: {avg_advantage_fixed:+.6f} ({100*avg_advantage_fixed:.3f}%)")
    print(f"  With VARYING gamma: {avg_advantage_varying:+.6f} ({100*avg_advantage_varying:.3f}%)")
    print(f"  Difference: {avg_diff:+.6f}")
    
    if avg_diff > 0:
        print(f"\n>>> VARYING GAMMA INCREASES SWITCHING ADVANTAGE! <<<")
        print("Dynamic strategies benefit MORE from changing network conditions!")
    elif avg_diff < 0:
        print(f"\n>>> FIXED GAMMA HAS BETTER SWITCHING ADVANTAGE <<<")
        print("Stable network conditions favor dynamic strategies more")
    else:
        print(f"\n>>> No significant difference <<<")
    
    # Best cases
    print("\n" + "-"*80)
    print("TOP 5 HASH RATES WHERE VARYING GAMMA HELPS SWITCHING MOST:")
    print("-"*80)
    top = analysis_df.nlargest(5, 'AdvantageDiff')
    for idx, row in top.iterrows():
        print(f"HashRate={row['HashRate']:.2f}: "
              f"Fixed advantage={row['AdvantageFixed']:.6f}, "
              f"Varying advantage={row['AdvantageVarying']:.6f} "
              f"(+{row['AdvantageDiff']:.6f})")
    
    print("\n" + "-"*80)
    print("TOP 5 HASH RATES WHERE FIXED GAMMA HELPS SWITCHING MORE:")
    print("-"*80)
    bottom = analysis_df.nsmallest(5, 'AdvantageDiff')
    for idx, row in bottom.iterrows():
        print(f"HashRate={row['HashRate']:.2f}: "
              f"Fixed advantage={row['AdvantageFixed']:.6f}, "
              f"Varying advantage={row['AdvantageVarying']:.6f} "
              f"({row['AdvantageDiff']:.6f})")
    
    # Breakdown by hash rate
    print("\n" + "-"*80)
    print("BREAKDOWN BY HASH RATE RANGE:")
    print("-"*80)
    analysis_df['HashRateRange'] = pd.cut(analysis_df['HashRate'], 
                                           bins=[0.14, 0.20, 0.25, 0.31],
                                           labels=['15-20%', '20-25%', '25-30%'])
    for hr_range in ['15-20%', '20-25%', '25-30%']:
        hr_data = analysis_df[analysis_df['HashRateRange'] == hr_range]
        if len(hr_data) > 0:
            varying_wins = len(hr_data[hr_data['AdvantageDiff'] > 0])
            total = len(hr_data)
            avg_diff = hr_data['AdvantageDiff'].mean()
            winner = "Varying" if avg_diff > 0 else "Fixed"
            print(f"Hash Rate {hr_range}: Varying helps {varying_wins}/{total} ({100*varying_wins/total:.1f}%), "
                  f"avg diff: {avg_diff:+.6f} (favors {winner})")
    
    # Summary
    print("\n" + "-"*80)
    print("SUMMARY:")
    print("-"*80)
    print(f"Pure strategy with fixed gamma: {analysis_df['PureFixed'].mean():.6f}")
    print(f"Pure strategy with varying gamma: {analysis_df['PureVarying'].mean():.6f}")
    print(f"Switching with fixed gamma: {analysis_df['SwitchingFixed'].mean():.6f}")
    print(f"Switching with varying gamma: {analysis_df['SwitchingVarying'].mean():.6f}")
    print(f"\nSwitching advantage with fixed gamma: {avg_advantage_fixed:+.6f}")
    print(f"Switching advantage with varying gamma: {avg_advantage_varying:+.6f}")
    print(f"Improvement from varying gamma: {avg_diff:+.6f}")

def save_analysis(analysis_df, output_file='switching_advantage_gamma.csv'):
    """Save analysis to CSV."""
    analysis_df.to_csv(output_file, index=False)
    print(f"\n\nDetailed analysis saved to: {output_file}")

if __name__ == '__main__':
    if len(sys.argv) != 5:
        print("Usage: python compare_switching_advantage_gamma.py <pure_fixed> <switching_fixed> <pure_varying> <switching_varying>")
        print("\nExample:")
        print("  python compare_switching_advantage_gamma.py \\")
        print("    results/results_b5_pure_selfish.txt \\")
        print("    results/results_b5_asymmetric_3000_1000.txt \\")
        print("    results/results_b5_gamma_selfish_volatile.txt \\")
        print("    results/results_b5_gamma_switching_volatile.txt")
        sys.exit(1)
    
    pure_fixed_file = sys.argv[1]
    switching_fixed_file = sys.argv[2]
    pure_varying_file = sys.argv[3]
    switching_varying_file = sys.argv[4]
    
    print("Loading results...")
    print(f"  Pure (fixed gamma): {pure_fixed_file}")
    print(f"  Switching (fixed gamma): {switching_fixed_file}")
    print(f"  Pure (varying gamma): {pure_varying_file}")
    print(f"  Switching (varying gamma): {switching_varying_file}")
    
    try:
        pure_fixed_df = load_results(pure_fixed_file)
        switching_fixed_df = load_results(switching_fixed_file)
        pure_varying_df = load_results(pure_varying_file)
        switching_varying_df = load_results(switching_varying_file)
        
        analysis_df = analyze_switching_advantage(pure_fixed_df, switching_fixed_df, 
                                                   pure_varying_df, switching_varying_df)
        print_analysis(analysis_df)
        save_analysis(analysis_df)
        
    except FileNotFoundError as e:
        print(f"\nError: Could not find file - {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
