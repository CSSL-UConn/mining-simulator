//
//  incentive_stubborn_lead_trail_miner.cpp
//  BlockSim


#include "incentive_stubborn_lead_trail_miner.hpp"
#include "block.hpp"
#include "blockchain.hpp"
#include "logging.h"
#include "miner.hpp"
#include "default_miner.hpp"
#include "strategy.hpp"

#include <cmath>
#include <iostream>
#include <assert.h>
#include <algorithm>

using std::placeholders::_1;
using std::placeholders::_2;

std::unique_ptr<Strategy> createIncentiveStubbornLeadTrailStrategy(bool noiseInTransactions, Value trailCutoff, double incentiveFraction, Value forksToIncentivize) {
    auto valueFunc = std::bind(defaultValueInMinedChild, _1, _2, noiseInTransactions);
    
    auto bindedStubbornLeadTrailBlockToMineOn = std::bind(incentiveStubbornLeadTrailBlockToMineOn, std::placeholders::_1, std::placeholders::_2, trailCutoff);

    return std::make_unique<Strategy>("incentive-stubbornLeadTrail", bindedStubbornLeadTrailBlockToMineOn, valueFunc, 
    std::make_unique<IncentiveStubbornLeadTrailPublishingStyle>(incentiveFraction, forksToIncentivize));
}

Block &incentiveStubbornLeadTrailBlockToMineOn(const Miner &me, const Blockchain &chain, Value trailCutoff) {
    Block *newest = me.newestUnpublishedBlock();
    if (newest && newest->height >= chain.getMaxHeightPub()) { 
        return *newest;
    }
    Block *newestPublished = chain.newestBlockByMinerAtHeight(chain.getMaxHeightPub(), me);
    std::vector<Block*> possiblities = chain.oldestBlocks(chain.getMaxHeightPub());

    bool newestPublishedBlockContained = false;
    for(int i = 0; i < possiblities.size(); i++) {
        if (possiblities[i]->parent == newestPublished) {
            newestPublishedBlockContained = true;
        } 
    }
    if (newestPublished) {
        if (chain.getMaxHeightPub() > 1 &&  newestPublished->height >= (chain.getMaxHeightPub() - trailCutoff) && newestPublished->height < (chain.getMaxHeightPub()) && chain.blocksOfHeight(newestPublished->height) > BlockCount(1) && !newestPublishedBlockContained ) {
            return *newestPublished;
        }
            
    } 
    return chain.oldest(chain.getMaxHeightPub(), me);

}

std::vector<std::unique_ptr<Block>> IncentiveStubbornLeadTrailPublishingStyle::publishBlocks(const Blockchain &chain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) {
    assert(!unpublishedBlocks.empty());
    BlockHeight height = heightToPublish(chain, me, unpublishedBlocks);
    
    std::vector<BlockHeight> heights;
    heights.resize(unpublishedBlocks.size());
    std::transform(begin(unpublishedBlocks), end(unpublishedBlocks), heights.begin(), [&](auto &block) { return block->height; });
    auto splitPoint = std::upper_bound(std::begin(heights), std::end(heights), height, [](auto first, auto second) { return first < second; });
    auto offset = std::distance(std::begin(heights), splitPoint);
    std::vector<std::unique_ptr<Block>> split_lo(std::make_move_iterator(std::begin(unpublishedBlocks)), std::make_move_iterator(std::begin(unpublishedBlocks) + offset));

    unpublishedBlocks.erase(begin(unpublishedBlocks), std::begin(unpublishedBlocks) + offset);
    
    return split_lo;
}

BlockHeight IncentiveStubbornLeadTrailPublishingStyle::getPrivateHeadHeight(std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const {
    return unpublishedBlocks.back()->height;
}

IncentiveStubbornLeadTrailPublishingStyle::IncentiveStubbornLeadTrailPublishingStyle(double incentive_, Value forksToIncentivize_) : PublishingStrategy(), incentiveFraction(incentive_), forksToIncentivize(forksToIncentivize_) {}

BlockHeight IncentiveStubbornLeadTrailPublishingStyle::heightToPublish(const Blockchain &chain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const {
    assert(!unpublishedBlocks.empty());
    auto privateHeight = getPrivateHeadHeight(unpublishedBlocks);
    auto publicHeight = chain.getMaxHeightPub();
    BlockHeight heightToPublish(publicHeight);

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
        heightToPublish = privateHeight;
    } 

    if (privateHeight == publicHeight) {
        unpublishedBlocks[0]->tip +=  Value(incentiveFraction * chain.expectedBlockSize());
    }

    return heightToPublish;
}
