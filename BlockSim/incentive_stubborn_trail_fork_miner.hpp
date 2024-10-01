//
//  incentive_stubborn_lead_trail_fork_miner.hpp
//  BlockSim

#ifndef incentive_stubborn_trail_fork_miner_hpp
#define incentive_stubborn_trail_fork_miner_hpp

#include "publishing_strategy.hpp"

#include <memory>

class Block;
class Strategy;

class IncentiveStubbornTrailForkPublishingStyle : public PublishingStrategy {
public:
    IncentiveStubbornTrailForkPublishingStyle(double incentiveFraction, Value forksToIncentivize);
private:
    const double incentiveFraction;
    const Value forksToIncentivize;
    std::vector<std::unique_ptr<Block>> publishBlocks(const Blockchain &blockchain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) override;
    bool withholdsBlocks() const override { return true; }
    
    BlockHeight getPrivateHeadHeight(std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const;
    
protected:
    virtual BlockHeight heightToPublish(const Blockchain &blockchain, const Miner &me, std::vector<std::unique_ptr<Block>> &unpublishedBlocks) const;
};


std::unique_ptr<Strategy> createIncentiveStubbornTrailForkStrategy(bool noiseInTransactions, Value trailCutoff,double incentiveFraction, Value forksToIncentivize);
Block &incentiveStubbornTrailForkBlockToMineOn(const Miner &me, const Blockchain &blockchain, Value trailCutoff);

#endif /* incentive_stubborn_trail_fork_miner_hpp */
