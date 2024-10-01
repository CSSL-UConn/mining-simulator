//
//  incentive_selfish_miner.hpp
//  BlockSim


#ifndef incentive_selfish_miner_hpp
#define incentive_selfish_miner_hpp

#include "publishing_strategy.hpp"

#include <memory>

class Block;
class Strategy;

class IncentiveSelfishPublishingStyle : public PublishingStrategy {
private:
    const double incentiveFraction;
    const Value forksToIncentivize;
    std::vector<std::unique_ptr<Block>> publishBlocks(const Blockchain &blockchain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) override;
    bool withholdsBlocks() const override { return true; }
    
    BlockHeight getPrivateHeadHeight(std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const;
public:
    IncentiveSelfishPublishingStyle(double incentiveFraction, Value forksToIncentivize);
protected:
    virtual BlockHeight heightToPublish(const Blockchain &blockchain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const;
};

std::unique_ptr<Strategy> createIncentiveSelfishStrategy(bool noiseInTransactions, double incentiveFraction, Value forksToIncentivize);
Block &incentiveSelfishBlockToMineOn(const Miner &me, const Blockchain &blockchain);

#endif /* incentive_selfish_miner_hpp */
