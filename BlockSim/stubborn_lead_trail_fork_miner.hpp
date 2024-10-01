//
//  stubborn_lead_trail_fork_miner.hpp
//  BlockSim


#ifndef stubborn_lead_trail_fork_miner_hpp
#define stubborn_lead_trail_fork_miner_hpp

#include "publishing_strategy.hpp"

#include <memory>

class Block;
class Strategy;

class StubbornLeadTrailForkPublishingStyle : public PublishingStrategy {
private:
    std::vector<std::unique_ptr<Block>> publishBlocks(const Blockchain &blockchain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) override;
    bool withholdsBlocks() const override { return true; }
    
    BlockHeight getPrivateHeadHeight(std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const;
    
protected:
    virtual BlockHeight heightToPublish(const Blockchain &blockchain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const;
};

std::unique_ptr<Strategy> createStubbornLeadTrailForkStrategy(bool noiseInTransactions, Value trailCutoff);
Block &stubbornLeadTrailForkBlockToMineOn(const Miner &me, const Blockchain &blockchain, Value trailCutoff);

#endif /* stubborn_lead_trail_fork_miner_hpp */