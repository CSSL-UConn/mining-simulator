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
#include <regex>

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
    
    // Incentivized strategies - parse with regex
    // Formats: incentive-trail-<k>-<f>, incentive-trail-<k>-lead-<f>, 
    //          incentive-trail-<k>-fork-<f>, incentive-trail-<k>-lead-fork-<f>,
    //          incentive-selfish-<f>
    // Where k = trail cutoff (1 or 2), f = incentive fraction (e.g., 2.0)
    
    // incentive-selfish-<f>
    std::regex selfishIncentiveRegex("^incentive-selfish-([0-9.]+)$");
    std::smatch selfishMatch;
    if (std::regex_match(lowerName, selfishMatch, selfishIncentiveRegex)) {
        double incentiveFraction = std::stod(selfishMatch[1].str());
        return createIncentiveSelfishStrategy(noiseInTransactions, incentiveFraction, 1,
            whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    // incentive-trail-<k>-lead-fork-<f>
    std::regex trailLeadForkRegex("^incentive-trail-([12])-lead-fork-([0-9.]+)$");
    std::smatch trailLeadForkMatch;
    if (std::regex_match(lowerName, trailLeadForkMatch, trailLeadForkRegex)) {
        int trailCutoff = std::stoi(trailLeadForkMatch[1].str());
        double incentiveFraction = std::stod(trailLeadForkMatch[2].str());
        return createIncentiveStubbornLeadTrailForkStrategy(noiseInTransactions, trailCutoff, 
            incentiveFraction, 1, whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    // incentive-trail-<k>-lead-<f>
    std::regex trailLeadRegex("^incentive-trail-([12])-lead-([0-9.]+)$");
    std::smatch trailLeadMatch;
    if (std::regex_match(lowerName, trailLeadMatch, trailLeadRegex)) {
        int trailCutoff = std::stoi(trailLeadMatch[1].str());
        double incentiveFraction = std::stod(trailLeadMatch[2].str());
        return createIncentiveStubbornLeadTrailStrategy(noiseInTransactions, trailCutoff, 
            incentiveFraction, 1, whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    // incentive-trail-<k>-fork-<f>
    std::regex trailForkRegex("^incentive-trail-([12])-fork-([0-9.]+)$");
    std::smatch trailForkMatch;
    if (std::regex_match(lowerName, trailForkMatch, trailForkRegex)) {
        int trailCutoff = std::stoi(trailForkMatch[1].str());
        double incentiveFraction = std::stod(trailForkMatch[2].str());
        return createIncentiveStubbornTrailForkStrategy(noiseInTransactions, trailCutoff, 
            incentiveFraction, 1, whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    // incentive-trail-<k>-<f> (basic trail)
    std::regex trailRegex("^incentive-trail-([12])-([0-9.]+)$");
    std::smatch trailMatch;
    if (std::regex_match(lowerName, trailMatch, trailRegex)) {
        int trailCutoff = std::stoi(trailMatch[1].str());
        double incentiveFraction = std::stod(trailMatch[2].str());
        return createIncentiveStubbornTrailStrategy(noiseInTransactions, trailCutoff, 
            incentiveFraction, 1, whaleEnabled, whaleProb, whaleMultiplier);
    }
    
    // If no match found, return default strategy
    std::cerr << "Warning: Unknown strategy name '" << name 
              << "', using default strategy instead" << std::endl;
    return createDefaultStrategy(atomic, noiseInTransactions, whaleEnabled, whaleProb, whaleMultiplier);
}
