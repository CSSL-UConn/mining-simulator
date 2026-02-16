//
//  main.cpp
//  ScheduledStratSim
//
//  Simulation with continuous blockchain and dynamic strategy changes
//  Structured like SelfishSim for statistical testing
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

#define B BlockValue(0)
#define TOTAL_BLOCK_VALUE BlockValue(Value(25) * SATOSHI_PER_BITCOIN)
#define SEC_PER_BLOCK BlockRate(600)
#define A (TOTAL_BLOCK_VALUE - B)/SEC_PER_BLOCK

#define EXPECTED_NUMBER_OF_BLOCKS BlockCount(20000)

int main(int argc, const char *argv[]) {
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <strategy_schedule_file> [output_file]" << std::endl;
        std::cerr << "\nStrategy schedule file format:" << std::endl;
        std::cerr << "  miner_id, start_block, end_block, strategy_name" << std::endl;
        std::cerr << "\nThis runs multiple games testing different gamma values and hash rates" << std::endl;
        std::cerr << "with dynamic strategy switching (continuous blockchain per game)." << std::endl;
        std::cerr << "\nSchedule example:" << std::endl;
        std::cerr << "  0, 0, 9999, selfish" << std::endl;
        std::cerr << "  0, 10000, 19999, stubborn-trail" << std::endl;
        std::cerr << "  1, 0, 19999, default-selfish" << std::endl;
        return 1;
    }
    
    std::string scheduleFile = argv[1];
    std::string outputFile = argc >= 3 ? argv[2] : "scheduled_output.txt";
    
    int numberOfGames = 25;
    
    StrategyScheduler scheduler;
    if (!scheduler.loadFromFile(scheduleFile)) {
        std::cerr << "Failed to load strategy schedule from: " << scheduleFile << std::endl;
        return 1;
    }
    
    std::cout << "Successfully loaded schedule with " << scheduler.getScheduleSize() << " strategy changes" << std::endl;
    
    GAMEINFO("\n#####\nRunning Dynamic Strategy Switching Simulation\n#####\n" << std::endl);
    
    std::ofstream output(outputFile);
    if (!output.is_open()) {
        std::cerr << "Error opening output file: " << outputFile << std::endl;
        return 1;
    }
    
    output << "# Dynamic Strategy Switching Simulation Results" << std::endl;
    output << "# Schedule file: " << scheduleFile << std::endl;
    output << "# Miner 0: selfish (0-6666), stubborn-trail (6667-13333), petty (13334-19999)" << std::endl;
    output << "# Miner 1: default-selfish (0-19999)" << std::endl;
    output << "Gamma, Miner0_ProfitFraction, Miner0_HashRate, Miner1_HashRate, Miner0_BlockFraction" << std::endl;

    
    for (double gammaVal = 0.0; gammaVal < 1.01; gammaVal += 0.25) {
        
        std::cout << "\n=== Testing with Gamma = " << gammaVal << " ===" << std::endl;
        
        for (double hashVal = 0.005; hashVal < 0.51; hashVal += 0.005) {
            
            HashRate miner0Power = HashRate(hashVal);
            HashRate miner1Power = HashRate(1.0 - hashVal);
            
            if (((int)(hashVal * 1000)) % 50 == 0) {
                std::cout << "  Testing hash rate: " << (hashVal * 100) << "%" << std::endl;
            }
            
            for (int gameNum = 1; gameNum <= numberOfGames; gameNum++) {
                
                GAMEINFO("\nGame #" << gameNum << " | Gamma=" << gammaVal 
                         << " | Miner0=" << miner0Power << std::endl);
                
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
                
                std::string miner0Strategy = scheduler.getActiveStrategy(0, BlockHeight(0));
                std::string miner1Strategy = scheduler.getActiveStrategy(1, BlockHeight(0));
                if (miner0Strategy.empty()) miner0Strategy = "selfish";
                if (miner1Strategy.empty()) miner1Strategy = "default-selfish";
                
                MinerParameters miner0Params = {0, "Miner-0", miner0Power, NETWORK_DELAY, COST_PER_SEC_TO_MINE};
                MinerParameters miner1Params = {1, "Miner-1", miner1Power, NETWORK_DELAY, COST_PER_SEC_TO_MINE};
                
                std::vector<std::unique_ptr<Miner>> miners;
                miners.push_back(std::make_unique<Miner>(miner0Params, *strategyPool[miner0Strategy]));
                miners.push_back(std::make_unique<Miner>(miner1Params, *strategyPool[miner1Strategy]));
                
                MinerGroup minerGroup(std::move(miners));
                
                BlockchainSettings blockchainSettings = {SEC_PER_BLOCK, A, B, EXPECTED_NUMBER_OF_BLOCKS};
                auto blockchain = std::make_unique<Blockchain>(blockchainSettings);
                minerGroup.reset(*blockchain);
                minerGroup.resetOrder();
                
                BlockTime totalSeconds = EXPECTED_NUMBER_OF_BLOCKS * SEC_PER_BLOCK;

                
                while (blockchain->getTime() < totalSeconds) {
                    BlockHeight currentHeight = blockchain->getMaxHeightPub();
                    
                    std::string newMiner0Strategy = scheduler.getActiveStrategy(0, currentHeight);
                    std::string newMiner1Strategy = scheduler.getActiveStrategy(1, currentHeight);
                    
                    if (newMiner0Strategy.empty()) newMiner0Strategy = miner0Strategy;
                    if (newMiner1Strategy.empty()) newMiner1Strategy = miner1Strategy;
                    
                    bool strategyChanged = false;
                    
                    if (newMiner0Strategy != miner0Strategy) {
                        GAMEINFO("Block " << currentHeight << ": Miner 0 switching from " 
                                 << miner0Strategy << " to " << newMiner0Strategy << std::endl);
                        minerGroup.getMiner(0).changeStrategy(*strategyPool[newMiner0Strategy], *blockchain);
                        miner0Strategy = newMiner0Strategy;
                        strategyChanged = true;
                    }
                    
                    if (newMiner1Strategy != miner1Strategy) {
                        GAMEINFO("Block " << currentHeight << ": Miner 1 switching from " 
                                 << miner1Strategy << " to " << newMiner1Strategy << std::endl);
                        minerGroup.getMiner(1).changeStrategy(*strategyPool[newMiner1Strategy], *blockchain);
                        miner1Strategy = newMiner1Strategy;
                        strategyChanged = true;
                    }
                    
                    if (strategyChanged) {
                        minerGroup.resetOrder();
                    }
                    
                    BlockTime nextTime = minerGroup.nextEventTime(*blockchain);
                    blockchain->advanceToTime(nextTime);
                    
                    COMMENTARY("Round " << blockchain->getTime() << " of the game..." << std::endl);
                    
                    minerGroup.nextMineRound(*blockchain);
                    minerGroup.nextBroadcastRound(*blockchain);
                    
                    COMMENTARY("Publish phase:" << std::endl);
                    
                    minerGroup.nextPublishRound(*blockchain);
                }

                
                minerGroup.finalize(*blockchain);
                
                auto &winningBlock = blockchain->winningHead();
                auto winningChain = winningBlock.getChain();
                
                BlockCount miner0Blocks(0);
                BlockCount miner1Blocks(0);
                Value miner0Profit(0);
                Value miner1Profit(0);
                
                for (auto block : winningChain) {
                    if (block->height == BlockHeight(0)) break;
                    
                    if (block->miner == &minerGroup.getMiner(0)) {
                        miner0Blocks = BlockCount(rawCount(miner0Blocks) + 1);
                        miner0Profit = Value(rawValue(miner0Profit) + rawValue(block->value));
                    } else if (block->miner == &minerGroup.getMiner(1)) {
                        miner1Blocks = BlockCount(rawCount(miner1Blocks) + 1);
                        miner1Profit = Value(rawValue(miner1Profit) + rawValue(block->value));
                    }
                }
                
                Value totalProfit = Value(rawValue(miner0Profit) + rawValue(miner1Profit));
                BlockCount totalBlocks = BlockCount(rawCount(miner0Blocks) + rawCount(miner1Blocks));
                
                double profitFraction = (rawValue(totalProfit) > 0) ? 
                    (double)rawValue(miner0Profit) / (double)rawValue(totalProfit) : 0.0;
                double blockFraction = (rawCount(totalBlocks) > 0) ? 
                    (double)rawCount(miner0Blocks) / (double)rawCount(totalBlocks) : 0.0;
                
                GAMEINFO("Game complete: Miner0 profit fraction=" << profitFraction 
                         << ", block fraction=" << blockFraction << std::endl);
                
                output << gammaVal << ", " << profitFraction << ", " << hashVal << ", " 
                       << (1.0 - hashVal) << ", " << blockFraction << std::endl;
            }
        }
    }
    
    output.close();
    std::cout << "\n=== Simulation Complete ===" << std::endl;
    std::cout << "Results written to: " << outputFile << std::endl;
    
    GAMEINFO("All games complete." << std::endl);
    
    return 0;
}
