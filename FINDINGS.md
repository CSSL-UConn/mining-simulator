# Dynamic Strategy Switching in Blockchain Mining: Research Findings

## Executive Summary

This research investigated whether dynamic strategy switching can outperform static mining strategies in blockchain networks with varying connectivity conditions. Through extensive simulation testing 2,000+ games across multiple parameter combinations, we discovered that **gamma-adaptive strategy switching can provide up to +0.3% profit advantage** over static strategies when properly configured.

## Research Question

**Can switching mining strategies mid-game be more profitable than staying with a single strategy?**

Initial hypothesis: Switching between honest and selfish mining might capture benefits of both approaches.

**Answer: Yes, but only under specific conditions with compatible strategies.**

## Key Findings

### 1. Honest ↔ Selfish Switching FAILS

**Result: -5.5% disadvantage compared to pure selfish mining**

- Tested switching frequencies: 500, 1000, 2500, 5000 blocks
- Tested asymmetric patterns: 3000:1000 (75% selfish, 25% honest)
- All configurations underperformed pure selfish mining
- Optimal frequency (1000 blocks) still lost by -5.3%

**Why it fails:**
- Fundamental incompatibility between honest and selfish strategies
- Switching to honest mining wastes hash power
- No complementary benefits between the two approaches
- Chain abandonment cost is negligible compared to strategic mismatch


### 2. Gamma-Adaptive Switching SUCCEEDS

**Result: +0.296% advantage with optimal configuration**

When switching between selfish mining variants optimized for different network connectivity (gamma) levels, dynamic strategies outperform static approaches.

**Optimal Configuration:**
- High gamma (0.95): Selfish mining
- Low gamma (0.8): Stubborn-trail mining
- Switching frequency: Every 2500 blocks
- Target hash rate: 20-25%

**Performance by Strategy Pair:**

| Strategy Combination | Advantage | Win Rate | Peak Gain |
|---------------------|-----------|----------|-----------|
| Selfish ↔ Stubborn-Trail | +0.296% | 68.8% | +1.16% |
| Petty ↔ Stubborn-Lead | +0.247% | 75.0% | +0.82% |
| Selfish ↔ Lazy-Fork | +0.232% | 62.5% | +1.17% |
| Selfish ↔ Stubborn-Lead-Fork | +0.120% | 50.0% | +1.54% |
| Selfish ↔ Gap | +0.029% | 50.0% | +1.16% |

**Why it works:**
- All strategies remain in the selfish mining family
- Strategies optimized for different network conditions
- Switches timed to actual gamma changes
- Compatible strategic approaches minimize disruption


### 3. The 20-25% Hash Rate Sweet Spot

**All successful adaptive strategies dominate in the 20-25% hash rate range.**

Performance breakdown:
- **15-20% hash rate**: Minimal advantage (strategies matter less with low power)
- **20-25% hash rate**: Maximum advantage (+0.4% to +0.7%, 80-100% win rates)
- **25-30% hash rate**: Declining advantage (pure selfish becomes dominant)

**Interpretation:**
- Below 20%: Insufficient hash power for strategic differences to matter
- 20-25%: Perfect balance where adaptation provides maximum benefit
- Above 25%: Pure selfish mining so profitable that switching disrupts it

### 4. Switching Frequency Matters

**Tested frequencies: 1000, 2500, 5000 blocks**

| Frequency | Result | Interpretation |
|-----------|--------|----------------|
| 500 blocks | Not tested (too frequent) | Expected: excessive overhead |
| 1000 blocks | +0.239% | Good, especially at high hash rates |
| 2500 blocks | +0.296% | **OPTIMAL** - best balance |
| 5000 blocks | -0.114% | **TOO SLOW** - can't adapt fast enough |

**Optimal: 2500 blocks** (~17 days at 10 min/block)
- Not too frequent: Avoids excessive switching overhead
- Not too slow: Adapts quickly to network changes
- Aligns well with gamma volatility patterns


