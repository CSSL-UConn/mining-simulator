//
//  selfish_miner.hpp
//  BlockSim
//
//  Created by Harry Kalodner on 5/25/16.
//  Copyright © 2016 Harry Kalodner. All rights reserved.
//

#ifndef publish_n_miner_hpp
#define publish_n_miner_hpp

#include "publishing_strategy.hpp"

#include <memory>

class Block;
class Strategy;

class PublishNPublishingStyle : public PublishingStrategy {
private:
    const Value buildLimit; 
    std::vector<std::unique_ptr<Block>> publishBlocks(const Blockchain &blockchain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) override;
    bool withholdsBlocks() const override { return true; }
    
    BlockHeight getPrivateHeadHeight(std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const;
public:
    PublishNPublishingStyle(Value buildLimit); 
protected:
    virtual BlockHeight heightToPublish(const Blockchain &blockchain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const;
};

std::unique_ptr<Strategy> createPublishNStrategy(bool noiseInTransactions, Value buildLimit);
Block &PublishNBlockToMineOn(const Miner &me, const Blockchain &blockchain);

#endif /* publish_n_miner_hpp */
