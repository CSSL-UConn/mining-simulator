//
//  clevel_selfish_miner.hpp
//  BlockSim
//
//  Created by Harry Kalodner on 6/6/16.
//  Copyright © 2016 Harry Kalodner. All rights reserved.
//

#ifndef clever_publishN_miner_hpp
#define clever_publishN_miner_hpp

#include "publish_n_miner.hpp"

class CleverPublishNPublishingStyle : public PublishNPublishingStyle {
private:
    const Value cutoff;
    BlockHeight heightToPublish(const Blockchain &blockchain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const override;
public:
    CleverPublishNPublishingStyle(Value cutoff, Value buildLimit);
};

std::unique_ptr<Strategy> createCleverPublishNStrategy(bool noiseInTransactions, Value cutoff, Value buildLimit);

#endif /* clevel_selfish_miner_hpp */
