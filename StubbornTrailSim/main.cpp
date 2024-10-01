//
//  main.cpp
//  BlockSim


#include "BlockSim/strategy.hpp"
#include "BlockSim/utils.hpp"
#include "BlockSim/miner.hpp"
#include "BlockSim/block.hpp"
#include "BlockSim/blockchain.hpp"
#include "BlockSim/minerStrategies.h"
#include "BlockSim/logging.h"
#include "BlockSim/game.hpp"
#include "BlockSim/minerGroup.hpp"
#include "BlockSim/minerStrategies.h"
#include "BlockSim/game_result.hpp"
#include "BlockSim/miner_result.hpp"
#include "BlockSim/mining_style.hpp"

#include <cassert>
#include <iostream>
#include <fstream>
#include <cmath>
#include <queue>
#include <random>
#include <algorithm>
#include <vector>
#include <map>


#define NOISE_IN_TRANSACTIONS false //miners don't put the max value they can into a block (simulate tx latency)

#define NETWORK_DELAY BlockTime(0)         //network delay in seconds for network to hear about your block
#define EXPECTED_NUMBER_OF_BLOCKS BlockCount(10000)

#define LAMBERT_COEFF 0.13533528323661//coeff for lambert func equil  must be in [0,.2]
//0.13533528323661 = 1/(e^2)

#define B BlockValue(0) // Block reward
//#define TOTAL_BLOCK_VALUE BlockValue(15.625)
#define TOTAL_BLOCK_VALUE BlockValue(Value(25) * SATOSHI_PER_BITCOIN)

#define SEC_PER_BLOCK BlockRate(600)     //mean time in seconds to find a block

//#define B BlockValue(3.125) // Block reward
#define A (TOTAL_BLOCK_VALUE - B)/SEC_PER_BLOCK  //rate transactions come in

//#define SELFISH_GAMMA 0.0 //fraction of network favoring your side in a dead tie
//half way and miners have equal hash power

int main(int argc, const char *argv[]) {

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <percentageGamma>" << std::endl;
        return 1;
    }
    
    int numberOfGames = 100;
    
    //#########################################################################################
    //idea of simulation: 2 miners, only an honest, and a selfish miner running a stubborn mining strategy. Run many games, with the
    //size of the two changing. Plot the expected profit vs. actual profit. (reproduce fig 2 in selfish paper)
    GAMEINFO("#####\nRunning Selfish Mining Simulation\n#####" << std::endl);
    std::ofstream plot;
    char  filename[1024] = {0};
    sprintf(filename, "%s_%s.txt", argv[0], argv[1]);
    plot.open(filename);
    if (!plot.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return 1;
    }
    plot << "Gamma, Selfish Profit, Selfish Hash Rate, Honest Profit" << std::endl;
    //start running games

    for(double gammaVal = 0.0; gammaVal < 1.01; gammaVal+=0.005) {

    for(double hashVal = 0.005; hashVal < .51; hashVal+=.005) {

        HashRate attackerPower = HashRate(hashVal);
        HashRate honestPower = HashRate(1-hashVal);

        for (int gameNum = 1; gameNum <= numberOfGames; gameNum++) {
        

        std::vector<std::unique_ptr<Miner>> miners;

        using std::placeholders::_1;
        using std::placeholders::_2;
        
        std::function<Value(const Blockchain &, Value)> forkFunc(std::bind(functionForkPercentage, _1, _2, 2));

//        auto defaultStrat = createPettyStrategy(NOISE_IN_TRANSACTIONS, SELFISH_GAMMA);
        auto defaultStubbornTrailStrat = createDefaultStubbornTrailStrategy(NOISE_IN_TRANSACTIONS, gammaVal);
        auto trailStrat1 = createStubbornTrailStrategy(NOISE_IN_TRANSACTIONS, 1);

        MinerParameters selfishMinerParams = {0, std::to_string(0), attackerPower, NETWORK_DELAY, COST_PER_SEC_TO_MINE};
        MinerParameters defaultinerParams = {2, std::to_string(1), honestPower, NETWORK_DELAY, COST_PER_SEC_TO_MINE};

        
        miners.push_back(std::make_unique<Miner>(selfishMinerParams, *trailStrat1));
        //miners.push_back(std::make_unique<Miner>(pettyMinerParams, *selfishStrat2));
        miners.push_back(std::make_unique<Miner>(defaultinerParams, *defaultStubbornTrailStrat));
        
        MinerGroup minerGroup(std::move(miners));
        
        GAMEINFO("\n\nGame#: " << gameNum << " The board is set, the pieces are in motion..." << std::endl);
        GAMEINFO("miner ratio:" << attackerPower << " selfish." << std::endl);
        
        BlockchainSettings blockchainSettings = {SEC_PER_BLOCK, A, B, EXPECTED_NUMBER_OF_BLOCKS};
        GameSettings settings = {blockchainSettings};
        
        
        auto blockchain = std::make_unique<Blockchain>(settings.blockchainSettings);
        minerGroup.reset(*blockchain);
        minerGroup.resetOrder();
        
        GAMEINFO("\n\nGame#: " << gameNum << " The board is set, the pieces are in motion..." << std::endl);
        
        auto result = runGame(minerGroup, *blockchain, settings);
        
        auto minerResults = result.minerResults;
        
        GAMEINFO("The game is complete. Calculate the scores:" << std::endl);

        GAMEINFO("Total profit:" << result.moneyInLongestChain << std::endl);
        
        assert(minerResults[0].totalProfit <= result.moneyInLongestChain);
        
        auto fractionOfProfits = valuePercentage(minerResults[0].totalProfit, result.moneyInLongestChain);
        auto honestFractionOfProfits = valuePercentage(minerResults[1].totalProfit, result.moneyInLongestChain);

        GAMEINFO("Gamma" << "Fraction earned by stubborn trail:" << fractionOfProfits << " with " << attackerPower << " fraction of hash power" << "Fraction by honest: " << honestFractionOfProfits << std::endl);
        plot << gammaVal << ", " << fractionOfProfits<< ", " << attackerPower << ", " << honestFractionOfProfits  << std::endl;
        
        }
    }
    }  
    
    GAMEINFO("Games over." << std::endl);
}