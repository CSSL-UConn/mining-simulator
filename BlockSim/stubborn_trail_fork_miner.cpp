//
//  stubborn_trail_fork_miner.cpp
//  BlockSim


#include "stubborn_trail_fork_miner.hpp"
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

std::unique_ptr<Strategy> createStubbornTrailForkStrategy(bool noiseInTransactions, Value trailCutoff) {
    auto valueFunc = std::bind(defaultValueInMinedChild, _1, _2, noiseInTransactions);
    
    auto bindedStubbornTrailForkBlockToMineOn = std::bind(stubbornTrailForkBlockToMineOn, std::placeholders::_1, std::placeholders::_2, trailCutoff);

    return std::make_unique<Strategy>("stubbornTrailFork", bindedStubbornTrailForkBlockToMineOn, valueFunc, std::make_unique<StubbornTrailForkPublishingStyle>());
}

Block &stubbornTrailForkBlockToMineOn(const Miner &me, const Blockchain &chain, Value trailCutoff) {
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
            COMMENTARY("OLDER");
            return *newestPublished;
        }
            
    } 
    return chain.oldest(chain.getMaxHeightPub(), me);
}

std::vector<std::unique_ptr<Block>> StubbornTrailForkPublishingStyle::publishBlocks(const Blockchain &chain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) {
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

BlockHeight StubbornTrailForkPublishingStyle::getPrivateHeadHeight(std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const {
    return unpublishedBlocks.back()->height;
}

BlockHeight StubbornTrailForkPublishingStyle::heightToPublish(const Blockchain &chain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const {
    assert(!unpublishedBlocks.empty());
    auto privateHeight = getPrivateHeadHeight(unpublishedBlocks);
    auto publicHeight = chain.getMaxHeightPub();
    BlockHeight heightToPublish(publicHeight);
    bool forkCameFirst = true;

    if ( chain.blocksOfHeight(publicHeight) > 1 && privateHeight == publicHeight + 1) {

        std::vector<Block*> possiblities = chain.oldestBlocks(chain.getMaxHeightPub());
         for(auto i = 0; i < possiblities.size(); i++) {
             if (unpublishedBlocks.front()->timeMined < possiblities[i]->timeMined) {
                forkCameFirst = false;
             } 
         }
        
        std::vector<Block*> possiblities2 = chain.blocksAtHeight(publicHeight);
        if (possiblities2.size() == 2) { //fork between the selfish miner and the rest of the network
            //mineHere should already be set to the side of the fork not the selfish miner
            Block *selfishBlock = nullptr;
            Block *defaultBlock = nullptr;
            if (ownBlock(&me, possiblities2[0])) {
                defaultBlock = possiblities2[1];
                selfishBlock = possiblities2[0];
            } else {
                defaultBlock = possiblities2[0];
                selfishBlock = possiblities2[1];
            }
            // handles the trailing case of stubborn-trail
            if (defaultBlock->timeMined < selfishBlock->timeMined) {
                return privateHeight;
            } 
        } else if (possiblities2.size() == 3){ // we must choose between multiple selfish miners, and honest miner
                    //mineHere should already be set to the side of the fork not the selfish miner
            Block *Selfish1Block = nullptr;
            Block *default2Block = nullptr;
            Block *defaultBlock = nullptr;
            //determine which two blocks are running a selfish mining strategy
            if (ownBlock(&me, possiblities2[0])) {
                defaultBlock = possiblities2[1];
                default2Block = possiblities2[2];
                Selfish1Block = possiblities2[0];
            } else if (ownBlock(&me, possiblities2[1])){
                defaultBlock = possiblities2[0];
                default2Block = possiblities2[2];
                Selfish1Block = possiblities2[1]; 
            } else {
                defaultBlock = possiblities2[0];
                Selfish1Block = possiblities2[2]; 
                default2Block = possiblities2[1]; 
            }
        
            // check if either of the blocks of the fork are coming from a leading position (don't mine on them)
            if (defaultBlock->timeMined < Selfish1Block->timeMined || default2Block->timeMined < Selfish1Block->timeMined) {
            return privateHeight;
            }

        } }

    if (forkCameFirst && privateHeight == publicHeight + 1 && chain.blocksOfHeight(publicHeight) > BlockCount(1) ) {
        return heightToPublish; // publish after fork
        
    }  else if (privateHeight == publicHeight + BlockHeight(1) && chain.blocksOfHeight(publicHeight) > BlockCount(1)) {
        heightToPublish = privateHeight; // 2 ahead
    } else {
        return heightToPublish;
    }

    return heightToPublish;
}
