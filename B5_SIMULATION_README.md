# B5 Focused Simulation

## Overview

ScheduledStratSimB5 is a focused version of the dynamic strategy scheduler that tests specific parameter ranges requested for detailed analysis:

- **Gamma (connectivity rate)**: 0.8 to 1.0 in 0.05 increments (5 values)
- **Hash rate**: 0.15 to 0.3 (15% to 30%) in 0.01 increments (16 values)
- **Total games**: 5 × 16 × 25 = 2,000 games (vs 12,625 in full simulation)

## Purpose

This focused simulation allows faster iteration when testing different strategy switching patterns in the most interesting parameter region where:
- High connectivity (0.8-1.0) means the network is well-connected
- Medium hash rates (15-30%) are where strategic mining becomes profitable

## Compilation

```bash
make scheduled-strat-b5
```

## Usage

### Basic Run
```bash
./scheduled-strat-b5 <schedule_file> [output_file]
```

### Example Runs

**Test stubborn-fork ↔ stubborn-trail-fork switching (5000 block intervals):**
```bash
./scheduled-strat-b5 schedule_fork_stubborn_trail_fork_5000.txt results_b5_fork_switching.txt
```

**Test honest ↔ selfish switching (5000 block intervals):**
```bash
./scheduled-strat-b5 schedule_honest_selfish_5000.txt results_b5_honest_selfish.txt
```

## Schedule Files

### schedule_fork_stubborn_trail_fork_5000.txt
Alternates between stubborn-fork and stubborn-trail-fork every 5000 blocks:
- Blocks 0-4999: stubborn-fork
- Blocks 5000-9999: stubborn-trail-fork
- Blocks 10000-14999: stubborn-fork
- Blocks 15000-19999: stubborn-trail-fork

### schedule_honest_selfish_5000.txt
Alternates between honest and selfish mining every 5000 blocks:
- Blocks 0-4999: default (honest)
- Blocks 5000-9999: selfish
- Blocks 10000-14999: default (honest)
- Blocks 15000-19999: selfish

## Creating Custom Schedules

To test different switching frequencies, create new schedule files:

**Every 2500 blocks (8 switches):**
```
0, 0, 2499, stubborn-fork
0, 2500, 4999, stubborn-trail-fork
0, 5000, 7499, stubborn-fork
0, 7500, 9999, stubborn-trail-fork
0, 10000, 12499, stubborn-fork
0, 12500, 14999, stubborn-trail-fork
0, 15000, 17499, stubborn-fork
0, 17500, 19999, stubborn-trail-fork
1, 0, 19999, default-selfish
```

**Every 10000 blocks (2 switches):**
```
0, 0, 9999, stubborn-fork
0, 10000, 19999, stubborn-trail-fork
1, 0, 19999, default-selfish
```

## Output Format

Same as ScheduledStratSim:
```
Gamma, Miner0_ProfitFraction, Miner0_HashRate, Miner1_HashRate, Miner0_BlockFraction
0.8, 0.152341, 0.15, 0.85, 0.149823
0.8, 0.156789, 0.16, 0.84, 0.158234
...
```

## Performance

- **Full ScheduledStratSim**: ~12,625 games (several hours)
- **B5 Focused**: ~2,000 games (much faster, ~30-60 minutes)

Use B5 for rapid testing of different switching patterns, then use full simulation for comprehensive analysis.

## Research Questions

This focused simulation helps answer:

1. **Optimal switching frequency**: Does switching every 2500, 5000, or 10000 blocks work best?
2. **Strategy pair effectiveness**: Which strategy pairs benefit most from switching?
3. **Connectivity sensitivity**: How does high connectivity (0.8-1.0) affect dynamic strategies?
4. **Hash rate sweet spot**: At what hash rate (15-30%) is dynamic switching most profitable?

## Comparison Workflow

1. Run B5 with different switching frequencies
2. Compare profit fractions across schedules
3. Identify optimal switching pattern
4. Run full ScheduledStratSim with optimal pattern for comprehensive validation

## Files

- `ScheduledStratSimB5/main.cpp` - B5 simulation source
- `schedule_fork_stubborn_trail_fork_5000.txt` - Fork strategy switching
- `schedule_honest_selfish_5000.txt` - Honest/selfish switching
- `Makefile` - Build configuration (includes `scheduled-strat-b5` target)
