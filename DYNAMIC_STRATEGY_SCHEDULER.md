# Dynamic Strategy Scheduler for BlockSim

## Overview

The Dynamic Strategy Scheduler extends BlockSim to allow miners to switch strategies mid-game while maintaining continuity of their blockchain state and private chains. This enables testing whether dynamic strategy switching is more profitable than static strategies.

## Key Features

- **Mid-game strategy switching**: Miners can change strategies at specified block heights
- **State preservation**: Private chains and miner state persist across strategy changes
- **Continuous blockchain**: Each game runs one continuous blockchain (not multiple separate games)
- **Statistical testing**: Structured like SelfishSim with multiple gamma values, hash rates, and game repetitions
- **Gamma parameter support**: Honest miners use gamma-aware strategies for realistic selfish mining scenarios

## Architecture

### Components

1. **StrategyScheduler** (`BlockSim/strategy_scheduler.hpp/cpp`)
   - Loads schedule files defining when miners should switch strategies
   - Queries active strategy for any miner at any block height
   - Validates schedules for overlaps and conflicts

2. **StrategyFactory** (`BlockSim/strategy_factory.hpp/cpp`)
   - Creates strategy instances by name
   - Supports gamma parameter for tie-breaking behavior
   - Handles all existing BlockSim strategies

3. **ScheduledStratSim** (`ScheduledStratSim/main.cpp`)
   - Main simulation executable
   - Triple-nested loop structure: gamma values → hash rates → games
   - Manages strategy switching during game execution
   - Outputs results in SelfishSim-compatible format

## How It Works

### Schedule File Format

Schedule files define strategy changes with the format:
```
miner_id, start_block, end_block, strategy_name
```

Example (`example_schedule.txt`):
```
# Miner 0: Dynamic strategy switching
0, 0, 6666, selfish
0, 6667, 13333, stubborn-trail
0, 13334, 19999, petty

# Miner 1: Gamma-aware honest miner
1, 0, 19999, default-selfish
```

### Execution Flow

1. **Initialization**
   - Load strategy schedule from file
   - Create strategy pool with all possible strategies (gamma-parameterized)
   - Set up miners with initial strategies from schedule

2. **Game Loop** (for each gamma value, hash rate, and game repetition)
   - Create ONE continuous blockchain per game
   - Initialize miners with starting strategies
   - Run simulation for 20,000 blocks

3. **Strategy Switching** (during game execution)
   - At each round, check current block height
   - Query scheduler for active strategies
   - If strategy changed:
     - Call `miner.changeStrategy()` to switch strategy
     - Call `minerGroup.resetOrder()` to rebuild mining queue heap
   - Continue mining with new strategy

4. **State Preservation**
   - Miners keep their private chains across strategy changes
   - Mining costs and timing are properly updated
   - Blockchain remains continuous throughout the game

### Critical Implementation Details

**Heap Management**: After changing a miner's strategy, their next mining time changes. The mining queue is a heap ordered by next mining time, so we must call `minerGroup.resetOrder()` to rebuild the heap. Without this, the simulation will fail with time assertion errors.

**Gamma Parameter**: The honest miner MUST use `default-selfish` strategy (not `default`) for gamma to work correctly. The `default-selfish` strategy respects the gamma parameter for tie-breaking during forks.

**Continuous Blockchain**: Unlike some other simulations, each game runs ONE continuous blockchain. Miners switch strategies mid-game while maintaining their state, simulating adaptive behavior.

## Usage

### Compilation

```bash
make scheduled-strat
```

### Running

```bash
./scheduled-strat <schedule_file> [output_file]
```

Example:
```bash
./scheduled-strat example_schedule.txt dynamic_results.txt
```

### Output Format

The output file contains:
- Header comments describing the schedule
- CSV data: `Gamma, Miner0_ProfitFraction, Miner0_HashRate, Miner1_HashRate, Miner0_BlockFraction`
- One row per game (25 games × 101 hash rates × 5 gamma values = 12,625 rows)

Example output:
```
# Dynamic Strategy Switching Simulation Results
# Schedule file: example_schedule.txt
# Miner 0: selfish (0-6666), stubborn-trail (6667-13333), petty (13334-19999)
# Miner 1: default-selfish (0-19999)
Gamma, Miner0_ProfitFraction, Miner0_HashRate, Miner1_HashRate, Miner0_BlockFraction
0, 0.00148723, 0.005, 0.995, 0.00149328
0, 0.000835388, 0.005, 0.995, 0.00113726
...
```

