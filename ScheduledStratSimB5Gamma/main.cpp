//
//  main.cpp
//  ScheduledStratSimB5Gamma
//
//  Focused simulation with dynamic gamma (connectivity) changes
//  Tests how varying network conditions affect strategy profitability
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
#include "BlockSim/gamma_scheduler.hpp"
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
    
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <strategy_schedule_file> <gamma_schedule_file> [output_file]" << std::endl;
        std::cerr << "\nScheduledStratSimB5Gamma - Dynamic Gamma Testing" << std::endl;
        std::cerr << "Tests single attacker with varying network connectivity (gamma)" << std::endl;
        std::cerr << "Hash rate: 0.15 to 0.3 in 0.01 increments" << std::endl;
        std::cerr << "\nStrategy schedule: miner_id, start_block, end_block, strategy_name" << std::endl;
        std::cerr << "Gamma schedule: start_block, end_block, gamma_value" << std::endl;
        return 1;
    }
    
    std::string strategyFile = argv[1];
    std::string gammaFile = argv[2];
    std::string outputFile = argc >= 4 ? argv[3] : "scheduled_b5_gamma_output.txt";
    
    int numberOfGames = 25;
    
    StrategyScheduler stratScheduler;
    if (!stratScheduler.loadFromFile(strategyFile)) {
        std::cerr << "Failed to load strategy schedule from: " << strategyFile << std::endl;
        return 1;
    }
    
    GammaScheduler gammaScheduler;
    if (!gammaScheduler.loadFromFile(gammaFile)) {
        std::cerr << "Failed to load gamma schedule from: " << gammaFile << std::endl;
        return 1;
    }
    
    std::cout << "Successfully loaded schedules:" << std::endl;
    std::cout << "  Strategy changes: " << stratScheduler.getScheduleSize() << std::endl;
    std::cout << "  Gamma changes: " << gammaScheduler.getScheduleSize() << std::endl;
    
    GAMEINFO("\n#####\nRunning B5 Gamma Dynamic Simulation\n#####\n" << std::endl);
    GAMEINFO("Single attacker with dynamic gamma (network connectivity)" << std::endl);
    
    std::ofstream output(outputFile);
    if (!output.is_open()) {
        std::cerr << "Error opening output file: " << outputFile << std::endl;
        return 1;
    }
    
    output << "# B5 Gamma Dynamic Simulation Results" << std::endl;
    output << "# Strategy schedule: " << strategyFile << std::endl;
    output << "# Gamma schedule: " << gammaFile << std::endl;
    output << "# Hash rate range: 0.15 - 0.3 (15% - 30%) in 0.01 increments" << std::endl;
    output << "InitialGamma, Miner0_ProfitFraction, Miner0_HashRate, Miner1_HashRate, Miner0_BlockFraction" << std::endl;

    
    // Test across hash rate range (gamma varies within each game)
    for (double hashVal = 0.15; hashVal < 0.31; hashVal += 0.01) {
        
        HashRate miner0Power = HashRate(hashVal);
        HashRate miner1Power = HashRate(1.0 - hashVal);
        
        std::cout << "\n=== Testing hash rate: " << (hashVal * 100) << "% ===" << std::endl;
        
        for (int gameNum = 1; gameNum <= numberOfGames; gameNum++) {
            
            double initialGamma = gammaScheduler.getActiveGamma(BlockHeight(0));
            double currentGamma = initialGamma;
            
            GAMEINFO("\nGame #" << gameNum << " | InitialGamma=" << initialGamma 
                     << " | Miner0=" << miner0Power << std::endl);
            
            // Create initial strategy pool with initial gamma
            std::map<std::string, std::unique_ptr<Strategy>> strategyPool;
            std::vector<std::string> strategyNames = {
                "default", "selfish", "default-selfish", "stubborn-trail", "stubborn-fork", 
                "stubborn-lead", "stubborn-lead-fork", "stubborn-trail-fork", 
                "stubborn-lead-trail", "stubborn-lead-trail-fork", "petty", 
                "lazy-fork", "gap", "rational", "publish-3", "publish-4"
            };
            
            for (const auto& name : strategyNames) {
                strategyPool[name] = createStrategyByName(name, false, NOISE_IN_TRANSACTIONS, currentGamma);
            }
            
            std::string miner0Strategy = stratScheduler.getActiveStrategy(0, BlockHeight(0));
            std::string miner1Strategy = stratScheduler.getActiveStrategy(1, BlockHeight(0));
            if (miner0Strategy.empty()) miner0Strategy = "selfish";
            if (miner1Strategy.empty()) miner1Strategy = "default-selfish";
            
            MinerParameters miner0Params = {0, "Attacker", miner0Power, NETWORK_DELAY, COST_PER_SEC_TO_MINE};
            MinerParameters miner1Params = {1, "Honest", miner1Power, NETWORK_DELAY, COST_PER_SEC_TO_MINE};
            
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
                
                // Check for gamma changes
                double newGamma = gammaScheduler.getActiveGamma(currentHeight);
                if (newGamma != currentGamma) {
                    GAMEINFO("Block " << currentHeight << ": Gamma changing from " 
                             << currentGamma << " to " << newGamma << std::endl);
                    
                    // Recreate strategy pool with new gamma
                    strategyPool.clear();
                    for (const auto& name : strategyNames) {
                        strategyPool[name] = createStrategyByName(name, false, NOISE_IN_TRANSACTIONS, newGamma);
                    }
                    
                    // Update both miners with new gamma strategies
                    // Miner 0 (attacker) keeps current strategy type but with new gamma
                    // Miner 1 (honest) always uses default-selfish with new gamma
                    minerGroup.getMiner(0).reset(*blockchain);
                    minerGroup.getMiner(0).changeStrategy(*strategyPool[miner0Strategy], *blockchain);
                    
                    minerGroup.getMiner(1).reset(*blockchain);
                    minerGroup.getMiner(1).changeStrategy(*strategyPool["default-selfish"], *blockchain);
                    
                    minerGroup.resetOrder();
                    currentGamma = newGamma;
                }
                
                // Check for strategy changes (only for attacker - miner 0)
                std::string newMiner0Strategy = stratScheduler.getActiveStrategy(0, currentHeight);
                if (newMiner0Strategy.empty()) newMiner0Strategy = miner0Strategy;
                
                if (newMiner0Strategy != miner0Strategy) {
                    // Only switch if no private chain
                    if (!minerGroup.getMiner(0).publishesNextRound()) {
                        GAMEINFO("Block " << currentHeight << ": Attacker switching from " 
                                 << miner0Strategy << " to " << newMiner0Strategy << std::endl);
                        minerGroup.getMiner(0).changeStrategy(*strategyPool[newMiner0Strategy], *blockchain);
                        miner0Strategy = newMiner0Strategy;
                        minerGroup.resetOrder();
                    } else {
                        GAMEINFO("Block " << currentHeight << ": Attacker deferring switch (has private chain)" << std::endl);
                    }
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
            
            GAMEINFO("Game complete: Attacker profit fraction=" << profitFraction 
                     << ", block fraction=" << blockFraction << std::endl);
            
            output << initialGamma << ", " << profitFraction << ", " << hashVal << ", " 
                   << (1.0 - hashVal) << ", " << blockFraction << std::endl;
        }
    }
    
    output.close();
    std::cout << "\n=== B5 Gamma Simulation Complete ===" << std::endl;
    std::cout << "Results written to: " << outputFile << std::endl;
    std::cout << "Total games run: " << (16 * numberOfGames) << std::endl;
    
    GAMEINFO("All B5 Gamma games complete." << std::endl);
    
    return 0;
}