### 5. Network Volatility Impact

**Tested gamma patterns:**
- Volatile (0.95 ↔ 0.8, every 2500 blocks): +0.296% ⭐
- Rapid (0.95 ↔ 0.8, every 1000 blocks): +0.239%
- Extreme (1.0 ↔ 0.7, every 2500 blocks): +0.132%
- Slow drift (gradual changes over 5000 blocks): -0.114%

**Key insights:**
- Moderate volatility provides best opportunities for adaptation
- Extreme volatility shows diminishing returns (strategies not optimized for such wide ranges)
- Slow changes eliminate adaptation advantage
- Network must change frequently enough to justify switching

### 6. Chain Abandonment Cost is Negligible

**Tested three approaches:**
1. Abandon private chains when switching: -5.531% vs pure selfish
2. Defer switches until chains published: -5.485% vs pure selfish
3. Force switches keeping chains: Crashes (blockchain violations)

**Difference between abandon vs defer: Only 0.046%**

**Conclusion:** The cost of abandoning private chains is minimal. The real problem with honest↔selfish switching is fundamental strategic incompatibility, not chain abandonment overhead.


### 7. Multi-Strategy Rotation

**Triple rotation (Selfish → Stubborn-Trail → Petty): +0.169%**

- Win rate: 62.5%
- Best at 20-25% hash rate: +0.771% (100% win rate)
- Peak gain: +1.64% at 24% hash rate

**Interpretation:**
- Adding a third strategy provides benefit in the sweet spot
- More complex than two-strategy switching
- Diminishing returns compared to optimal two-strategy approach
- May be worth exploring for specific parameter ranges

## Methodology

### Simulation Parameters
- **Games per configuration:** 25
- **Total blocks per game:** 20,000
- **Gamma range:** 0.8 to 1.0 (connectivity rate)
- **Hash rate range:** 0.15 to 0.3 (15% to 30%)
- **Total configurations tested:** 80 (5 gamma × 16 hash rates)
- **Total games:** 2,000 per simulation

### Strategies Tested
- **Honest variants:** default, default-selfish
- **Selfish variants:** selfish, clever-selfish, petty
- **Stubborn variants:** stubborn-trail, stubborn-lead, stubborn-fork, stubborn-lead-trail, stubborn-trail-fork, stubborn-lead-fork, stubborn-lead-trail-fork
- **Other:** lazy-fork, gap, rational, publish-N


## Practical Implications

### When Dynamic Switching Works

**Required conditions:**
1. ✅ Network connectivity varies significantly (gamma volatility)
2. ✅ Miner has 20-25% of total hash power
3. ✅ Strategies are compatible (all selfish variants)
4. ✅ Switches timed to network condition changes
5. ✅ Moderate switching frequency (1000-2500 blocks)

### When to Stay Static

**Use pure selfish mining when:**
- Hash rate > 25% (pure selfish dominates)
- Hash rate < 20% (strategies matter less)
- Network connectivity is stable (no gamma volatility)
- Cannot detect network condition changes reliably

### Real-World Applicability

**Challenges for implementation:**
1. **Detection:** Miners must accurately detect gamma changes in real-time
2. **Coordination:** Multi-miner scenarios add complexity
3. **Magnitude:** +0.3% advantage is measurable but modest
4. **Risk:** Incorrect switching timing could reduce profits

**Opportunities:**
1. Networks with known volatility patterns (e.g., geographic routing changes)
2. Automated systems that can detect connectivity shifts
3. Competitive mining pools seeking marginal advantages
4. Research into better gamma-detection mechanisms


## Technical Architecture

### Implementation Details

**Strategy Scheduler:**
- Loads schedule files defining: `miner_id, start_block, end_block, strategy_name`
- Checks current block height each round
- Triggers strategy switches at scheduled blocks
- Rebuilds mining queue heap after switches

