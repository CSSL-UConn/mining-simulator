//
//  strategy_factory.cpp
//  BlockSim
//

#include "strategy_factory.hpp"
#include "strategy.hpp"
#include "minerStrategies.h"
#include "logging.h"
#include <iostream>
#include <algorithm>

std::unique_ptr<Strategy> createStrategyByName(const std::string& name, 
                                                bool atomic, 
                                                bool noiseInTransactions,
                                                double gamma,
                                                bool whaleEnabled,
                                                double whaleProb,
                                                double whaleMultiplier)  {
    // Convert to lowercase for case-insensitive comparison
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    // Default/Honest mining
    if (lowerName == "default" || lowerName == "honest") {
        return createDefaultStrategy(atomic, noiseInTransactions,whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    // Selfish mining variants
    if (lowerName == "selfish") {
        return createSelfishStrategy(noiseInTransactions,whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    if (lowerName == "default-selfish") {
        return createDefaultSelfishStrategy(noiseInTransactions, gamma, whaleEnabled, whaleProb, whaleMultiplier);
    }
   
    if (lowerName == "default-stubborn") {
        return createDefaultStubbornTrailStrategy(noiseInTransactions, gamma, whaleEnabled, whaleProb, whaleMultiplier);
    }

    if (lowerName == "clever-selfish") {
        return createCleverSelfishStrategy(noiseInTransactions, UNDERCUT_VALUE, whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    // Stubborn mining variants
    if (lowerName == "stubborn-trail" || lowerName == "stubborn-trail-1") {
        return createStubbornTrailStrategy(noiseInTransactions, 1, whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    if (lowerName == "stubborn-trail-2") {
        return createStubbornTrailStrategy(noiseInTransactions, 2, whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    if (lowerName == "stubborn-trail-3") {
        return createStubbornTrailStrategy(noiseInTransactions, 3, whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    if (lowerName == "stubborn-trail-4") {
        return createStubbornTrailStrategy(noiseInTransactions, 4, whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    if (lowerName == "stubborn-fork") {
        return createStubbornForkStrategy(noiseInTransactions, whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    if (lowerName == "stubborn-lead") {
        return createStubbornLeadStrategy(noiseInTransactions, whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    if (lowerName == "stubborn-lead-fork") {
        return createStubbornLeadForkStrategy(noiseInTransactions,whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    if (lowerName == "stubborn-lead-trail") {
        return createStubbornLeadTrailStrategy(noiseInTransactions, 1,whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    if (lowerName == "stubborn-trail-fork") {
        return createStubbornTrailForkStrategy(noiseInTransactions, 1, whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    if (lowerName == "stubborn-lead-trail-fork") {
        return createStubbornLeadTrailForkStrategy(noiseInTransactions, 1, whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    // Other strategies
    if (lowerName == "petty") {
        return createPettyStrategy(atomic, noiseInTransactions, whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    if (lowerName == "lazy-fork") {
        return createLazyForkStrategy(atomic);
    }
    
    if (lowerName == "gap") {
        return createGapStrategy(atomic, noiseInTransactions,whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    if (lowerName == "rational") {
        return createRationalStrategy(noiseInTransactions,whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    // Publish N strategies
    if (lowerName == "publish-3" || lowerName == "publish3") {
        return createPublishNStrategy(noiseInTransactions, 3, whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    if (lowerName == "publish-4" || lowerName == "publish4") {
        return createPublishNStrategy(noiseInTransactions, 4, whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    // If no match found, return default strategy
    std::cerr << "Warning: Unknown strategy name '" << name 
              << "', using default strategy instead" << std::endl;
    return createDefaultStrategy(atomic, noiseInTransactions, whaleEnabled, whaleProb, whaleMultiplier);
}
