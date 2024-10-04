//
//  clevel_publishN_miner.cpp
//  BlockSim
//  Copyright © 2016 Harry Kalodner. All rights reserved.
//

#include "clever_publishN_miner.hpp"

#include "block.hpp"
#include "blockchain.hpp"
#include "logging.h"
#include "minerParameters.h"
#include "miner.hpp"
#include "default_miner.hpp"
#include "strategy.hpp"

#include <assert.h>
#include <iostream>

using std::placeholders::_1;
using std::placeholders::_2;

std::unique_ptr<Strategy> createCleverPublishNStrategy(bool noiseInTransactions, Value cutoff, Value buildLimit) {
    auto valueFunc = std::bind(defaultValueInMinedChild, _1, _2, noiseInTransactions);
    
    return std::make_unique<Strategy>("clever-publishN", PublishNBlockToMineOn, valueFunc, std::make_unique<CleverPublishNPublishingStyle>(cutoff, buildLimit));
}

CleverPublishNPublishingStyle::CleverPublishNPublishingStyle(Value cutoff_, Value buildLimit_) : PublishNPublishingStyle(buildLimit_), cutoff(cutoff_) {}

BlockHeight CleverPublishNPublishingStyle::heightToPublish(const Blockchain &blockchain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const {
    assert(unpublishedBlocks.back());
    if(unpublishedBlocks.back()->height == blockchain.getMaxHeightPub() + BlockHeight(1) &&
       unpublishedBlocks.back()->value >= Value(cutoff * blockchain.expectedBlockSize() ) &&
       unpublishedBlocks.size() == 1) {
        //finding a block. Normally hide (point of selfish mining) Might decide to normal mine if big block
        //(not worth the risk of using it to selfish mine)
        COMMENTARY("Miner " << me.params.name << " publishes selfish chain. Too large a block to selfishly mine." << std::endl);
        return unpublishedBlocks.back()->height;
    } else {
        return PublishNPublishingStyle::heightToPublish(blockchain, me, unpublishedBlocks);
    }
}
