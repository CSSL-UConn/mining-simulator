# Strategy Scheduler

This feature allows you to run blockchain mining simulations where miners change their strategies based on a duration schedule defined in a text file.

## Overview

The strategy scheduler reads a text file that specifies:
- Which miner should change strategy
- How many blocks to use that strategy
- What strategy to use

This allows you to simulate dynamic strategy changes during a mining game.

## File Format

The schedule file uses a simple CSV format:

```
miner_id, duration_in_blocks, strategy_name
```

- **miner_id**: The ID of the miner (0, 1, 2, etc.)
- **duration_in_blocks**: How many blocks to mine with this strategy
- **strategy_name**: The name of the strategy to use

### Example Schedule File

```
# Miner 0 starts with selfish mining for 20 blocks
0, 20, selfish

# Then switches to stubborn-trail for 30 blocks
0, 30, stubborn-trail

# Finally switches to honest mining for 50 blocks
0, 50, default

# Miner 1 stays honest the entire time
1, 100, default
```

Lines starting with `#` are comments and will be ignored.

## Available Strategies

### Basic Strategies
- `default` or `honest` - Standard honest mining
- `selfish` - Classic selfish mining
- `default-selfish` - Selfish mining with gamma parameter
- `clever-selfish` - Clever selfish mining variant

### Stubborn Mining Strategies
- `stubborn-trail` (or `stubborn-trail-1`) - Stubborn trail mining
- `stubborn-trail-2`, `stubborn-trail-3`, `stubborn-trail-4` - Variants with different trail lengths
- `stubborn-fork` - Stubborn fork mining
- `stubborn-lead` - Stubborn lead mining
- `stubborn-lead-fork` - Combination of lead and fork
- `stubborn-lead-trail` - Combination of lead and trail
- `stubborn-trail-fork` - Combination of trail and fork
- `stubborn-lead-trail-fork` - All three combined

### Other Strategies
- `petty` - Petty mining
- `lazy-fork` - Lazy fork strategy
- `gap` - Gap mining strategy
- `rational` - Rational mining
- `publish-3` or `publish3` - Publish N strategy (N=3)
- `publish-4` or `publish4` - Publish N strategy (N=4)

Strategy names are case-insensitive.

## Building

To build the scheduled strategy simulator:

```bash
make scheduled-strat
```

## Usage

Run the simulator with a strategy schedule file:

```bash
./scheduled-strat example_schedule.txt [output_file]
```

Arguments:
- `example_schedule.txt` - Path to your strategy schedule file (required)
- `output_file` - Path to output results file (optional, defaults to `scheduled_output.txt`)

## Output

The simulator produces a CSV file with the following columns:
- Block number
- Miner 0's current strategy
- Miner 0's total profit
- Miner 1's current strategy
- Miner 1's total profit

Example output:
```
Block, Miner0_Strategy, Miner0_Profit, Miner1_Strategy, Miner1_Profit
10, selfish, 150000000, default, 100000000
20, stubborn-trail, 300000000, default, 200000000
30, stubborn-trail, 450000000, default, 300000000
```

## How It Works

1. The scheduler loads the strategy schedule from the file at startup
2. Before each block is mined, it checks if any miner should change strategy
3. If a miner's current strategy duration has expired, it switches to the next strategy in the schedule
4. The scheduler tracks how many blocks remain for each miner's current strategy
5. Results are logged periodically to the output file

## Example Workflow

1. Create a strategy schedule file:
```bash
cat > my_schedule.txt << EOF
# Test selfish vs honest
0, 50, selfish
0, 50, default
1, 100, default
EOF
```

2. Build the simulator:
```bash
make scheduled-strat
```

3. Run the simulation:
```bash
./scheduled-strat my_schedule.txt results.txt
```

4. Analyze the results in `results.txt`

## Extending

To add new strategies:

1. Add the strategy creation function to `BlockSim/strategy_factory.cpp`
2. Update the strategy name mapping in `createStrategyByName()`
3. Rebuild with `make scheduled-strat`

## Notes

- The simulation currently supports 2 miners by default (can be modified in the main.cpp)
- Each miner can have multiple strategy changes scheduled
- If a miner runs out of scheduled strategies, it keeps using the last strategy
- The total number of blocks is set by `EXPECTED_NUMBER_OF_BLOCKS` in the code
