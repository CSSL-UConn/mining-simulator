//
//  game_ttp.cpp
//  BlockSim

#include "game_ttp.hpp"
#include "blockchain.hpp"
#include "block.hpp"
#include "miner.hpp"
#include "logging.h"
#include "minerGroup.hpp"
#include "miner_result.hpp"
#include "game_result.hpp"
#include "minerStrategies.h"
#include "strategy.hpp"
#include "dap_tracker.hpp"
 
#include <cassert>
#include <iostream>
#include <fstream>

// Time-to-Profitability game loop. 

GameResult runGameTTP(MinerGroup &minerGroup, Blockchain &blockchain, GameSettings gameSettings, DAPTracker &dapTracker) {

    // GAMEINFO("Players:" << std::endl << minerGroup);
 
    BlockTime totalSeconds =
        gameSettings.blockchainSettings.numberOfBlocks
        * gameSettings.blockchainSettings.secondsPerBlock;

    while (blockchain.getTime() < totalSeconds) {
        BlockTime nextTime = minerGroup.nextEventTime(blockchain);

        assert(blockchain.getTime() <= nextTime);
 
        blockchain.advanceToTime(nextTime);
 
        assert(blockchain.getTime() == nextTime);
 
        COMMENTARY("Round " << blockchain.getTime() << " of the game..." << std::endl);

        // Mine, Broadcast, Publish
        minerGroup.nextMineRound(blockchain);
        minerGroup.nextBroadcastRound(blockchain);
        COMMENTARY("Publish phase:" << std::endl);
        minerGroup.nextPublishRound(blockchain);

        dapTracker.checkAndProcessDAP(blockchain, minerGroup);
 
        COMMENTARY("Round " << blockchain.getTime()
                   << " over. Current blockchain:" << std::endl);
        COMMENTARYBLOCK(
            blockchain.printBlockchain();
            blockchain.printHeads();
        )
    }

    minerGroup.finalize(blockchain);
    dapTracker.finalize(blockchain, minerGroup);

    std::vector<MinerResult> minerResults;
    minerResults.resize(minerGroup.miners.size());
 
    auto &winningBlock = blockchain.winningHead();
    auto winningChain  = winningBlock.getChain();
    int parentCount = 0;
    Value totalValue(0);
 
    for (auto mined : winningChain) {
        if (mined->height == BlockHeight(0)) {
            break;
        }
        if (mined->parent->minedBy(mined->miner)) {
            parentCount++;
        }
 
        auto miner = mined->miner;
        size_t minerIndex = minerGroup.miners.size();
        for (size_t ind = 0; ind < minerGroup.miners.size(); ind++) {
            if (minerGroup.miners[ind].get() == miner) {
                minerIndex = ind;
                break;
            }
        }
 
        minerResults[minerIndex].addBlock(mined, minerGroup.miners[minerIndex]->totalMiningCost);
        totalValue += mined->value;
    }
 
    BlockCount totalBlocks(0);
    BlockCount finalBlocks(0);
 
    for (size_t i = 0; i < minerGroup.miners.size(); i++) {
        const auto &miner = minerGroup.miners[i];
        // GAMEINFO(*miner << " earned:" << minerResults[i].totalProfit
        //          << " mined " << miner->getBlocksMinedTotal()
        //          << " total, of which "
        //          << minerResults[i].blocksInWinningChain
        //          << " made it into the final chain" << std::endl);
        totalBlocks += miner->getBlocksMinedTotal();
        finalBlocks += minerResults[i].blocksInWinningChain;
    }
 
    Value moneyLeftAtEnd = blockchain.rem(*winningChain[0]);
 
    GameResult result(minerResults, totalBlocks, finalBlocks,
                      moneyLeftAtEnd, totalValue);
 
    assert(winningBlock.valueInChain == totalValue);
 
    // GAMEINFO("Total blocks mined:" << totalBlocks
    //          << " with " << finalBlocks
    //          << " making it into the final chain" << std::endl);
 
    return result;
}