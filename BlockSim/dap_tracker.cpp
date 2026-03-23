//
//  dap_tracker.cpp
//  BlockSim
//

#include "dap_tracker.hpp"
#include "blockchain.hpp"
#include "block.hpp"
#include "miner.hpp"
#include "minerGroup.hpp"
 
#include <cassert>
#include <algorithm>
#include <cmath>
#include <iomanip>

DAPTracker::DAPTracker(int dapLength, size_t numMiners, std::vector<AttackerInfo> attackers , double blockReward, double txFeeRate): 
    _dapLength(dapLength),
    _numMiners(numMiners),
    _attackers(std::move(attackers)),
    _blockReward(blockReward),
    _txFeeRate(txFeeRate),
    _expectedBlockValue(0),
    _currentDAP(0),
    _baseSecondsPerBlock(0)
{
    _prevCostSnapshot.resize(numMiners, 0.0);
}

void DAPTracker::reset(BlockRate initialSecondsPerBlock) {
    _currentDAP = 0;
    _baseSecondsPerBlock = initialSecondsPerBlock;
    _dapHistory.clear();
    _revAdvantageCurve.clear();
 
    _expectedBlockValue = _blockReward + _txFeeRate * static_cast<double>(rawRate(_baseSecondsPerBlock));
    std::fill(_prevCostSnapshot.begin(), _prevCostSnapshot.end(), 0.0);
   
}

bool DAPTracker::checkAndProcessDAP(Blockchain &blockchain, const MinerGroup &minerGroup) {
    BlockHeight mainChainHeight = blockchain.getMaxHeightPub();
    int expectedDAP = rawHeight(mainChainHeight) / _dapLength;

    if(expectedDAP <= _currentDAP) {
        return false;
    }

    bool anyProcessed = false;

    while (_currentDAP < expectedDAP) {
        DAPRecord record;
        record.dapIndex = _currentDAP;
        record.startHeight = BlockHeight(_currentDAP * _dapLength + 1);
        record.endHeight = BlockHeight((_currentDAP + 1) * _dapLength);
        record.difficultyRate = blockchain.getSecondsPerBlock();

        if(_dapHistory.empty()) {
            record.startTime = BlockTime(0);
        } else {
            record.startTime = _dapHistory.back().endTime;
        }
        record.endTime = blockchain.getTime();
        record.minerStats.resize(_numMiners);

        snapShotCosts(minerGroup, record);
        _dapHistory.push_back(record);
        recomputeAllDAPs(blockchain, minerGroup);

        rebuildRevenueAdvantageCurve();

        applyDifficultyAdjustment(blockchain, _dapHistory.back());


        

        if (_boundaryCallback) {
            _boundaryCallback(_currentDAP, _dapHistory.back(), 
            _revAdvantageCurve.back());
        }
        _currentDAP++;
        anyProcessed = true;
    }
    return anyProcessed;
}

void DAPTracker::recomputeAllDAPs(const Blockchain &blockchain, const MinerGroup &minerGroup) {
    for (auto &dap: _dapHistory) {
        for (auto &ms : dap.minerStats) {
            ms.blocksOnChain = BlockCount(0);
            ms.revenue = Value(0);
        }
        dap.totalBlocksOnChain = BlockCount(0);
    }

    const Block &head = blockchain.winningHead();
    auto chain = head.getChain();

    for (const Block *block : chain) {
        if (block->height == BlockHeight(0)) {
            continue; // skip genesis block
        }
        // determine which DAP a block belongs to, and what miner added it.

        int dapIndex = (rawHeight(block->height) -1) / _dapLength;

        if (dapIndex == static_cast<int>(_dapHistory.size())) {
            continue;
        }

        DAPRecord &dap = _dapHistory[dapIndex];

        for (size_t i = 0; i < minerGroup.miners.size(); i++) {
            if (block->minedBy(minerGroup.miners[i].get())) {
                dap.minerStats[i].blocksOnChain++;
                dap.minerStats[i].revenue += block->value;
                break;
            }
        }
        dap.totalBlocksOnChain++;
    }
} 

