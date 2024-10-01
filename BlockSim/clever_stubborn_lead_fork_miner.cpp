//
//  clever_stubborn_lead_fork_miner.cpp
//  BlockSim
//
//  Created by Harry Kalodner on 6/6/16.
//  Copyright © 2016 Harry Kalodner. All rights reserved.
//

#include "clever_stubborn_lead_fork_miner.hpp"

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

std::unique_ptr<Strategy> createCleverStubbornLeadForkStrategy(bool noiseInTransactions, Value cutoff) {
    auto valueFunc = std::bind(defaultValueInMinedChild, _1, _2, noiseInTransactions);
    
    return std::make_unique<Strategy>("clever-stubborn_lead_fork", stubbornLeadForkBlockToMineOn, valueFunc, std::make_unique<CleverStubbornLeadForkPublishingStyle>(cutoff));
}

CleverStubbornLeadForkPublishingStyle::CleverStubbornLeadForkPublishingStyle(Value cutoff_) : StubbornLeadForkPublishingStyle(), cutoff(cutoff_) {}

BlockHeight CleverStubbornLeadForkPublishingStyle::heightToPublish(const Blockchain &blockchain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const {
    assert(unpublishedBlocks.back());
    if(unpublishedBlocks.back()->height == blockchain.getMaxHeightPub() + BlockHeight(1) &&
       unpublishedBlocks.back()->value >= Value(cutoff * blockchain.expectedBlockSize() ) &&
       unpublishedBlocks.size() == 1) {
        //finding a block. Normally hide (point of selfish mining) Might decide to normal mine if big block
        //(not worth the risk of using it to selfish mine)
        COMMENTARY("Miner " << me.params.name << " publishes selfish chain. Too large a block to selfishly mine." << std::endl);
        return unpublishedBlocks.back()->height;
    } else {
        return StubbornLeadForkPublishingStyle::heightToPublish(blockchain, me, unpublishedBlocks);
    }
}
