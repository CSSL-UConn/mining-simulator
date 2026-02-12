//
//  main.cpp
//  ScheduledStratSim
//
//  Simulation with continuous blockchain and dynamic strategy changes
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

int main(int argc, const char *argv[]) {
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <strategy_schedule_file> [output_file]" << std::endl;
        std::cerr << "\nStrategy schedule file format:" << std::endl;
        std::cerr << "  miner_id, start_block, end_block, strategy_name" << std::endl;
        std::cerr << "\nExample:" << std::endl;
        std::cerr << "  0, 0, 19, selfish" << std::endl;
        std::cerr << "  0, 20, 49, stubborn-trail" << std::endl;
        std::cerr << "  1, 0, 99, default" << std::endl;
        return 1;
    }
    
    std::string scheduleFile = argv[1];
    std::string outputFile = argc >= 3 ? argv[2] : "scheduled_output.txt";
    
    // Load strategy schedule
    StrategyScheduler scheduler;
    if (!scheduler.loadFromFile(scheduleFile)) {
        std::cerr << "Failed to load strategy schedule from: " << scheduleFile << std::endl;
        return 1;
    }
    
    std::cout << "Successfully loaded schedule from: " << scheduleFile << std::endl;
    std::cout << "Schedule has " << scheduler.getScheduleSize() << " strategy changes" << std::endl;
    
    GAMEINFO("\n#####\nRunning Scheduled Strategy Simulation (Continuous Chain)\n#####\n" << std::endl);
    
    // Open output file
    std::ofstream output(outputFile);
    if (!output.is_open()) {
        std::cerr << "Error opening output file: " << outputFile << std::endl;
        return 1;
    }
    
    output << "BlockHeight, Miner0_Strategy, Miner0_CumulativeProfit, Miner0_CumulativeBlocks, Miner1_Strategy, Miner1_CumulativeProfit, Miner1_CumulativeBlocks" << std::endl;
    
    GAMEINFO("Starting simulation with scheduled strategy changes..." << std::endl);
    
    // Determine total simulation length
    BlockHeight maxHeight(0);
    for (unsigned int minerId = 0; minerId < 2; minerId++) {
        if (scheduler.hasMiner(minerId)) {
            for (int h = 0; h < 200; h++) {
                std::string strat = scheduler.getActiveStrategy(minerId, BlockHeight(h));
                if (!strat.empty() && h > rawHeight(maxHeight)) {
                    maxHeight = BlockHeight(h);
                }
            }
        }
    }
    
    if (rawHeight(maxHeight) == 0) {
        std::cerr << "No valid schedule found!" << std::endl;
        return 1;
    }
    
    BlockCount totalBlocks = BlockCount(rawHeight(maxHeight) + 1);
    GAMEINFO("Running continuous simulation for " << totalBlocks << " blocks" << std::endl);
    
    // Create strategy pool
    std::map<std::string, std::unique_ptr<Strategy>> strategyPool;
    std::vector<std::string> strategyNames = {
        "default", "selfish", "stubborn-trail", "stubborn-fork", "stubborn-lead",
        "stubborn-lead-fork", "stubborn-trail-fork", "stubborn-lead-trail",
        "stubborn-lead-trail-fork", "petty", "lazy-fork", "gap", "rational",
        "publish-3", "publish-4"
    };
    
    for (const auto& name : strategyNames) {
        strategyPool[name] = createStrategyByName(name, false, NOISE_IN_TRANSACTIONS);
    }
    
    // Setup initial strategies
    std::string miner0Strategy = scheduler.getActiveStrategy(0, BlockHeight(0));
    std::string miner1Strategy = scheduler.getActiveStrategy(1, BlockHeight(0));
    
    std::cout << "Queried initial strategies from scheduler:" << std::endl;
    std::cout << "  Miner 0 at block 0: '" << miner0Strategy << "'" << std::endl;
    std::cout << "  Miner 1 at block 0: '" << miner1Strategy << "'" << std::endl;
    
    if (miner0Strategy.empty()) miner0Strategy = "default";
    if (miner1Strategy.empty()) miner1Strategy = "default";
    
    GAMEINFO("Initial strategies: Miner0=" << miner0Strategy << ", Miner1=" << miner1Strategy << std::endl);
    
    // Setup miners
    HashRate miner0Power = HashRate(0.4);
    HashRate miner1Power = HashRate(0.6);
    
    MinerParameters miner0Params = {0, "Miner-0", miner0Power, NETWORK_DELAY, COST_PER_SEC_TO_MINE};
    MinerParameters miner1Params = {1, "Miner-1", miner1Power, NETWORK_DELAY, COST_PER_SEC_TO_MINE};
    
    std::vector<std::unique_ptr<Miner>> miners;
    miners.push_back(std::make_unique<Miner>(miner0Params, *strategyPool[miner0Strategy]));
    miners.push_back(std::make_unique<Miner>(miner1Params, *strategyPool[miner1Strategy]));
    
    MinerGroup minerGroup(std::move(miners));
    
    // Setup blockchain
    BlockchainSettings blockchainSettings = {SEC_PER_BLOCK, A, B, totalBlocks};
    GameSettings settings = {blockchainSettings};
    auto blockchain = std::make_unique<Blockchain>(settings.blockchainSettings);
    minerGroup.reset(*blockchain);
    minerGroup.resetOrder();
    
    // Track cumulative results
    Value miner0TotalProfit(0);
    Value miner1TotalProfit(0);
    BlockCount miner0TotalBlocks(0);
    BlockCount miner1TotalBlocks(0);
    
    // Run simulation in segments with strategy changes
    BlockHeight currentHeight(0);
    BlockHeight reportInterval(10);
    
    while (rawHeight(currentHeight) < rawCount(totalBlocks)) {
        // Check for strategy changes
        std::string newMiner0Strategy = scheduler.getActiveStrategy(0, currentHeight);
        std::string newMiner1Strategy = scheduler.getActiveStrategy(1, currentHeight);
        
        if (newMiner0Strategy.empty()) newMiner0Strategy = "default";
        if (newMiner1Strategy.empty()) newMiner1Strategy = "default";
        
        // Change strategies if needed
        if (newMiner0Strategy != miner0Strategy) {
            std::cout << ">>> Block " << currentHeight << ": Miner 0 switching from " 
                     << miner0Strategy << " to " << newMiner0Strategy << std::endl;
            GAMEINFO("Block " << currentHeight << ": Miner 0 switching from " 
                     << miner0Strategy << " to " << newMiner0Strategy << std::endl);
            minerGroup.getMiner(0).changeStrategy(*strategyPool[newMiner0Strategy], *blockchain);
            miner0Strategy = newMiner0Strategy;
        }
        
        if (newMiner1Strategy != miner1Strategy) {
            std::cout << ">>> Block " << currentHeight << ": Miner 1 switching from " 
                     << miner1Strategy << " to " << newMiner1Strategy << std::endl;
            GAMEINFO("Block " << currentHeight << ": Miner 1 switching from " 
                     << miner1Strategy << " to " << newMiner1Strategy << std::endl);
            minerGroup.getMiner(1).changeStrategy(*strategyPool[newMiner1Strategy], *blockchain);
            miner1Strategy = newMiner1Strategy;
        }
        
        // Find next strategy change or report point
        BlockHeight nextStrategyChange(rawCount(totalBlocks));
        for (int h = rawHeight(currentHeight) + 1; h < rawCount(totalBlocks); h++) {
            std::string s0 = scheduler.getActiveStrategy(0, BlockHeight(h));
            std::string s1 = scheduler.getActiveStrategy(1, BlockHeight(h));
            if (s0.empty()) s0 = "default";
            if (s1.empty()) s1 = "default";
            
            if (s0 != miner0Strategy || s1 != miner1Strategy) {
                nextStrategyChange = BlockHeight(h);
                break;
            }
        }
        
        BlockHeight nextReport = BlockHeight(rawHeight(currentHeight) + rawHeight(reportInterval));
        BlockHeight runUntil = BlockHeight(std::min(rawHeight(nextReport), rawHeight(nextStrategyChange)));
        runUntil = BlockHeight(std::min(rawHeight(runUntil), rawCount(totalBlocks)));
        
        BlockCount segmentSize = BlockCount(rawHeight(runUntil) - rawHeight(currentHeight));
        
        if (rawCount(segmentSize) > 0) {
            GAMEINFO("Running from block " << currentHeight << " to " << runUntil 
                     << " (M0=" << miner0Strategy << ", M1=" << miner1Strategy << ")" << std::endl);
            
            // Run this segment
            BlockchainSettings segmentSettings = {SEC_PER_BLOCK, A, B, segmentSize};
            GameSettings segSettings = {segmentSettings};
            
            auto result = runGame(minerGroup, *blockchain, segSettings);
            
            // IMPORTANT: After runGame, miners are in a finalized state
            // We need to reset them for the next segment to avoid null block issues
            minerGroup.reset(*blockchain);
            
            // Accumulate results
            miner0TotalProfit = Value(rawValue(miner0TotalProfit) + rawValue(result.minerResults[0].totalProfit));
            miner1TotalProfit = Value(rawValue(miner1TotalProfit) + rawValue(result.minerResults[1].totalProfit));
            miner0TotalBlocks = BlockCount(rawCount(miner0TotalBlocks) + rawCount(result.minerResults[0].blocksInWinningChain));
            miner1TotalBlocks = BlockCount(rawCount(miner1TotalBlocks) + rawCount(result.minerResults[1].blocksInWinningChain));
            
            currentHeight = runUntil;
            
            // Report if at interval
            if (rawHeight(currentHeight) % rawHeight(reportInterval) == 0 || rawHeight(currentHeight) >= rawCount(totalBlocks)) {
                std::cout << "Block " << currentHeight 
                         << " | M0: " << miner0TotalProfit << " profit, " << miner0TotalBlocks << " blocks"
                         << " | M1: " << miner1TotalProfit << " profit, " << miner1TotalBlocks << " blocks" << std::endl;
                
                GAMEINFO("Block " << currentHeight 
                         << " | M0: " << miner0TotalProfit << " profit, " << miner0TotalBlocks << " blocks"
                         << " | M1: " << miner1TotalProfit << " profit, " << miner1TotalBlocks << " blocks" << std::endl);
                
                output << currentHeight << ", "
                       << miner0Strategy << ", " << miner0TotalProfit << ", " << miner0TotalBlocks << ", "
                       << miner1Strategy << ", " << miner1TotalProfit << ", " << miner1TotalBlocks << std::endl;
            }
        }
    }
    
    output.close();
    
    std::cout << "\n=== Simulation Complete ===" << std::endl;
    std::cout << "Results written to: " << outputFile << std::endl;
    std::cout << "Total simulation time: " << totalBlocks << " blocks worth of time" << std::endl;
    std::cout << "Final cumulative results:" << std::endl;
    std::cout << "  Miner 0: " << miner0TotalProfit << " profit, " << miner0TotalBlocks << " blocks in chain" << std::endl;
    std::cout << "  Miner 1: " << miner1TotalProfit << " profit, " << miner1TotalBlocks << " blocks in chain" << std::endl;
    std::cout << "  Total blocks in final chain: " << (rawCount(miner0TotalBlocks) + rawCount(miner1TotalBlocks)) << std::endl;
    
    GAMEINFO("\nResults written to: " << outputFile << std::endl);
    GAMEINFO("Simulation complete!" << std::endl);
    GAMEINFO("Final: M0 earned " << miner0TotalProfit << " (" << miner0TotalBlocks << " blocks), "
             << "M1 earned " << miner1TotalProfit << " (" << miner1TotalBlocks << " blocks)" << std::endl);
    
    return 0;
}
