//
//  main.cpp
//  TTP_Scheduled_3Miner_Sim
//
//  3-miner simulation with dynamic strategy switching and DAP tracking.
//  Miner 0 & 1: attackers (strategies from schedule)
//  Miner 2: honest miner
//

#include "BlockSim/strategy.hpp"
#include "BlockSim/utils.hpp"
#include "BlockSim/miner.hpp"
#include "BlockSim/block.hpp"
#include "BlockSim/blockchain.hpp"
#include "BlockSim/minerStrategies.h"
#include "BlockSim/logging.h"
#include "BlockSim/game.hpp"
#include "BlockSim/minerGroup.hpp"
#include "BlockSim/game_result.hpp"
#include "BlockSim/miner_result.hpp"
#include "BlockSim/mining_style.hpp"
#include "BlockSim/strategy_scheduler.hpp"
#include "BlockSim/strategy_factory.hpp"
#include "BlockSim/dap_tracker.hpp"

#include <cassert>
#include <iostream>
#include <fstream>
#include <cmath>
#include <queue>
#include <random>
#include <algorithm>
#include <vector>
#include <map>

#define NETWORK_DELAY BlockTime(0)

#define B BlockValue(Value(25) * SATOSHI_PER_BITCOIN)
#define TOTAL_BLOCK_VALUE BlockValue(Value(30) * SATOSHI_PER_BITCOIN)
#define SEC_PER_BLOCK BlockRate(600)
#define A (TOTAL_BLOCK_VALUE - B)/SEC_PER_BLOCK

#define EXPECTED_NUMBER_OF_BLOCKS BlockCount(24192)
#define DAP_LENGTH 2016

// Helper to get or create a strategy in the pool
Strategy& getOrCreateStrategy(std::map<std::string, std::unique_ptr<Strategy>>& pool, 
                               const std::string& name, double gamma,
                               bool noisy, bool whaleEnabled, double whaleProb, double whaleMult) {
    if (pool.find(name) == pool.end()) {
        pool[name] = createStrategyByName(name, false, noisy, gamma, 
                                          whaleEnabled, whaleProb, whaleMult);
    }
    return *pool[name];
}