void DAPTracker::finalize(Blockchain &blockchain, const MinerGroup &minerGroup) {
    BlockHeight mainChainHeight = blockchain.getMaxHeightPub();
    int blocksInPartialDAP = rawHeight(mainChainHeight) % _dapLength;

    if(blocksInPartialDAP == 0 && !_dapHistory.empty()) {
        return;
    }

    DAPRecord record;
    record.dapIndex = _currentDAP;
    record.startHeight = BlockHeight(_currentDAP + _dapLength + 1);
    record.endHeight = mainChainHeight;
    record.difficultyRate = blockchain.getSecondsPerBlock();
    
    if (_dapHistory.empty()) {
        record.startTime = BlockTime(0);
    } else {
        record.startTime = _dapHistory.back().endTime;
    }
    record.endTime = blockchain.getTime();
    record.minerStats.resize(_numMiners);
    snapShotCosts(minerGroup, record);

    _dapHistory.push_back(record);
    recomputeAllDAPs(blockchain, minerGroup);
    rebuildRevenueAdvantageCurve();
}
void DAPTracker::applyDifficultyAdjustment(Blockchain &blockchain, const DAPRecord &completedDAP) {
    BlockTime targetDAPTime = BlockCount(_dapLength) * _baseSecondsPerBlock;

    BlockTime actualDAPTime = completedDAP.endTime - completedDAP.startTime;

    if (rawTime(actualDAPTime) <= 0) {
        return; //failsafe
    }

    double adjustment = static_cast<double>(rawTime(actualDAPTime)) / static_cast<double>(rawTime(targetDAPTime));

    // Per the bitcoin protocol the adjustment period can be adjusted by updated of 4x at a given time.
    adjustment = std::max(0.25, std::min(adjustment, 4.0));

    BlockRate currentRate = blockchain.getSecondsPerBlock();

   double newRateRaw = static_cast<double>(rawRate(currentRate)) / adjustment;

    if (newRateRaw < 1.0) {
        newRateRaw = 1.0; //shouldn't happen but failsafe against monotonically decreasing rate.
    }

   BlockRate newRate = BlockRate(static_cast<decltype(rawRate(currentRate))>(newRateRaw));
   blockchain.setSecondsPerBlock(newRate);
}

void DAPTracker::rebuildRevenueAdvantageCurve() {
    _revAdvantageCurve.clear();
    _revAdvantageCurve.reserve(_dapHistory.size());
 
    size_t numAtk = _attackers.size();
    double baseRate = static_cast<double>(rawRate(_baseSecondsPerBlock));
 
    // Running cumulative totals per attacker
    std::vector<double> cumAtkRev(numAtk, 0.0);
    std::vector<double> cumHonestRev(numAtk, 0.0);
 
    for (const auto &dap : _dapHistory) {
 
        RevenueAdvantagePoint pt;
        pt.dapIndex = dap.dapIndex;
        pt.attackerMetrics.resize(numAtk);
 
        double totalBlk = rawCount(dap.totalBlocksOnChain);
        double elapsed = static_cast<double>(
            rawTime(dap.endTime) - rawTime(dap.startTime));
 
        // Total blocks honest mining would produce in this elapsed time
        double honestTotalBlocksThisDAP = (baseRate > 0) ? (elapsed / baseRate) : 0;
 
        for (size_t a = 0; a < numAtk; a++) {
            const auto &atk = _attackers[a];
            auto &m = pt.attackerMetrics[a];

            double atkRevThisDAP    = 0;
            double atkBlocksThisDAP = 0;
            if (atk.minerIndex < dap.minerStats.size()) {
                atkRevThisDAP    = rawValue(dap.minerStats[atk.minerIndex].revenue);
                atkBlocksThisDAP = rawCount(dap.minerStats[atk.minerIndex].blocksOnChain);
            }
            m.atkRevenueThisDAP = atkRevThisDAP;
            m.atkBlocksThisDAP  = atkBlocksThisDAP;
 
            double honestAtkBlocksThisDAP = atk.alpha * honestTotalBlocksThisDAP;
            double honestAtkRevThisDAP    = honestAtkBlocksThisDAP * _expectedBlockValue;
            m.honestCounterfactualThisDAP = honestAtkRevThisDAP;
 
            //Per-DAP revenue advantage 
            m.revenueAdvantageThisDAP = atkRevThisDAP - honestAtkRevThisDAP;
 
            cumAtkRev[a]    += atkRevThisDAP;
            cumHonestRev[a] += honestAtkRevThisDAP;
 
            m.cumulativeAtkRevenue           = cumAtkRev[a];
            m.cumulativeHonestCounterfactual = cumHonestRev[a];
            m.cumulativeRevenueAdvantage     = cumAtkRev[a] - cumHonestRev[a];
 
            // --- RRR (per-attacker share metric) ---
            if (totalBlk > 0 && atk.alpha > 0) {
                m.rrr = (atkBlocksThisDAP / totalBlk) / atk.alpha;
            }
        }
 
        _revAdvantageCurve.push_back(pt);
    }
}


