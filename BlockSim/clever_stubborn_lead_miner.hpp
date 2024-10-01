//
//  clevel_selfish_miner.hpp
//  BlockSim
//
//  Created by Harry Kalodner on 6/6/16.
//  Copyright © 2016 Harry Kalodner. All rights reserved.
//

#ifndef clever_stubborn_lead_miner_hpp
#define clever_stubborn_lead_miner_hpp

#include "stubborn_lead_miner.hpp"

class CleverStubbornLeadPublishingStyle : public StubbornLeadPublishingStyle {
private:
    const Value cutoff;
    BlockHeight heightToPublish(const Blockchain &blockchain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const override;
public:
    CleverStubbornLeadPublishingStyle(Value cutoff);
};

std::unique_ptr<Strategy> createCleverStubbornLeadStrategy(bool noiseInTransactions, Value cutoff);

#endif /* clevel_stubborn_lead_miner_hpp */
