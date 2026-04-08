//
//  strategy_scheduler.hpp
//  BlockSim
//
//  Strategy scheduler for duration-based strategy changes
//

#ifndef strategy_scheduler_hpp
#define strategy_scheduler_hpp

#include "typeDefs.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

class Strategy;

struct StrategyChange {
    unsigned int minerId;
    BlockHeight startBlock;
    BlockHeight endBlock;
    std::string strategyName;
    double gamma;
    
    StrategyChange(unsigned int id, BlockHeight start, BlockHeight end, std::string name, double connectivity)
        : minerId(id), startBlock(start), endBlock(end), strategyName(name), gamma(connectivity) {}
    
    // Check if this change contains a specific block height
    bool contains(BlockHeight height) const {
        return rawHeight(height) >= rawHeight(startBlock) && rawHeight(height) <= rawHeight(endBlock);
    }
    
    // Check if this change overlaps with another change
    bool overlaps(const StrategyChange& other) const {
        if (minerId != other.minerId) return false;
        return !(rawHeight(endBlock) < rawHeight(other.startBlock) || 
                 rawHeight(startBlock) > rawHeight(other.endBlock));
    }
};

struct FeeBreakpoint {
    BlockHeight BlockHeight;
    double multiplier;
};
class StrategyScheduler {
private:
    std::vector<StrategyChange> schedule;
    std::map<unsigned int, std::vector<StrategyChange>> minerSchedules;  // Indexed by miner
    std::vector<FeeBreakpoint> feeSchedule;
    
public:
    StrategyScheduler();
    
    // Load schedule from file (new format: miner_id, start_block, end_block, strategy_name)
    bool loadFromFile(const std::string& filename);
    double getCurrentFeeMultiplier(BlockHeight height) const;

    bool noisyTransaction = false;
    bool whaleEnabled = false;
    double whaleProb = 0.05;
    double whaleMultiplier = 3.0;
    
    
    // Get all strategy changes that should occur at this block height
    std::vector<StrategyChange> getChangesAtHeight(BlockHeight height) const;
    
    double getConnectivityAtHeight(BlockHeight height) const;

    // Get the active strategy for a miner at a given height
    std::string getActiveStrategy(unsigned int minerId, BlockHeight height) const;
    
    // Validate the entire schedule
    bool validate(const std::vector<unsigned int>& validMinerIds,
                  const std::vector<std::string>& validStrategyNames,
                  std::string& errorMessage) const;
    
    // Reset scheduler
    void reset();
    
    // Check if scheduler has instructions for a miner
    bool hasMiner(unsigned int minerId) const;
    
    // Get total number of strategy changes
    size_t getScheduleSize() const { return schedule.size(); }
};

#endif /* strategy_scheduler_hpp */
