//
//  main.cpp
//  TTP_DoubleStrat_Sim


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
#define EXPECTED_NUMBER_OF_BLOCKS BlockCount(20000)

#define LAMBERT_COEFF 0.13533528323661

#define B BlockValue(Value(22) * SATOSHI_PER_BITCOIN)
#define TOTAL_BLOCK_VALUE BlockValue(Value(25) * SATOSHI_PER_BITCOIN)

#define SEC_PER_BLOCK BlockRate(600)

#define A (TOTAL_BLOCK_VALUE - B)/SEC_PER_BLOCK

#define DAP_LENGTH 2016

int main(int argc, const char *argv[]) {

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <output_suffix>" << std::endl;
        return 1;
    }

    int numberOfGames = 10;

    GAMEINFO("#####\nRunning Double Selfish Mining TTP Simulation\n#####" << std::endl);

    // Output file 1: per-game summary
    std::ofstream plot;
    char filename[1024] = {0};
    sprintf(filename, "%s_%s.txt", argv[0], argv[1]);
    plot.open(filename);
    if (!plot.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return 1;
    }
    plot << "Gamma, Selfish1 Profit Fraction, Selfish2 Profit Fraction, "
         << "Individual Miner Hash Rate, Honest Profit Fraction, "
         << "Selfish1 Final CumRA, Selfish2 Final CumRA, Num DAPs"
         << std::endl;

    // Output file 2: per-DAP per-attacker revenue advantage curves
    std::ofstream dapDetail;
    char dapFilename[1024] = {0};
    sprintf(dapFilename, "%s_%s_dap_detail.csv", argv[0], argv[1]);
    dapDetail.open(dapFilename);
    if (!dapDetail.is_open()) {
        std::cerr << "Error opening DAP detail file: " << dapFilename << std::endl;
        return 1;
    }
    dapDetail << "gamma,combined_hash_rate,game_num,"
              << "dap,attacker_id,attacker_name,attacker_alpha,"
              << "start_height,end_height,start_time,end_time,"
              << "atk_blocks,total_blocks,"
              << "atk_revenue,honest_counterfactual_revenue,revenue_advantage,"
              << "cumulative_atk_revenue,cumulative_honest_revenue,cumulative_revenue_advantage,"
              << "rrr,seconds_per_block, orphan_rate"
              << std::endl;

    for (double gammaVal = 0.0; gammaVal < 1.01; gammaVal += 0.25) {

    for (double hashVal = 0.15; hashVal < .70; hashVal += .01) {

        HashRate selfishPower1 = HashRate(hashVal / 2);
        HashRate selfishPower2 = HashRate(hashVal / 2);
        HashRate honestPower   = HashRate(1 - hashVal);

        for (int gameNum = 1; gameNum <= numberOfGames; gameNum++) {
            std::vector<std::unique_ptr<Miner>> miners;
            using std::placeholders::_1;
            using std::placeholders::_2;

            std::function<Value(const Blockchain &, Value)> forkFunc(
                std::bind(functionForkPercentage, _1, _2, 2));

            auto defaultStrat  = createDefaultStubbornTrailStrategy(NOISE_IN_TRANSACTIONS, gammaVal);
            auto publishStrat  = createSelfishStrategy(NOISE_IN_TRANSACTIONS);
            auto publishStrat2 = createSelfishStrategy(NOISE_IN_TRANSACTIONS);

            MinerParameters selfishMinerParams1 = {
                0, std::to_string(0), selfishPower1, NETWORK_DELAY, COST_PER_SEC_TO_MINE
            };
            MinerParameters selfishMinerParams2 = {
                1, std::to_string(1), selfishPower2, NETWORK_DELAY, COST_PER_SEC_TO_MINE
            };
            MinerParameters defaultMinerParams = {
                2, std::to_string(2), honestPower, NETWORK_DELAY, COST_PER_SEC_TO_MINE
            };

            miners.push_back(std::make_unique<Miner>(selfishMinerParams1, *publishStrat2));
            miners.push_back(std::make_unique<Miner>(selfishMinerParams2, *publishStrat));
            miners.push_back(std::make_unique<Miner>(defaultMinerParams, *defaultStrat));

            MinerGroup minerGroup(std::move(miners));

            BlockchainSettings blockchainSettings = {SEC_PER_BLOCK, A, B, EXPECTED_NUMBER_OF_BLOCKS};
            GameSettings settings = {blockchainSettings};

            auto blockchain = std::make_unique<Blockchain>(settings.blockchainSettings);
            minerGroup.reset(*blockchain);
            minerGroup.resetOrder();

            std::vector<AttackerInfo> attackers;
            attackers.emplace_back(0, rawRate(selfishPower1), "Selfish1");
            attackers.emplace_back(1, rawRate(selfishPower2), "Selfish2");

            DAPTracker dapTracker(
                DAP_LENGTH,
                minerGroup.miners.size(),  // 3 miners
                attackers,
                static_cast<double>(rawValue(blockchainSettings.blockReward)),
                static_cast<double>(rawRate(blockchainSettings.transactionFeeRate))
            );
            dapTracker.reset(blockchainSettings.secondsPerBlock);

            auto result = runGameTTP(minerGroup, *blockchain, settings, dapTracker);

            auto minerResults = result.minerResults;

            assert(minerResults[0].totalProfit <= result.moneyInLongestChain);

            auto fractionOfProfits1 = valuePercentage(
                minerResults[0].totalProfit, result.moneyInLongestChain);
            auto fractionOfProfits2 = valuePercentage(
                minerResults[1].totalProfit, result.moneyInLongestChain);
            auto honestFractionOfProfits = valuePercentage(
                minerResults[2].totalProfit, result.moneyInLongestChain);

            const auto &curve = dapTracker.revenueAdvantageCurve();
            double finalCumRA1 = 0, finalCumRA2 = 0;
            if (!curve.empty()) {
                finalCumRA1 = curve.back().attackerMetrics[0].cumulativeRevenueAdvantage;
                finalCumRA2 = curve.back().attackerMetrics[1].cumulativeRevenueAdvantage;
            }

            plot << gammaVal << ", "
                 << fractionOfProfits1 << ", "
                 << fractionOfProfits2 << ", "
                 << selfishPower1 << ", "
                 << honestFractionOfProfits << ", "
                 << finalCumRA1 << ", "
                 << finalCumRA2 << ", "
                 << curve.size()
                 << std::endl;


            const auto &history = dapTracker.history();
            for (size_t d = 0; d < curve.size(); d++) {
                const auto &pt  = curve[d];
                const auto &dap = history[d];

                double totalBlk = rawCount(dap.totalBlocksOnChain);
                double totalMined = rawCount(dap.totalBlocksMined);
                double orphanRate = (totalMined > 0) ? 1.0 - (totalBlk / totalMined) : 0.0;

                for (size_t a = 0; a < dapTracker.numAttackers(); a++) {
                    const auto &atk = dapTracker.attackers()[a];
                    const auto &m   = pt.attackerMetrics[a];

                    dapDetail << gammaVal << ","
                              << hashVal << ","
                              << gameNum << ","
                              << pt.dapIndex << ","
                              << a << ","
                              << (atk.name.empty() ? std::to_string(a) : atk.name) << ","
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
                              << rawRate(dap.difficultyRate)<< ","
                              << orphanRate
                              << std::endl;
                }
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