**Gamma Scheduler:**
- Loads gamma schedule files defining: `start_block, end_block, gamma_value`
- Recreates entire strategy pool when gamma changes
- Updates all miners with new gamma-aware strategies
- Maintains blockchain continuity across changes

**Key Design Decisions:**
1. One continuous blockchain per game (not separate games)
2. Miners abandon private chains when switching (call `reset()`)
3. Mining queue rebuilt after strategy changes (`resetOrder()`)
4. Honest miner uses "default-selfish" (gamma-aware) not "default"

### Files Created

**Core Implementation:**
- `BlockSim/strategy_scheduler.hpp/cpp` - Strategy scheduling system
- `BlockSim/strategy_factory.hpp/cpp` - Factory for creating strategies by name
- `BlockSim/gamma_scheduler.hpp/cpp` - Gamma scheduling system
- `ScheduledStratSim/main.cpp` - Full parameter range simulator
- `ScheduledStratSimB5/main.cpp` - Focused B5 parameter range
- `ScheduledStratSimB5Gamma/main.cpp` - Combined strategy + gamma scheduling
- `ScheduledStratSimB5NoAbandon/main.cpp` - Deferred switching variant


**Analysis Tools:**
- `compare_results.py` - Compare 2 baselines vs 1 dynamic strategy
- `compare_switching_frequencies.py` - Direct comparison of two strategies
- `compare_3strategy.py` - Compare 3-strategy rotation vs baselines
- `compare_gamma_varying.py` - Compare fixed vs varying gamma
- `compare_switching_advantage_gamma.py` - Analyze switching advantage with gamma changes

**Schedule Files:**
- `schedules/schedule_b5_*.txt` - Strategy schedules for various combinations
- `schedules/gamma_schedule_*.txt` - Gamma volatility patterns

## Conclusions

### Main Findings

1. **Dynamic strategy switching CAN outperform static strategies** (+0.3% advantage)
2. **Compatibility matters more than chain abandonment cost**
3. **Network volatility creates opportunities for adaptive strategies**
4. **The 20-25% hash rate range is optimal for adaptation**
5. **Moderate switching frequency (2500 blocks) works best**

### Optimal Configuration

**Best performing setup:**
- Strategy pair: Selfish ↔ Stubborn-Trail
- Switching frequency: Every 2500 blocks
- Gamma pattern: Volatile (0.95 ↔ 0.8)
- Hash rate: 20-25%
- Advantage: +0.296% over pure selfish mining


### Future Research Directions

1. **Multi-attacker scenarios:** Test with multiple competing adaptive miners
2. **Extreme parameter ranges:** Test hash rates <15% or >30%
3. **Real-world gamma detection:** Develop practical methods to detect connectivity changes
4. **Strategy optimization:** Design strategies specifically for switchability
5. **Longer time horizons:** Test with millions of blocks to see long-term effects
6. **Transaction fee dynamics:** Incorporate varying block rewards
7. **Network topology:** Test with different network structures beyond gamma
8. **Machine learning:** Use ML to predict optimal switching times

### Limitations

1. **Simulation-based:** Results based on BlockSim simulator, not real blockchain
2. **Single attacker:** Only tested one adaptive miner vs honest miners
3. **Perfect information:** Assumes perfect knowledge of gamma changes
4. **Fixed parameters:** Block time, reward structure held constant
5. **No transaction fees:** Only block rewards considered
6. **Simplified network model:** Gamma parameter abstracts complex network dynamics

## References

**Codebase:** BlockSim - Blockchain Mining Simulator
**Simulation Framework:** C++ with GSL (GNU Scientific Library)
**Analysis Tools:** Python with pandas
**Parameter Range:** B5 focused testing (gamma 0.8-1.0, hash rate 15-30%)

---

*Research conducted: March 2026*
*Total simulations run: 50,000+ games*
*Total computation time: ~10+ hours*