void DAPTracker::printSummary(std::ostream &os) const {
    os << "\n = Revenue Advantage Curves ===" << std::endl;
    os << "DAP length: " << _dapLength << " blocks" << std::endl;
    os << "Attackers: " << _attackers.size() << std::endl;
    for (size_t a = 0; a < _attackers.size(); a++) {
        os << "  [" << a << "] miner " << _attackers[a].minerIndex
           << " (alpha=" << _attackers[a].alpha << ")";
        if (!_attackers[a].name.empty()) {
            os << " \"" << _attackers[a].name << "\"";
        }
        os << std::endl;
    }
    os << "Expected block value (honest): " << _expectedBlockValue << std::endl;
    os << "Total DAPs: " << _dapHistory.size() << std::endl;
 
    // Print per-attacker curves
    for (size_t a = 0; a < _attackers.size(); a++) {
        os << "\n - Attacker " << a;
        if (!_attackers[a].name.empty()) {
            os << " (" << _attackers[a].name << ")";
        }
        os << " alpha=" << _attackers[a].alpha << " - " << std::endl;
 
        os << std::setw(5)  << "DAP"
           << std::setw(14) << "AtkRev"
           << std::setw(14) << "HonestCF"
           << std::setw(14) << "RA(d)"
           << std::setw(14) << "CumRA"
           << std::setw(10) << "RRR"
           << std::setw(12) << "SecPerBlk"
           << std::endl;
        os << std::string(83, '-') << std::endl;
 
        for (const auto &pt : _revAdvantageCurve) {
            const auto &m = pt.attackerMetrics[a];
            os << std::setw(5)  << pt.dapIndex
               << std::setw(14) << std::fixed << std::setprecision(1) << m.atkRevenueThisDAP
               << std::setw(14) << std::fixed << std::setprecision(1) << m.honestCounterfactualThisDAP
               << std::setw(14) << std::fixed << std::setprecision(1) << m.revenueAdvantageThisDAP
               << std::setw(14) << std::fixed << std::setprecision(1) << m.cumulativeRevenueAdvantage
               << std::setw(10) << std::fixed << std::setprecision(4) << m.rrr;
 
            if (pt.dapIndex < static_cast<int>(_dapHistory.size())) {
                os << std::setw(12) << rawRate(_dapHistory[pt.dapIndex].difficultyRate);
            }
            os << std::endl;
        }
 
        if (!_revAdvantageCurve.empty()) {
            auto &last = _revAdvantageCurve.back().attackerMetrics[a];
            os << "Final cumulative RA: "
               << std::fixed << std::setprecision(1) << last.cumulativeRevenueAdvantage
               << "  (" << (last.cumulativeRevenueAdvantage > 0 ? "POSITIVE" : "NEGATIVE")
               << ")" << std::endl;
        }
    }
}

void DAPTracker::snapShotCosts(const MinerGroup &minerGroup,
                                DAPRecord &record) {
    for (size_t i = 0; i < _numMiners; i++) {
        double currentCost = minerGroup.miners[i]->totalMiningCost;
        record.minerStats[i].miningCostSnapshot = currentCost;
        record.minerStats[i].costDuringDAP      = currentCost - _prevCostSnapshot[i];
        _prevCostSnapshot[i] = currentCost;
    }
}
