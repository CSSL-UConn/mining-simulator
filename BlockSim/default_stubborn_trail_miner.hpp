//
//  default_stubborn_trail_miner.hpp
//  BlockSim
//
//  Created by Harry Kalodner on 6/6/16.
//  Copyright © 2016 Harry Kalodner. All rights reserved.
//

#ifndef default_stubborn_trail_miner_hpp
#define default_stubborn_trail_miner_hpp

#include <memory>

//class Block;
class Strategy;

std::unique_ptr<Strategy> createDefaultStubbornTrailStrategy(bool noiseInTransactions, double gamma);

#endif /* default_stubborn_trail_miner_hpp */