int main(int argc, const char *argv[]) {
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <strategy_schedule_file> [output_suffix]" << std::endl;
        std::cerr << "\nStrategy schedule file format:" << std::endl;
        std::cerr << "  miner_id, start_block, end_block, strategy_name, gamma" << std::endl;
        std::cerr << "\nSchedule example:" << std::endl;
        std::cerr << "  0, 0, 9999, selfish, -1" << std::endl;
        std::cerr << "  0, 10000, 19999, incentive-trail-1-2.0, -1" << std::endl;
        std::cerr << "  1, 0, 19999, selfish, -1" << std::endl;
        return 1;
    }
    
    std::string scheduleFile = argv[1];
    std::string outputSuffix = argc >= 3 ? argv[2] : "output";
    
    int numberOfGames = 100;
    
    StrategyScheduler scheduler;
    if (!scheduler.loadFromFile(scheduleFile)) {
        std::cerr << "Failed to load strategy schedule from: " << scheduleFile << std::endl;
        return 1;
    }
   
    std::cout << "Config: noisyTransaction=" << scheduler.noisyTransaction
              << " whaleEnabled=" << scheduler.whaleEnabled
              << " whaleProb=" << scheduler.whaleProb
              << " whaleMultiplier=" << scheduler.whaleMultiplier
              << std::endl;

    std::cout << "Successfully loaded schedule with " << scheduler.getScheduleSize() 
              << " strategy changes" << std::endl;
    
    GAMEINFO("\n#####\nRunning 3-Miner Dynamic Strategy Simulation with TTP\n#####\n" << std::endl);

    // Output files
    char filename[1024] = {0};
    sprintf(filename, "%s_%s.txt", argv[0], outputSuffix.c_str());
    std::ofstream output(filename);
    if (!output.is_open()) {
        std::cerr << "Error opening output file: " << filename << std::endl;
        return 1;
    }
    
    output << "# 3-Miner Dynamic Strategy Simulation Results" << std::endl;
    output << "# Schedule file: " << scheduleFile << std::endl;
    output << "Gamma, Miner0_ProfitFrac, Miner1_ProfitFrac, Honest_ProfitFrac, "
           << "Combined_HashRate, Miner0_CumRA, Miner1_CumRA, Num_DAPs"
           << std::endl;

    char dapFilename[1024] = {0};
    sprintf(dapFilename, "%s_%s_dap_detail.csv", argv[0], outputSuffix.c_str());
    std::ofstream dapDetail(dapFilename);
    if (!dapDetail.is_open()) {
        std::cerr << "Error opening DAP detail file: " << dapFilename << std::endl;
        return 1;
    }
    dapDetail << "gamma,combined_hash_rate,game_num,"
              << "dap,attacker_id,attacker_name,attacker_alpha,"
              << "start_height,end_height,start_time,end_time,"
              << "atk_blocks,total_blocks,"
              << "atk_revenue,honest_counterfactual_revenue,revenue_advantage,"
              << "cumulative_atk_revenue,cumulative_honest_revenue,cumulative_revenue_advantage,"
              << "rrr,seconds_per_block,orphan_rate"
              << std::endl;

    // Parameter sweep
    for (double gammaVal = 0.0; gammaVal < 1.01; gammaVal += 0.01) {
        
        std::cout << "\n=== Gamma = " << gammaVal << " ===" << std::endl;
        
        for (double hashVal = 0.02; hashVal < 0.70; hashVal += 0.01) {
            
            // Split hash rate: each attacker gets half, rest is honest
            HashRate attacker0Power = HashRate(hashVal / 2.0);
            HashRate attacker1Power = HashRate(hashVal / 2.0);
            HashRate honestPower = HashRate(1.0 - hashVal);
            
            for (int gameNum = 1; gameNum <= numberOfGames; gameNum++) {
                
                std::map<std::string, std::unique_ptr<Strategy>> strategyPool;

                std::string miner0Strategy = scheduler.getActiveStrategy(0, BlockHeight(0));
                std::string miner1Strategy = scheduler.getActiveStrategy(1, BlockHeight(0));
                if (miner0Strategy.empty()) miner0Strategy = "selfish";
                if (miner1Strategy.empty()) miner1Strategy = "selfish";
                
                // Create honest-but-rational strategy for miner 2
                // Rational miners choose forks based on expected profit, not blindly following protocol
                auto honestStrat = createRationalStrategy(scheduler.noisyTransaction);
                
                MinerParameters miner0Params = {0, "Attacker-0", attacker0Power, NETWORK_DELAY, COST_PER_SEC_TO_MINE};
                MinerParameters miner1Params = {1, "Attacker-1", attacker1Power, NETWORK_DELAY, COST_PER_SEC_TO_MINE};
                MinerParameters honestParams = {2, "Honest", honestPower, NETWORK_DELAY, COST_PER_SEC_TO_MINE};
                
                std::vector<std::unique_ptr<Miner>> miners;
                miners.push_back(std::make_unique<Miner>(miner0Params, 
                    getOrCreateStrategy(strategyPool, miner0Strategy, gammaVal,
                        scheduler.noisyTransaction, scheduler.whaleEnabled, 
                        scheduler.whaleProb, scheduler.whaleMultiplier)));
                miners.push_back(std::make_unique<Miner>(miner1Params, 
                    getOrCreateStrategy(strategyPool, miner1Strategy, gammaVal,
                        scheduler.noisyTransaction, scheduler.whaleEnabled, 
                        scheduler.whaleProb, scheduler.whaleMultiplier)));
                miners.push_back(std::make_unique<Miner>(honestParams, *honestStrat));
                
                MinerGroup minerGroup(std::move(miners));
                
                BlockchainSettings blockchainSettings = {SEC_PER_BLOCK, A, B, EXPECTED_NUMBER_OF_BLOCKS};
                auto blockchain = std::make_unique<Blockchain>(blockchainSettings);
                minerGroup.reset(*blockchain);
                minerGroup.resetOrder();

                // Track both attackers
                std::vector<AttackerInfo> attackers;
                attackers.emplace_back(0, rawRate(attacker0Power), "Attacker-0");
                attackers.emplace_back(1, rawRate(attacker1Power), "Attacker-1");

                DAPTracker dapTracker(
                    DAP_LENGTH,
                    minerGroup.miners.size(),
                    attackers,
                    static_cast<double>(rawValue(blockchainSettings.blockReward)),
                    static_cast<double>(rawRate(blockchainSettings.transactionFeeRate))
                );
                dapTracker.reset(blockchainSettings.secondsPerBlock);

                BlockTime totalSeconds = EXPECTED_NUMBER_OF_BLOCKS * SEC_PER_BLOCK;

                while (blockchain->getTime() < totalSeconds) {

                    BlockHeight currentHeight = blockchain->getMaxHeightPub();

                    double feeMultiplier = scheduler.getCurrentFeeMultiplier(currentHeight);
                    blockchain->updateFeeMultiplier(feeMultiplier);

                    std::string newMiner0Strategy = scheduler.getActiveStrategy(0, currentHeight);
                    std::string newMiner1Strategy = scheduler.getActiveStrategy(1, currentHeight);
                    
                    if (newMiner0Strategy.empty()) newMiner0Strategy = miner0Strategy;
                    if (newMiner1Strategy.empty()) newMiner1Strategy = miner1Strategy;
                    
                    bool strategyChanged = false;
                    
                    if (newMiner0Strategy != miner0Strategy) {
                        GAMEINFO("Block " << currentHeight << ": Miner 0 switching from " 
                                 << miner0Strategy << " to " << newMiner0Strategy << std::endl);
                        minerGroup.getMiner(0).changeStrategy(
                            getOrCreateStrategy(strategyPool, newMiner0Strategy, gammaVal,
                                scheduler.noisyTransaction, scheduler.whaleEnabled, 
                                scheduler.whaleProb, scheduler.whaleMultiplier), 
                            *blockchain);
                        miner0Strategy = newMiner0Strategy;
                        strategyChanged = true;
                    }
                  
                    if (newMiner1Strategy != miner1Strategy) {
                        GAMEINFO("Block " << currentHeight << ": Miner 1 switching from " 
                                 << miner1Strategy << " to " << newMiner1Strategy << std::endl);
                        minerGroup.getMiner(1).changeStrategy(
                            getOrCreateStrategy(strategyPool, newMiner1Strategy, gammaVal,
                                scheduler.noisyTransaction, scheduler.whaleEnabled, 
                                scheduler.whaleProb, scheduler.whaleMultiplier), 
                            *blockchain);
                        miner1Strategy = newMiner1Strategy;
                        strategyChanged = true;
                    }
                    
                    if (strategyChanged) {
                        minerGroup.resetOrder();
                    }
                    
                    BlockTime nextTime = minerGroup.nextEventTime(*blockchain);
                    blockchain->advanceToTime(nextTime);
                    
                    minerGroup.nextMineRound(*blockchain);
                    minerGroup.nextBroadcastRound(*blockchain);
                    minerGroup.nextPublishRound(*blockchain);

                    dapTracker.checkAndProcessDAP(*blockchain, minerGroup);
                }
                minerGroup.finalize(*blockchain);
                dapTracker.finalize(*blockchain, minerGroup);

                // Calculate results
                auto &winningBlock = blockchain->winningHead();
                auto winningChain = winningBlock.getChain();
                
                Value miner0Profit(0), miner1Profit(0), honestProfit(0);
                
                for (auto block : winningChain) {
                    if (block->height == BlockHeight(0)) break;
                    
                    if (block->miner == &minerGroup.getMiner(0)) {
                        miner0Profit = Value(rawValue(miner0Profit) + rawValue(block->value));
                    } else if (block->miner == &minerGroup.getMiner(1)) {
                        miner1Profit = Value(rawValue(miner1Profit) + rawValue(block->value));
                    } else if (block->miner == &minerGroup.getMiner(2)) {
                        honestProfit = Value(rawValue(honestProfit) + rawValue(block->value));
                    }
                }
                
                Value totalProfit = Value(rawValue(miner0Profit) + rawValue(miner1Profit) + rawValue(honestProfit));
                
                double m0Frac = (rawValue(totalProfit) > 0) ? (double)rawValue(miner0Profit) / (double)rawValue(totalProfit) : 0.0;
                double m1Frac = (rawValue(totalProfit) > 0) ? (double)rawValue(miner1Profit) / (double)rawValue(totalProfit) : 0.0;
                double hFrac = (rawValue(totalProfit) > 0) ? (double)rawValue(honestProfit) / (double)rawValue(totalProfit) : 0.0;
                
                const auto &curve = dapTracker.revenueAdvantageCurve();
                double finalCumRA0 = 0, finalCumRA1 = 0;
                if (!curve.empty()) {
                    finalCumRA0 = curve.back().attackerMetrics[0].cumulativeRevenueAdvantage;
                    finalCumRA1 = curve.back().attackerMetrics[1].cumulativeRevenueAdvantage;
                }

                output << gammaVal << ", " << m0Frac << ", " << m1Frac << ", " << hFrac << ", "
                       << hashVal << ", " << finalCumRA0 << ", " << finalCumRA1 << ", " 
                       << curve.size() << std::endl;

                // Write DAP detail
                const auto &history = dapTracker.history();
                for (size_t d = 0; d < curve.size(); d++) {
                    const auto &pt  = curve[d];
                    const auto &dap = history[d];

                    double totalBlk = rawCount(dap.totalBlocksOnChain);
                    double totalMined = rawCount(dap.totalBlocksMined);
                    double orphanRate = (totalMined > 0) ? 1.0 - (totalBlk / totalMined) : 0.0;

                    for (size_t a = 0; a < dapTracker.numAttackers(); a++) {
                        const auto &atk = dapTracker.attackers()[a];
                        const auto &m   = pt.attackerMetrics[a];

                        dapDetail << gammaVal << ","
                                  << hashVal << ","
                                  << gameNum << ","
                                  << pt.dapIndex << ","
                                  << a << ","
                                  << atk.name << ","
                                  << atk.alpha << ","
                                  << rawHeight(dap.startHeight) << ","
                                  << rawHeight(dap.endHeight) << ","
                                  << rawTime(dap.startTime) << ","
                                  << rawTime(dap.endTime) << ","
                                  << static_cast<int>(m.atkBlocksThisDAP) << ","
                                  << static_cast<int>(totalBlk) << ","
                                  << m.atkRevenueThisDAP << ","
                                  << m.honestCounterfactualThisDAP << ","
                                  << m.revenueAdvantageThisDAP << ","
                                  << m.cumulativeAtkRevenue << ","
                                  << m.cumulativeHonestCounterfactual << ","
                                  << m.cumulativeRevenueAdvantage << ","
                                  << m.rrr << ","
                                  << rawRate(dap.difficultyRate) << ","
                                  << orphanRate
                                  << std::endl;
                    }
                }
            } // gameNum
        } // hashVal
    } // gammaVal
    
    output.close();
    dapDetail.close();
    
    std::cout << "\n=== Simulation Complete ===" << std::endl;
    std::cerr << "Results written to: " << filename << std::endl;
    std::cerr << "DAP detail written to: " << dapFilename << std::endl;
    
    return 0;
}
