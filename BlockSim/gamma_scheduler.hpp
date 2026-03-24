//
//  gamma_scheduler.hpp
//  BlockSim
//
//  Gamma (connectivity) scheduler for varying network conditions mid-game
//

#ifndef gamma_scheduler_hpp
#define gamma_scheduler_hpp

#include "typeDefs.hpp"
#include <string>
#include <vector>

struct GammaChange {
    BlockHeight startBlock;
    BlockHeight endBlock;
    double gamma;
    
    GammaChange(BlockHeight start, BlockHeight end, double g)
        : startBlock(start), endBlock(end), gamma(g) {}
    
    bool contains(BlockHeight height) const {
        return rawHeight(height) >= rawHeight(startBlock) && rawHeight(height) <= rawHeight(endBlock);
    }
};

class GammaScheduler {
private:
    std::vector<GammaChange> schedule;
    
public:
    GammaScheduler();
    
    // Load schedule from file (format: start_block, end_block, gamma)
    bool loadFromFile(const std::string& filename);
    
    // Get the active gamma value at a given height
    double getActiveGamma(BlockHeight height) const;
    
    // Reset scheduler
    void reset();
    
    // Get total number of gamma changes
    size_t getScheduleSize() const { return schedule.size(); }
};

#endif /* gamma_scheduler_hpp */
