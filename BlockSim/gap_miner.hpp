//
//  gap_miner.hpp
//  BlockSim
//
//  Created by Harry Kalodner on 5/25/16.
//  Copyright © 2016 Harry Kalodner. All rights reserved.
//

#ifndef gap_miner_hpp
#define gap_miner_hpp

#include <memory>

class Strategy;

std::unique_ptr<Strategy> createGapStrategy(bool atomic, bool noiseInTransactions, bool whaleEnabled, double whaleProb, double whaleMultiplier);

#endif /* gap_miner_hpp */
