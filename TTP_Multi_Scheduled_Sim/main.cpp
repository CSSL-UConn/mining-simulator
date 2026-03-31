//
//  main.cpp
//  TTP_ScheduledStratMultiAttackersSim
//
//  Simulation with continuous blockchain, dynamic strategy changes,
//  multiple attackers, and per-DAP per-attacker revenue advantage tracking.
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

#define NOISE_IN_TRANSACTIONS false
#define NETWORK_DELAY BlockTime(0)

#define B BlockValue(Value(22) * SATOSHI_PER_BITCOIN)
#define TOTAL_BLOCK_VALUE BlockValue(Value(25) * SATOSHI_PER_BITCOIN)
#define SEC_PER_BLOCK BlockRate(600)
#define A (TOTAL_BLOCK_VALUE - B)/SEC_PER_BLOCK

#define EXPECTED_NUMBER_OF_BLOCKS BlockCount(20000)
#define DAP_LENGTH 2016

int main(int argc, const char *argv[]) {
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <strategy_schedule_file> [output_file]" << std::endl;
        std::cerr << "\nStrategy schedule file format:" << std::endl;
        std::cerr << "  miner_id, start_block, end_block, strategy_name" << std::endl;
        std::cerr << "\nSchedule example:" << std::endl;
        std::cerr << "  0, 0, 9999, selfish" << std::endl;
        std::cerr << "  0, 10000, 19999, stubborn-trail" << std::endl;
        std::cerr << "  1, 0, 19999, selfish" << std::endl;
        std::cerr << "  2, 0, 19999, default-selfish" << std::endl;
        return 1;
    }
    
    std::string scheduleFile = argv[1];
    std::string outputFile = argc >= 3 ? argv[2] : "scheduled_multi_output.txt";
    
    int numberOfGames = 25;
    
    StrategyScheduler scheduler;
    if (!scheduler.loadFromFile(scheduleFile)) {
        std::cerr << "Failed to load strategy schedule from: " << scheduleFile << std::endl;
        return 1;
    }
    
    std::cout << "Successfully loaded schedule with " << scheduler.getScheduleSize() 
              << " strategy changes" << std::endl;
    
    GAMEINFO("\n#####\nRunning Multi-Attacker Dynamic Strategy Switching Simulation with TTP\n#####\n" << std::endl);
    
    // Output file 1: per-game summary
    std::ofstream output(outputFile);
    if (!output.is_open()) {
        std::cerr << "Error opening output file: " << outputFile << std::endl;
        return 1;
    }
    
    output << "# Multi-Attacker Dynamic Strategy Switching Simulation Results" << std::endl;
    output << "# Schedule file: " << scheduleFile << std::endl;
    output << "Gamma, Miner0_ProfitFraction, Miner1_ProfitFraction, Miner0/1_HashRate, "
           << "Miner2_HashRate, Miner0_BlockFraction, Miner1_BlockFraction, "
           << "Miner0_Final_CumRA, Miner1_Final_CumRA, Num_DAPs"
           << std::endl;
           
    // Output file 2: per-DAP per-attacker revenue advantage curves
    std::string dapFilename = outputFile + "_dap_detail.csv";
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

    for (double gammaVal = 0.0; gammaVal < 1.01; gammaVal += 0.25) {
        
        std::cout << "\n=== Testing with Gamma = " << gammaVal << " ===" << std::endl;
        
        for (double hashVal = 0.05; hashVal < 0.61; hashVal += 0.05) {
            
            HashRate miner0Power = HashRate(hashVal / 2);
            HashRate miner1Power = HashRate(hashVal / 2);
            HashRate miner2Power = HashRate(1 - hashVal);
            
            if (((int)(hashVal * 1000)) % 50 == 0) {
                std::cout << "  Testing combined hash rate: " << (hashVal * 100) << "%" << std::endl;
            }
            
            for (int gameNum = 1; gameNum <= numberOfGames; gameNum++) {
                
                GAMEINFO("\nGame #" << gameNum << " | Gamma=" << gammaVal 
                         << " | Miner0=" << miner0Power 
                         << " | Miner1=" << miner1Power 
                         << " | Miner2=" << miner2Power << std::endl);
                
                std::map<std::string, std::unique_ptr<Strategy>> strategyPool;
                std::vector<std::string> strategyNames = {
                    "default", "selfish", "default-selfish", "stubborn-trail", "stubborn-fork", 
                    "stubborn-lead", "stubborn-lead-fork", "stubborn-trail-fork", 
                    "stubborn-lead-trail", "stubborn-lead-trail-fork", "petty", 
                    "lazy-fork", "gap", "rational", "publish-3", "publish-4"
                };
                
                for (const auto& name : strategyNames) {
                    strategyPool[name] = createStrategyByName(name, false, NOISE_IN_TRANSACTIONS, gammaVal);
                }
                
                // Miner setup
                std::string miner0Strategy = scheduler.getActiveStrategy(0, BlockHeight(0));
                std::string miner1Strategy = scheduler.getActiveStrategy(1, BlockHeight(0));
                std::string miner2Strategy = scheduler.getActiveStrategy(2, BlockHeight(0));
                if (miner0Strategy.empty()) miner0Strategy = "selfish";
                if (miner1Strategy.empty()) miner1Strategy = "selfish";
                if (miner2Strategy.empty()) miner2Strategy = "default-selfish";
                
                MinerParameters miner0Params = {0, "Miner-0", miner0Power, NETWORK_DELAY, COST_PER_SEC_TO_MINE};
                MinerParameters miner1Params = {1, "Miner-1", miner1Power, NETWORK_DELAY, COST_PER_SEC_TO_MINE};
                MinerParameters miner2Params = {2, "Miner-2", miner2Power, NETWORK_DELAY, COST_PER_SEC_TO_MINE};
                
                std::vector<std::unique_ptr<Miner>> miners;
                miners.push_back(std::make_unique<Miner>(miner0Params, *strategyPool[miner0Strategy]));
                miners.push_back(std::make_unique<Miner>(miner1Params, *strategyPool[miner1Strategy]));
                miners.push_back(std::make_unique<Miner>(miner2Params, *strategyPool[miner2Strategy]));
                
                MinerGroup minerGroup(std::move(miners));
                
                BlockchainSettings blockchainSettings = {SEC_PER_BLOCK, A, B, EXPECTED_NUMBER_OF_BLOCKS};
                auto blockchain = std::make_unique<Blockchain>(blockchainSettings);
                minerGroup.reset(*blockchain);
                minerGroup.resetOrder();

                // DAPTracker: miners 0 and 1 are attackers
                std::vector<AttackerInfo> attackers;
                attackers.emplace_back(0, hashVal / 2.0, "Miner-0");
                attackers.emplace_back(1, hashVal / 2.0, "Miner-1");

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
                    
                    std::string newMiner0Strategy = scheduler.getActiveStrategy(0, currentHeight);
                    std::string newMiner1Strategy = scheduler.getActiveStrategy(1, currentHeight);
                    std::string newMiner2Strategy = scheduler.getActiveStrategy(2, currentHeight);
                    
                    if (newMiner0Strategy.empty()) newMiner0Strategy = miner0Strategy;
                    if (newMiner1Strategy.empty()) newMiner1Strategy = miner1Strategy;
                    if (newMiner2Strategy.empty()) newMiner2Strategy = miner2Strategy;
                    
                    bool strategyChanged = false;
                    
                    // Miner 0
                    if(newMiner0Strategy == "default-selfish") {
                        double gamma = scheduler.getConnectivityAtHeight(currentHeight); 
                        if(gamma != -1.0) {
                            GAMEINFO("Block " << currentHeight << ": Miner 0 switching default-selfish's gamma to " 
                                 << gamma << std::endl);
                            std::string key = "default-selfish-0" + std::to_string(rawHeight(currentHeight));  
                            strategyPool[key] = createDefaultSelfishStrategy(NOISE_IN_TRANSACTIONS, gamma); 
                            minerGroup.getMiner(0).changeStrategy(*strategyPool["default-selfish"], *blockchain);
                            miner0Strategy = newMiner0Strategy;
                            strategyChanged = true;
                        }
                    } else if (newMiner0Strategy != miner0Strategy) {
                        GAMEINFO("Block " << currentHeight << ": Miner 0 switching from " 
                                 << miner0Strategy << " to " << newMiner0Strategy << std::endl);
                        minerGroup.getMiner(0).changeStrategy(*strategyPool[newMiner0Strategy], *blockchain);
                        miner0Strategy = newMiner0Strategy;
                        strategyChanged = true;
                    }
                  
                    // Miner 1
                    if(newMiner1Strategy == "default-selfish") {
                        double gamma = scheduler.getConnectivityAtHeight(currentHeight);
                        if (gamma != -1.0) {
                            GAMEINFO("Block " << currentHeight << ": Miner 1 switching default-selfish's gamma to " 
                                 << gamma << std::endl);
                            std::string key = "default-selfish-1" + std::to_string(rawHeight(currentHeight)); 
                            strategyPool[key] = createDefaultSelfishStrategy(NOISE_IN_TRANSACTIONS, gamma);
                            minerGroup.getMiner(1).changeStrategy(*strategyPool["default-selfish"], *blockchain);
                            miner1Strategy = newMiner1Strategy;
                            strategyChanged = true;
                        }
                    } else if (newMiner1Strategy != miner1Strategy) {
                        GAMEINFO("Block " << currentHeight << ": Miner 1 switching from " 
                                 << miner1Strategy << " to " << newMiner1Strategy << std::endl);
                        minerGroup.getMiner(1).changeStrategy(*strategyPool[newMiner1Strategy], *blockchain);  
                        miner1Strategy = newMiner1Strategy;
                        strategyChanged = true;
                    }

                    // Miner 2
                    if(newMiner2Strategy == "default-selfish") {
                        double gamma = scheduler.getConnectivityAtHeight(currentHeight); 
                        if(gamma != -1.0) {
                            GAMEINFO("Block " << currentHeight << ": Miner 2 switching default-selfish's gamma to " 
                                 << gamma << std::endl);
                            std::string key = "default-selfish-2" + std::to_string(rawHeight(currentHeight));  
                            strategyPool[key] = createDefaultSelfishStrategy(NOISE_IN_TRANSACTIONS, gamma); 
                            minerGroup.getMiner(2).changeStrategy(*strategyPool["default-selfish"], *blockchain);
                            miner2Strategy = newMiner2Strategy;
                            strategyChanged = true;
                        }
                    } else if (newMiner2Strategy != miner2Strategy) {
                        GAMEINFO("Block " << currentHeight << ": Miner 2 switching from " 
                                 << miner2Strategy << " to " << newMiner2Strategy << std::endl);
                        minerGroup.getMiner(2).changeStrategy(*strategyPool[newMiner2Strategy], *blockchain);
                        miner2Strategy = newMiner2Strategy;
                        strategyChanged = true;
                    }
                    
                    if (strategyChanged) {
                        minerGroup.resetOrder();
                    }
                    
                    // --- Mine / Broadcast / Publish ---
                    BlockTime nextTime = minerGroup.nextEventTime(*blockchain);
                    blockchain->advanceToTime(nextTime);
                    
                    COMMENTARY("Round " << blockchain->getTime() << " of the game..." << std::endl);
                    
                    minerGroup.nextMineRound(*blockchain);
                    minerGroup.nextBroadcastRound(*blockchain);
                    
                    COMMENTARY("Publish phase:" << std::endl);
                    
                    minerGroup.nextPublishRound(*blockchain);
                    dapTracker.checkAndProcessDAP(*blockchain, minerGroup);
                }

                minerGroup.finalize(*blockchain);
                dapTracker.finalize(*blockchain, minerGroup);
                

                auto &winningBlock = blockchain->winningHead();
                auto winningChain = winningBlock.getChain();
                
                BlockCount miner0Blocks(0);
                BlockCount miner1Blocks(0);
                BlockCount miner2Blocks(0);
                Value miner0Profit(0);
                Value miner1Profit(0);
                Value miner2Profit(0);
                
                for (auto block : winningChain) {
                    if (block->height == BlockHeight(0)) break;
                    
                    if (block->miner == &minerGroup.getMiner(0)) {
                        miner0Blocks = BlockCount(rawCount(miner0Blocks) + 1);
                        miner0Profit = Value(rawValue(miner0Profit) + rawValue(block->value));
                    } else if (block->miner == &minerGroup.getMiner(1)) {
                        miner1Blocks = BlockCount(rawCount(miner1Blocks) + 1);
                        miner1Profit = Value(rawValue(miner1Profit) + rawValue(block->value));
                    } else if (block->miner == &minerGroup.getMiner(2)) {
                        miner2Blocks = BlockCount(rawCount(miner2Blocks) + 1);
                        miner2Profit = Value(rawValue(miner2Profit) + rawValue(block->value));
                    } 
                }
                
                Value totalProfit = Value(rawValue(miner0Profit) + rawValue(miner1Profit) + rawValue(miner2Profit));
                BlockCount totalBlocks = BlockCount(rawCount(miner0Blocks) + rawCount(miner1Blocks) + rawCount(miner2Blocks));
                
                double profitFraction0 = (rawValue(totalProfit) > 0) ? 
                    (double)rawValue(miner0Profit) / (double)rawValue(totalProfit) : 0.0;
                double blockFraction0 = (rawCount(totalBlocks) > 0) ? 
                    (double)rawCount(miner0Blocks) / (double)rawCount(totalBlocks) : 0.0;

                double profitFraction1 = (rawValue(totalProfit) > 0) ? 
                    (double)rawValue(miner1Profit) / (double)rawValue(totalProfit) : 0.0;
                double blockFraction1 = (rawCount(totalBlocks) > 0) ? 
                    (double)rawCount(miner1Blocks) / (double)rawCount(totalBlocks) : 0.0;
                
                GAMEINFO("Game complete: Miner0 profit fraction=" << profitFraction0 
                         << ", block fraction=" << blockFraction0 << std::endl);
                GAMEINFO("Game complete: Miner1 profit fraction=" << profitFraction1 
                         << ", block fraction=" << blockFraction1 << std::endl);
                
                // Per-game summary with per-attacker revenue advantage
                const auto &curve = dapTracker.revenueAdvantageCurve();
                double finalCumRA0 = 0, finalCumRA1 = 0;
                if (!curve.empty()) {
                    finalCumRA0 = curve.back().attackerMetrics[0].cumulativeRevenueAdvantage;
                    finalCumRA1 = curve.back().attackerMetrics[1].cumulativeRevenueAdvantage;
                }

                output << gammaVal << ", "
                       << profitFraction0 << ", " 
                       << profitFraction1 << ", "
                       << hashVal / 2.0 << ", " 
                       << (1.0 - hashVal) << ", " 
                       << blockFraction0 << ", " 
                       << blockFraction1 << ", "
                       << finalCumRA0 << ", "
                       << finalCumRA1 << ", "
                       << curve.size()
                       << std::endl;

                // Per-DAP per-attacker revenue advantage rows
                const auto &history = dapTracker.history();
                for (size_t d = 0; d < curve.size(); d++) {
                    const auto &pt  = curve[d];
                    const auto &dap = history[d];

                    double totalBlk = rawCount(dap.totalBlocksOnChain);

                    for (size_t a = 0; a < dapTracker.numAttackers(); a++) {
                        const auto &atk = dapTracker.attackers()[a];
                        const auto &m   = pt.attackerMetrics[a];

                        dapDetail << gammaVal << ","
                                  << hashVal << ","
                                  << gameNum << ","
                                  << pt.dapIndex << ","
                                  << a << ","
                                  << (atk.name.empty() ? std::to_string(a) : atk.name) << ","
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
                                  << rawRate(dap.difficultyRate)
                                  << std::endl;
                    }
                }
            }
        }
    }
    
    output.close();
    dapDetail.close();
    std::cout << "\n=== Simulation Complete ===" << std::endl;
    std::cout << "Results written to: " << outputFile << std::endl;
    std::cout << "DAP detail written to: " << dapFilename << std::endl;
    
    GAMEINFO("All games complete." << std::endl);
    
    return 0;
}