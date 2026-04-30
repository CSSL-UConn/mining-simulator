//
//  petty_miner.cpp
//  BlockSim
//
//  Created by Harry Kalodner on 5/25/16.
//  Copyright © 2016 Harry Kalodner. All rights reserved.
//

#include "petty_miner.hpp"
#include "block.hpp"
#include "blockchain.hpp"
#include "miner.hpp"
#include "default_miner.hpp"
#include "publishing_strategy.hpp"
#include "strategy.hpp"

#include <cassert>

using std::placeholders::_1;
using std::placeholders::_2;

Block &blockToMineOnAtomic(const Miner &me, const Blockchain &chain);
Block &blockToMineOnNonAtomic(const Miner &me, const Blockchain &chain);

std::unique_ptr<Strategy> createPettyStrategy(bool atomic, bool noiseInTransactions, bool whaleEnabled, double whaleProb, double whaleMultiplier) {

    ParentSelectorFunc mineFunc;
    
    if (atomic) {
        mineFunc = blockToMineOnAtomic;
    } else {
        mineFunc = blockToMineOnNonAtomic;
    }
    auto valueFunc = std::bind(defaultValueInMinedChild, _1, _2, noiseInTransactions, whaleEnabled, whaleProb, whaleMultiplier);
    
    return std::make_unique<Strategy>("petty-honest", mineFunc, valueFunc);
}

Block &blockToMineOnAtomic(const Miner &me, const Blockchain &chain) {
    return chain.most(chain.getMaxHeightPub(), me);
}

Block &blockToMineOnNonAtomic(const Miner &, const Blockchain &chain) {
    return chain.most(chain.getMaxHeightPub());
}
