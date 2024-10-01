//
//  clever_stubborn_lead_trail_fork_miner.hpp
//  BlockSim
//
//  Created by Harry Kalodner on 6/6/16.
//  Copyright © 2016 Harry Kalodner. All rights reserved.
//

#ifndef clever_stubborn_lead_trail_fork_miner_hpp
#define clever_stubborn_lead_trail_fork_miner_hpp

#include "stubborn_lead_trail_fork_miner.hpp"

class CleverStubbornLeadTrailForkPublishingStyle : public StubbornLeadTrailForkPublishingStyle {
private:
    const Value cutoff;
    BlockHeight heightToPublish(const Blockchain &blockchain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const override;
public:
    CleverStubbornLeadTrailForkPublishingStyle(Value cutoff);
};

std::unique_ptr<Strategy> createCleverStubbornLeadTrailForkStrategy(bool noiseInTransactions, Value cutoff, Value trailCutoff);

#endif /* clever_stubborn_lead_trail_fork_miner_hpp */
