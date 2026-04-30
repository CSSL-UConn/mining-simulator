//
//  strategy_factory.hpp
//  BlockSim
//
//  Factory for creating strategies by name
//

#ifndef strategy_factory_hpp
#define strategy_factory_hpp

#include <string>
#include <memory>

class Strategy;

// Factory function to create strategies by name
std::unique_ptr<Strategy> createStrategyByName(const std::string& name,
                                                bool atomic,
                                                bool noiseInTransactions,
                                                double gamma,
                                                bool whaleEnabled = false,
                                                double whaleProb = 0.05,
                                                double whaleMultiplier = 3.0);

#endif /* strategy_factory_hpp */