## Supported Strategies

All BlockSim strategies are supported:
- `default` / `honest` - Standard honest mining
- `default-selfish` - Gamma-aware honest mining (required for honest miner)
- `selfish` - Classic selfish mining
- `clever-selfish` - Selfish mining with undercut
- `stubborn-trail` / `stubborn-trail-N` - Stubborn mining variants
- `stubborn-fork` - Fork-based stubborn mining
- `stubborn-lead` - Lead-based stubborn mining
- `stubborn-lead-fork` - Combined lead and fork
- `stubborn-lead-trail` - Combined lead and trail
- `stubborn-trail-fork` - Combined trail and fork
- `stubborn-lead-trail-fork` - All three combined
- `petty` - Petty compliant mining
- `lazy-fork` - Lazy fork mining
- `gap` - Gap mining
- `rational` - Rational mining
- `publish-3` / `publish-4` - Publish-N strategies

## Creating Custom Schedules

To test different strategy combinations:

1. Create a new schedule file (e.g., `my_schedule.txt`)
2. Define strategy changes for each miner
3. Ensure Miner 1 uses `default-selfish` for gamma support
4. Run: `./scheduled-strat my_schedule.txt my_results.txt`

Example custom schedule:
```
# Test if switching from selfish to stubborn-lead is profitable
0, 0, 9999, selfish
0, 10000, 19999, stubborn-lead
1, 0, 19999, default-selfish
```

## Comparison with SelfishSim

**Similarities:**
- Triple-nested loop structure (gamma, hash rates, games)
- Tests same gamma values: 0.0, 0.25, 0.5, 0.75, 1.0
- Tests hash rates from 0.5% to 51% in 0.5% increments
- Runs 25 games per configuration
- Outputs profit fractions in same format

**Differences:**
- ScheduledStratSim: Miners switch strategies mid-game
- SelfishSim: Miners use static strategies throughout
- ScheduledStratSim: Uses schedule files for configuration
- SelfishSim: Hardcoded strategy comparison

## Research Questions

This implementation enables testing:

1. **Is dynamic switching profitable?** Compare dynamic results to static SelfishSim results
2. **Which strategy sequences work best?** Test different schedule combinations
3. **Does timing matter?** Try switching at different block heights
4. **Gamma sensitivity?** How does gamma affect dynamic strategy profitability?

## Technical Notes

### File Sync Issues (WSL/Windows)

If running on WSL with Windows filesystem, you may encounter file sync issues where Kiro's file writes don't immediately appear in WSL. Solutions:
- Run `sync` command in WSL to force filesystem sync
- Close and reopen files in VS Code
- Write files directly from WSL terminal using `cat` or text editors

### Debugging

Enable detailed logging by checking `BlockSim/logging.h`:
- `_GAMEINFO` - Game-level events (strategy switches, game results)
- `_COMMENTARY` - Round-by-round details
- `_BLOCKINFO` - Block-level information

### Performance

A full run (5 gamma values × 101 hash rates × 25 games) takes significant time:
- ~12,625 games total
- Each game simulates 20,000 blocks
- Expect several hours for complete run
- Consider testing with fewer games or hash rates for quick validation

## Files Modified/Created

**New Files:**
- `BlockSim/strategy_scheduler.hpp` - Scheduler interface
- `BlockSim/strategy_scheduler.cpp` - Scheduler implementation
- `BlockSim/strategy_factory.hpp` - Factory interface
- `BlockSim/strategy_factory.cpp` - Factory implementation
- `ScheduledStratSim/main.cpp` - Main simulation executable
- `example_schedule.txt` - Example schedule file
- `STRATEGY_SCHEDULER_README.md` - Original implementation notes
- `DYNAMIC_STRATEGY_SCHEDULER.md` - This documentation

**Modified Files:**
- `Makefile` - Added `scheduled-strat` target
- `BlockSim/logging.h` - Enabled `_GAMEINFO` logging

## Future Enhancements

Potential improvements:
- Support for more than 2 miners
- Event-based triggers (e.g., switch when profit drops below threshold)
- Adaptive strategies that learn from blockchain state
- Parallel execution for faster results
- Real-time visualization of strategy switches
- Automatic comparison with static strategy baselines

## References

- Original BlockSim: https://github.com/hkalodner/BlockSim
- Selfish Mining: Eyal & Sirer (2014)
- Stubborn Mining: Nayak et al. (2016)
