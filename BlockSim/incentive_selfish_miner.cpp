//
//  incentive_selfish_miner.cpp
//  BlockSim


#include "incentive_selfish_miner.hpp"
#include "block.hpp"
#include "blockchain.hpp"
#include "logging.h"
#include "miner.hpp"
#include "utils.hpp"
#include "default_miner.hpp"
#include "strategy.hpp"

#include <cmath>
#include <iostream>
#include <assert.h>
#include <algorithm>

using std::placeholders::_1;
using std::placeholders::_2;

std::unique_ptr<Strategy> createIncentiveSelfishStrategy(bool noiseInTransactions, double incentiveFraction, Value forksToIncentivize) {
    auto valueFunc = std::bind(defaultValueInMinedChild, _1, _2, noiseInTransactions);
    
    return std::make_unique<Strategy>("incentive-selfish", incentiveSelfishBlockToMineOn, valueFunc, std::make_unique<IncentiveSelfishPublishingStyle>(incentiveFraction, forksToIncentivize));
}

Block &incentiveSelfishBlockToMineOn(const Miner &me, const Blockchain &chain) {
    Block *newest = me.newestUnpublishedBlock();
    if (newest && newest->height >= chain.getMaxHeightPub()) {
        return *newest;
    } else {
        return chain.oldest(chain.getMaxHeightPub(), me);
    }
}

std::vector<std::unique_ptr<Block>> IncentiveSelfishPublishingStyle::publishBlocks(const Blockchain &chain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) {
    assert(!unpublishedBlocks.empty());
    BlockHeight height = heightToPublish(chain, me, unpublishedBlocks);
    
    // COMMENTARY("Values to publish");
    // COMMENTARY(unpublishedBlocks.front()->value << "\n");
    // COMMENTARY(unpublishedBlocks.front()->valueInChain<< "\n");
    // COMMENTARY(unpublishedBlocks.front()->txFeesInChain << "\n");
    std::vector<BlockHeight> heights;
    heights.resize(unpublishedBlocks.size());
    std::transform(begin(unpublishedBlocks), end(unpublishedBlocks), heights.begin(), [&](auto &block) { return block->height; });
    auto splitPoint = std::upper_bound(std::begin(heights), std::end(heights), height, [](auto first, auto second) { return first < second; });
    auto offset = std::distance(std::begin(heights), splitPoint);
    std::vector<std::unique_ptr<Block>> split_lo(std::make_move_iterator(std::begin(unpublishedBlocks)), std::make_move_iterator(std::begin(unpublishedBlocks) + offset));

    unpublishedBlocks.erase(begin(unpublishedBlocks), std::begin(unpublishedBlocks) + offset);
    
    return split_lo;
}

BlockHeight IncentiveSelfishPublishingStyle::getPrivateHeadHeight(std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const {
    return unpublishedBlocks.back()->height;
}

IncentiveSelfishPublishingStyle::IncentiveSelfishPublishingStyle(double incentive_, Value forksToIncentivize_) : PublishingStrategy(), incentiveFraction(incentive_), forksToIncentivize(forksToIncentivize_) {}

BlockHeight IncentiveSelfishPublishingStyle::heightToPublish(const Blockchain &chain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const {
    assert(!unpublishedBlocks.empty());
    auto privateHeight = getPrivateHeadHeight(unpublishedBlocks);
    auto publicHeight = chain.getMaxHeightPub();
    BlockHeight heightToPublish(publicHeight);
    // If private chain is one block ahead of the public chain and there is a race for the public head then publish


bool forkCameFirst = true;
    std::vector<Block*> possiblities = chain.oldestBlocks(chain.getMaxHeightPub());
     if (possiblities.size() >= 2 & privateHeight == publicHeight + 1) { //fork between the selfish miner and the rest of the network
            
            for(auto i = 0; i < possiblities.size(); i++) {
                if (unpublishedBlocks.front()->timeMined < possiblities[i]->timeMined) {
                    forkCameFirst = false;
                } 
            }
        }
     if (forkCameFirst && privateHeight == publicHeight + 1 && chain.blocksOfHeight(publicHeight) > BlockCount(1) ) {
        unpublishedBlocks[0]->value -= unpublishedBlocks[0]->parent->tip;
        unpublishedBlocks[0]->valueInChain -= unpublishedBlocks[0]->parent->tip;
        unpublishedBlocks[0]->txFeesInChain -= unpublishedBlocks[0]->parent->tip;
    } 

    if (privateHeight == publicHeight + BlockHeight(1) && chain.blocksOfHeight(publicHeight) > BlockCount(1)) {
        heightToPublish = privateHeight;
    }

    if (privateHeight == publicHeight) {
        unpublishedBlocks[0]->tip +=  Value(incentiveFraction * chain.expectedBlockSize());
    }

    return heightToPublish;
}

Value IncentiveValueInMinedChild(const Blockchain &chain, const Block &mineHere, bool noiseInTransactions) {
    auto minVal = mineHere.nextBlockReward();

    auto maxVal = chain.rem(mineHere) + mineHere.nextBlockReward() + mineHere.tip;
    //this represents some noise-- no noise, value would = valueMax
    //value = ((valueMax - valueMin)*((dis(gen)+.7)/1.7)) + valueMin;
    auto value = maxVal;
    if (noiseInTransactions) {
        value = valWithNoise(minVal, maxVal);
    }
    return value;
}