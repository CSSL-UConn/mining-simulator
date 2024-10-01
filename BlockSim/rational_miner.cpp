//
//  rational_miner.cpp
//  BlockSim
//
//  Created by Harry Kalodner on 5/25/16.
//  Copyright © 2016 Harry Kalodner. All rights reserved.
//

#include "rational_miner.hpp"
#include "block.hpp"
#include "blockchain.hpp"
#include "miner.hpp"
#include "default_miner.hpp"
#include "publishing_strategy.hpp"
#include "strategy.hpp"

#include <cassert>

using std::placeholders::_1;
using std::placeholders::_2;

Block &RationalBlockToMineOn(const Miner &me, const Blockchain &chain);

std::unique_ptr<Strategy> createRationalStrategy(bool noiseInTransactions) {

    ParentSelectorFunc mineFunc;
    
   
    mineFunc = RationalBlockToMineOn;
    
    auto valueFunc = std::bind(defaultValueInMinedChild, _1, _2, noiseInTransactions);
    
    return std::make_unique<Strategy>("rational", mineFunc, valueFunc);
}




Block &RationalBlockToMineOn(const Miner &me, const Blockchain &chain) {
    
    std::vector<Block*> possiblities = chain.blocksAtHeight(chain.getMaxHeightPub());
    if (possiblities.size() == 1) { //no forking
        return *possiblities[0];
    }
    else  { //fork between the selfish miner and the rest of the network
        //mineHere should already be set to the side of the fork not the selfish miner
        Block *mostRemBlock = possiblities[0];

        bool ownABlock = false;
        Block *blockOwned = nullptr;
        for(int i = 0; i < possiblities.size(); i++) {

            if (ownBlock(&me, possiblities[i])) {
                ownABlock = true;
                blockOwned = possiblities[i];
            } 

            if (chain.rem(*possiblities[i]) > chain.rem(*mostRemBlock)) {
                mostRemBlock = possiblities[i];
            }
        }

        if (ownABlock) {
            if (chain.rem(*blockOwned) + blockOwned->value > chain.rem(*mostRemBlock)) {
                return *blockOwned;
            }
        }
        return *mostRemBlock;
    }
}


