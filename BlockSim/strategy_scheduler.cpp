//
//  strategy_scheduler.cpp
//  BlockSim
//

#include "strategy_scheduler.hpp"
#include "logging.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

StrategyScheduler::StrategyScheduler() {}

bool StrategyScheduler::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open strategy schedule file: " << filename << std::endl;
        return false;
    }
    
    schedule.clear();
    minerSchedules.clear();
    feeSchedule.clear();
    
    std::string line;
    int lineNumber = 0;
    
    while (std::getline(file, line)) {
        lineNumber++;
        
        // Trim whitespace from line
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse line: miner_id, start_block, end_block, strategy_name
        std::istringstream iss(line);
        std::string token;
        std::vector<std::string> tokens;
        
        while (std::getline(iss, token, ',')) {
            // Trim whitespace
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);
            tokens.push_back(token);
        }
        
        if(tokens.empty()) continue;

        if(tokens[0] == "FEE_CHANGE") {
            if (tokens.size() < 3) {
                std::cerr << "warning FEE_Change requires block height and multiplier at line" << lineNumber << std::endl;
                continue;
            }
            FeeBreakpoint bp; 
            bp.BlockHeight = BlockHeight(std::stoul(tokens[1]));
            bp.multiplier = std::stod(tokens[2]);
            feeSchedule.push_back(bp);
            std::sort(feeSchedule.begin(), feeSchedule.end(), [](const FeeBreakpoint& a, const FeeBreakpoint& b) {
                    return a.BlockHeight < b.BlockHeight;
                });
            std::cout << "Fee change at block " << rawHeight(bp.BlockHeight) 
                      << ": multiplier=" << bp.multiplier << std::endl;
            continue;
        }
        if (tokens[0] == "NOISY_TRANSACTION") {
            noisyTransaction = (tokens.size() >= 2 && (tokens[1] == "true" || tokens[1] == "1"));
            std::cout << "NOISY_TRANSACTION: " << (noisyTransaction ? "true" : "false") << std::endl;
            continue;
        }
        if (tokens[0] == "WHALE_ENABLED") {
            whaleEnabled = (tokens.size() >= 2 && (tokens[1] == "true" || tokens[1] == "1"));
            std::cout << "WHALE_ENABLED: " << (whaleEnabled ? "true" : "false") << std::endl;
            continue;
        }
        if (tokens[0] == "WHALE_PROB") {
             if (tokens.size() >= 2) {
                whaleProb = std::stod(tokens[1]);
            }
            std::cout << "WHALE_PROB: " << whaleProb << std::endl;
            continue;
        }
        if (tokens[0] == "WHALE_MULTIPLIER") {
            if (tokens.size() >= 2) {
                whaleMultiplier = std::stod(tokens[1]);
            }
            std::cout << "WHALE_MULTIPLIER: " << whaleMultiplier << std::endl;
            continue;
        }
        if (tokens.size() > 5) {
            std::cerr << "Warning: Invalid format at line " << lineNumber 
                      << " (expected: miner_id, start_block, end_block, strategy_name, gamma (optional))" << std::endl;
            continue;
        }
        
        try {
            unsigned int minerId = std::stoi(tokens[0]);
            BlockHeight startBlock = BlockHeight(std::stoi(tokens[1]));
            BlockHeight endBlock = BlockHeight(std::stoi(tokens[2]));
            std::string strategyName = tokens[3];

            //Handle optional gamma
           double gamma = -1.0;
            if (tokens.size() >= 5 && !tokens[4].empty()) {
                gamma = std::stod(tokens[4]);
            }
            
            StrategyChange change(minerId, startBlock, endBlock, strategyName, gamma);
            schedule.push_back(change);
            minerSchedules[minerId].push_back(change);
            
            GAMEINFO("Loaded strategy change: Miner " << minerId 
                     << " will use '" << strategyName 
                     << "' from block " << rawHeight(startBlock)
                     << " to " << rawHeight(endBlock) << std::endl);
            
        } catch (const std::exception& e) {
            std::cerr << "Warning: Error parsing line " << lineNumber 
                      << ": " << e.what() << std::endl;
        }
    }
    
    file.close();
    
    if (schedule.empty()) {
        std::cerr << "Warning: No valid strategy changes loaded from file" << std::endl;
        return false;
    }
    
    GAMEINFO("Successfully loaded " << schedule.size() 
             << " strategy changes for " << minerSchedules.size() 
             << " miners" << std::endl);
    
    return true;
}

