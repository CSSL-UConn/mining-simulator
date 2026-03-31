//
//  main.cpp
//  TTP_Selfish_Sim


#include "BlockSim/strategy.hpp"
#include "BlockSim/utils.hpp"
#include "BlockSim/miner.hpp"
#include "BlockSim/block.hpp"
#include "BlockSim/blockchain.hpp"
#include "BlockSim/minerStrategies.h"
#include "BlockSim/logging.h"
#include "BlockSim/game.hpp"
#include "BlockSim/game_ttp.hpp"
#include "BlockSim/dap_tracker.hpp"
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


#define NOISE_IN_TRANSACTIONS false

#define NETWORK_DELAY BlockTime(0)
#define EXPECTED_NUMBER_OF_BLOCKS BlockCount(12096)

#define LAMBERT_COEFF 0.13533528323661

#define B BlockValue(Value(20) * SATOSHI_PER_BITCOIN)
#define TOTAL_BLOCK_VALUE BlockValue(Value(25) * SATOSHI_PER_BITCOIN)

#define SEC_PER_BLOCK BlockRate(600)

#define A (TOTAL_BLOCK_VALUE - B)/SEC_PER_BLOCK

#define DAP_LENGTH 2016
#define ATTACKER_INDEX 0

int main(int argc, const char *argv[]) {

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <output_suffix>" << std::endl;
        return 1;
    }

    int numberOfGames = 100;

    GAMEINFO("#####\nRunning Selfish Mining Revenue Advantage Simulation\n#####" << std::endl);

    std::ofstream plot;
    char filename[1024] = {0};
    sprintf(filename, "%s_%s.txt", argv[0], argv[1]);
    plot.open(filename);
    if (!plot.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return 1;
    }
    plot << "Gamma, Selfish Profit Fraction, Selfish Hash Rate, Honest Profit Fraction, "
         << "Final Cumulative Revenue Advantage, Num DAPs"
         << std::endl;

    std::ofstream dapDetail;
    char dapFilename[1024] = {0};
    sprintf(dapFilename, "%s_%s_dap_detail.csv", argv[0], argv[1]);
    dapDetail.open(dapFilename);
    if (!dapDetail.is_open()) {
        std::cerr << "Error opening DAP detail file: " << dapFilename << std::endl;
        return 1;
    }
    dapDetail << "gamma,hash_rate,game_num,"
              << "dap,attacker_id,attacker_name,attacker_alpha,"
              << "start_height,end_height,start_time,end_time,"
              << "atk_blocks,total_blocks,"
              << "atk_revenue,honest_counterfactual_revenue,revenue_advantage,"
              << "cumulative_atk_revenue,cumulative_honest_revenue,cumulative_revenue_advantage,"
              << "rrr,seconds_per_block,orphan_rate"
              << std::endl;

    for (double gammaVal = 0.0; gammaVal < 1.005; gammaVal += .005) {

    for (double hashVal = 0.005; hashVal < .505; hashVal += .005) {

        HashRate selfishPower = HashRate(hashVal);
        HashRate honestPower  = HashRate(1 - hashVal);

        for (int gameNum = 1; gameNum <= numberOfGames; gameNum++) {

            std::vector<std::unique_ptr<Miner>> miners;
            using std::placeholders::_1;
            using std::placeholders::_2;

            std::function<Value(const Blockchain &, Value)> forkFunc(
                std::bind(functionForkPercentage, _1, _2, 2));

            auto defaultStrat = createDefaultStubbornTrailStrategy(NOISE_IN_TRANSACTIONS, gammaVal);
            auto selfishStrat = createStubbornTrailStrategy(NOISE_IN_TRANSACTIONS,1);

            MinerParameters selfishMinerParams = {
                0, std::to_string(0), selfishPower, NETWORK_DELAY, COST_PER_SEC_TO_MINE
            };
            MinerParameters defaultMinerParams = {
                1, std::to_string(1), honestPower, NETWORK_DELAY, COST_PER_SEC_TO_MINE
            };

            miners.push_back(std::make_unique<Miner>(selfishMinerParams, *selfishStrat));
            miners.push_back(std::make_unique<Miner>(defaultMinerParams, *defaultStrat));

            MinerGroup minerGroup(std::move(miners));

            BlockchainSettings blockchainSettings = {SEC_PER_BLOCK, A, B, EXPECTED_NUMBER_OF_BLOCKS};
            GameSettings settings = {blockchainSettings};

            auto blockchain = std::make_unique<Blockchain>(settings.blockchainSettings);
            minerGroup.reset(*blockchain);
            minerGroup.resetOrder();

            std::vector<AttackerInfo> attackers;
            attackers.emplace_back(ATTACKER_INDEX, hashVal, "Selfish");

            DAPTracker dapTracker(
                DAP_LENGTH,
                minerGroup.miners.size(),
                attackers,
                static_cast<double>(rawValue(blockchainSettings.blockReward)),
                static_cast<double>(rawRate(blockchainSettings.transactionFeeRate))
            );
            dapTracker.reset(blockchainSettings.secondsPerBlock);

            auto result = runGameTTP(minerGroup, *blockchain, settings, dapTracker);
            auto minerResults = result.minerResults;

            const double EPSILON = 1e-9;
            assert(minerResults[0].totalProfit <= result.moneyInLongestChain + EPSILON);

            auto fractionOfProfits = valuePercentage(minerResults[0].totalProfit, result.moneyInLongestChain);

            auto honestFractionOfProfits = valuePercentage( minerResults[1].totalProfit, result.moneyInLongestChain);

            const auto &curve = dapTracker.revenueAdvantageCurve();
            double finalCumRA = 0;
            if (!curve.empty()) {
                finalCumRA = curve.back().attackerMetrics[0].cumulativeRevenueAdvantage;
            }

            plot << gammaVal << ", "
                 << fractionOfProfits << ", "
                 << selfishPower << ", "
                 << honestFractionOfProfits << ", "
                 << finalCumRA << ", "
                 << curve.size()
                 << std::endl;

            const auto &history = dapTracker.history();
            for (size_t d = 0; d < curve.size(); d++) {
                const auto &pt  = curve[d];
                const auto &dap = history[d];
                const auto &atk = dapTracker.attackers()[0];
                const auto &m   = pt.attackerMetrics[0];

                double totalBlk = rawCount(dap.totalBlocksOnChain);
                double totalMined = rawCount(dap.totalBlocksMined);
                double orphanRate = (totalMined > 0) ? 1.0 - (totalBlk / totalMined) : 0.0;

                dapDetail << gammaVal << ","
                          << hashVal << ","
                          << gameNum << ","
                          << pt.dapIndex << ","
                          << 0 << ","
                          << atk.name << ","
                          << atk.alpha << ","
                          << rawHeight(dap.startHeight) << ","
                          << rawHeight(dap.endHeight) << ","
                          << rawTime(dap.startTime) << ","
                          << rawTime(dap.endTime) << ","
                          << static_cast<int>(m.atkBlocksThisDAP) << ","
                          << static_cast<int>(totalBlk) << ","
                          << m.atkRevenueThisDAP << ","
                          << m.honestCounterfactualThisDAP << ","
                          << m.revenueAdvantageThisDAP << ","
                          << m.cumulativeAtkRevenue << ","
                          << m.cumulativeHonestCounterfactual << ","
                          << m.cumulativeRevenueAdvantage << ","
                          << m.rrr << ","
                          << rawRate(dap.difficultyRate) << ","
                          << orphanRate
                          << std::endl;
            }

        } // gameNum
    } // hashVal
    } // gammaVal

    plot.close();
    dapDetail.close();

   GAMEINFO("Games over." << std::endl);

    std::cerr << "Results written to: " << filename << std::endl;
    std::cerr << "DAP detail written to: " << dapFilename << std::endl;

    return 0;
}