//
//  gamma_scheduler.cpp
//  BlockSim
//

#include "gamma_scheduler.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

GammaScheduler::GammaScheduler() {}

bool GammaScheduler::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open gamma schedule file: " << filename << std::endl;
        return false;
    }
    
    schedule.clear();
    std::string line;
    int lineNum = 0;
    
    while (std::getline(file, line)) {
        lineNum++;
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse: start_block, end_block, gamma
        std::stringstream ss(line);
        std::string startStr, endStr, gammaStr;
        
        if (!std::getline(ss, startStr, ',')) continue;
        if (!std::getline(ss, endStr, ',')) continue;
        if (!std::getline(ss, gammaStr, ',')) continue;
        
        // Trim whitespace
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            s.erase(s.find_last_not_of(" \t\r\n") + 1);
        };
        
        trim(startStr);
        trim(endStr);
        trim(gammaStr);
        
        try {
            unsigned int start = std::stoul(startStr);
            unsigned int end = std::stoul(endStr);
            double gamma = std::stod(gammaStr);
            
            if (gamma < 0.0 || gamma > 1.0) {
                std::cerr << "Warning: Gamma value " << gamma << " out of range [0,1] on line " << lineNum << std::endl;
                continue;
            }
            
            if (start > end) {
                std::cerr << "Warning: Start block > end block on line " << lineNum << std::endl;
                continue;
            }
            
            schedule.emplace_back(BlockHeight(start), BlockHeight(end), gamma);
            std::cout << "Loaded gamma change: gamma=" << gamma << " from block " << start << " to " << end << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Error parsing line " << lineNum << ": " << e.what() << std::endl;
            continue;
        }
    }
    
    std::cout << "Successfully loaded " << schedule.size() << " gamma changes" << std::endl;
    return !schedule.empty();
}

double GammaScheduler::getActiveGamma(BlockHeight height) const {
    for (const auto& change : schedule) {
        if (change.contains(height)) {
            return change.gamma;
        }
    }
    return 0.5; // Default gamma if not specified
}

void GammaScheduler::reset() {
    schedule.clear();
}