double StrategyScheduler::getCurrentFeeMultiplier(BlockHeight height) const {
    double multiplier = 1.0;
    for (const auto& bp: feeSchedule) {
        if (height >= bp.BlockHeight) {
            multiplier = bp.multiplier; 
        } else {
            break;
        }
    }
    return multiplier;
}

double StrategyScheduler::getConnectivityAtHeight(unsigned int minerId, BlockHeight height) const {
    auto it = minerSchedules.find(minerId);
    if (it == minerSchedules.end()) return -1.0;

    for (const auto& change : it->second) {
        if (change.contains(height) && change.gamma >= 0.0 && change.strategyName == "default-selfish") {
            return change.gamma;
        }
    }
    return -1.0;
}


std::vector<StrategyChange> StrategyScheduler::getChangesAtHeight(BlockHeight height) const {
    std::vector<StrategyChange> changes;
    
    // Find all strategy changes that start at this height
    for (const auto& change : schedule) {
        if (rawHeight(change.startBlock) == rawHeight(height)) {
            changes.push_back(change);
        }
    }
    
    return changes;
}

std::string StrategyScheduler::getActiveStrategy(unsigned int minerId, BlockHeight height) const {
    auto it = minerSchedules.find(minerId);
    if (it == minerSchedules.end()) {
        return "";
    }
    
    // Find the strategy change that contains this height
    for (const auto& change : it->second) {
        if (change.contains(height)) {
            return change.strategyName;
        }
    }
    
    return "";
}

bool StrategyScheduler::validate(const std::vector<unsigned int>& validMinerIds,
                                 const std::vector<std::string>& validStrategyNames,
                                 std::string& errorMessage) const {
    std::vector<std::string> errors;
    
    // Check each strategy change
    for (const auto& change : schedule) {
        // Validate miner ID
        if (std::find(validMinerIds.begin(), validMinerIds.end(), change.minerId) == validMinerIds.end()) {
            errors.push_back("Invalid miner ID: " + std::to_string(change.minerId));
        }
        
        // Validate strategy name
        if (std::find(validStrategyNames.begin(), validStrategyNames.end(), change.strategyName) == validStrategyNames.end()) {
            errors.push_back("Invalid strategy name: " + change.strategyName + " for miner " + std::to_string(change.minerId));
        }
        
        // Validate block range
        if (rawHeight(change.startBlock) > rawHeight(change.endBlock)) {
            errors.push_back("Invalid block range for miner " + std::to_string(change.minerId) + 
                           ": start (" + std::to_string(rawHeight(change.startBlock)) + 
                           ") > end (" + std::to_string(rawHeight(change.endBlock)) + ")");
        }
    }
    
    // Check for overlapping ranges per miner
    for (const auto& minerPair : minerSchedules) {
        unsigned int minerId = minerPair.first;
        const auto& minerChanges = minerPair.second;
        
        for (size_t i = 0; i < minerChanges.size(); i++) {
            for (size_t j = i + 1; j < minerChanges.size(); j++) {
                if (minerChanges[i].overlaps(minerChanges[j])) {
                    errors.push_back("Overlapping block ranges for miner " + std::to_string(minerId) +
                                   ": [" + std::to_string(rawHeight(minerChanges[i].startBlock)) + 
                                   ", " + std::to_string(rawHeight(minerChanges[i].endBlock)) + 
                                   "] and [" + std::to_string(rawHeight(minerChanges[j].startBlock)) + 
                                   ", " + std::to_string(rawHeight(minerChanges[j].endBlock)) + "]");
                }
            }
        }
    }
    
    if (!errors.empty()) {
        errorMessage = "Validation errors:\n";
        for (const auto& error : errors) {
            errorMessage += "  - " + error + "\n";
        }
        return false;
    }
    
    return true;
}

void StrategyScheduler::reset() {
    minerSchedules.clear();
}

bool StrategyScheduler::hasMiner(unsigned int minerId) const {
    return minerSchedules.find(minerId) != minerSchedules.end();
}
