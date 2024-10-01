//
//  rational_miner.hpp
//  BlockSim
//
//  Created by Harry Kalodner on 5/25/16.
//  Copyright © 2016 Harry Kalodner. All rights reserved.
//

#ifndef rational_miner_hpp
#define rational_miner_hpp

#include <memory>

class Strategy;

std::unique_ptr<Strategy> createRationalStrategy(bool noiseInTransactions);

#endif /* rational_miner_hpp */
