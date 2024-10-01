//
//  clever_stubborn_trail_miner.hpp
//  BlockSim
//
//  Created by Harry Kalodner on 6/6/16.
//  Copyright © 2016 Harry Kalodner. All rights reserved.
//

#ifndef clever_stubborn_trail_miner_hpp
#define clever_stubborn_trail_miner_hpp

#include "stubborn_trail_miner.hpp"

class CleverStubbornTrailPublishingStyle : public StubbornTrailPublishingStyle {
private:
    const Value cutoff;
    BlockHeight heightToPublish(const Blockchain &blockchain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const override;
public:
    CleverStubbornTrailPublishingStyle(Value cutoff);
};

std::unique_ptr<Strategy> createCleverStubbornTrailStrategy(bool noiseInTransactions, Value cutoff, Value trailCutoff);

#endif /* clever_stubborn_trail_miner_hpp */
