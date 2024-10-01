//
//  stubborn_fork_miner.hpp
//  BlockSim


#ifndef stubborn_fork_miner_hpp
#define stubborn_fork_miner_hpp

#include "publishing_strategy.hpp"

#include <memory>

class Block;
class Strategy;

class StubbornForkPublishingStyle : public PublishingStrategy {
private:
    std::vector<std::unique_ptr<Block>> publishBlocks(const Blockchain &blockchain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) override;
    bool withholdsBlocks() const override { return true; }
    
    BlockHeight getPrivateHeadHeight(std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const;
    
protected:
    virtual BlockHeight heightToPublish(const Blockchain &blockchain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const;
};

std::unique_ptr<Strategy> createStubbornForkStrategy(bool noiseInTransactions);
Block &stubbornForkBlockToMineOn(const Miner &me, const Blockchain &blockchain);

#endif /* stubborn_fork_miner_hpp */