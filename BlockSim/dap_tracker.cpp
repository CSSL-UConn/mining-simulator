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
#include <chrono>
#include <thread>

DAPTracker::DAPTracker(int dapLength, size_t numMiners, std::vector<AttackerInfo> attackers, double blockReward, double txFeeRate): 
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
    _prevBlocksMinedSnapshot.resize(numMiners, BlockCount(0));
    _runningAtkRevThisDAP.resize(_attackers.size(), 0.0);
    _runningCumRA.resize(_attackers.size(), 0.0);
}

void DAPTracker::reset(BlockRate initialSecondsPerBlock) {
    _currentDAP = 0;
    _baseSecondsPerBlock = initialSecondsPerBlock;
    _dapHistory.clear();
    _revAdvantageCurve.clear();
 
    _expectedBlockValue = _blockReward + _txFeeRate * static_cast<double>(rawRate(_baseSecondsPerBlock));
    std::fill(_prevCostSnapshot.begin(), _prevCostSnapshot.end(), 0.0);
    std::fill(_runningAtkRevThisDAP.begin(), _runningAtkRevThisDAP.end(), 0.0);
    std::fill(_runningCumRA.begin(),         _runningCumRA.end(),         0.0);
   
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

        if (_epochMasterLog.is_open() || _epochRollingFile.is_open()) {
            if (_epochWindow.size() >= 2) {
                if (_epochMasterLog.is_open())
                    writeEpochRow(_epochMasterLog,
                                _epochWindow.front(),
                                _revAdvantageCurve[_epochWindow.front().dapIndex]);
                _epochWindow.pop_front();
            }
            _epochWindow.push_back(_dapHistory.back());
            rewriteEpochRolling();
        }


        std::fill(_runningAtkRevThisDAP.begin(), _runningAtkRevThisDAP.end(), 0.0);
        if (!_revAdvantageCurve.empty()) {
            const auto &lastPt = _revAdvantageCurve.back();
            for (size_t a = 0; a < _attackers.size(); a++)
                _runningCumRA[a] = lastPt.attackerMetrics[a].cumulativeRevenueAdvantage;
        }

        flushBlockRolling();
        pauseForAgent();
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
           << std::setw(12) << "OrphanRate"
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
                const auto &dap = _dapHistory[pt.dapIndex];
                double totalBlk   = rawCount(dap.totalBlocksOnChain);
                double totalMined = rawCount(dap.totalBlocksMined);
                double orphanRate = (totalMined > 0) ? 1.0 - (totalBlk / totalMined) : 0.0;
                os << std::setw(12) << rawRate(dap.difficultyRate);
                os << std::setw(12) << std::fixed << std::setprecision(4) << orphanRate;
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
    BlockCount totalMinedThisDAP(0);
    for (size_t i = 0; i < _numMiners; i++) {
        double currentCost = minerGroup.miners[i]->totalMiningCost;
        record.minerStats[i].miningCostSnapshot = currentCost;
        record.minerStats[i].costDuringDAP      = currentCost - _prevCostSnapshot[i];
        _prevCostSnapshot[i] = currentCost;

        BlockCount currentMined = minerGroup.miners[i]->getBlocksMinedTotal();
        totalMinedThisDAP += currentMined - _prevBlocksMinedSnapshot[i];
        _prevBlocksMinedSnapshot[i] = currentMined;
    }
    record.totalBlocksMined = totalMinedThisDAP;
}

void DAPTracker::setOutputBase(const std::string &base) {
    _outputBase = base;

    // Block rolling file (overwritten each block — open truncating)
    _blockRollingFile.open(base + "block_rolling.csv", std::ios::trunc);
    writeBlockHeader(_blockRollingFile);

    // Block master log (append-only)
    _blockMasterLog.open(base + "block_log.csv", std::ios::app);
    if (_blockMasterLog.tellp() == 0)
        writeBlockHeader(_blockMasterLog);

    // Epoch rolling file
    _epochRollingFile.open(base + "epoch_rolling.csv", std::ios::trunc);
    writeEpochHeader(_epochRollingFile);

    // Epoch master log
    _epochMasterLog.open(base + "epoch_log.csv", std::ios::app);
    if (_epochMasterLog.tellp() == 0)
        writeEpochHeader(_epochMasterLog);
}

void DAPTracker::writeBlockHeader(std::ostream &os) const {
    os << "block_height,timestamp,miner_id,block_value,seconds_per_block";
    for (size_t i = 0; i < _numMiners; i++)
    os << ",gamma_miner" << i;
    for (size_t a = 0; a < _attackers.size(); a++) {
        std::string n = _attackers[a].name.empty()
                        ? std::to_string(a) : _attackers[a].name;
        os << "," << n << "_alpha"
        << "," << n << "_atk_rev_this_dap"
           << "," << n << "_cumulative_ra";
    }
    os << "\n";
}

void DAPTracker::writeBlockRow(std::ostream &os, const BlockRecord &r) const {
    os << rawHeight(r.height)    << ","
       << rawTime(r.timestamp)   << ","
       << r.minerId              << ","
       << rawValue(r.blockValue) << ","
       << rawRate(r.secondsPerBlock);

    for (size_t i = 0; i < _numMiners; i++) {
        double g = (i < r.gammaPerMiner.size()) ? r.gammaPerMiner[i] : -1.0;
        os << "," << g;
    }
    for (size_t a = 0; a < _attackers.size(); a++) {
        os << "," << r.alphas[a] 
        << "," << r.atkRevenueThisDAP[a]
           << "," << r.cumulativeRA[a];
    }
    os << "\n";
}

void DAPTracker::rewriteBlockRolling() {
    if (!_blockRollingFile.is_open()) return;
    _blockRollingFile.seekp(0);
    _blockRollingFile.clear();

    // Truncate by closing and reopening
    _blockRollingFile.close();
    _blockRollingFile.open(_outputBase + "block_rolling.csv", std::ios::trunc);
    writeBlockHeader(_blockRollingFile);
    for (const auto &r : _blockWindow)
        writeBlockRow(_blockRollingFile, r);
    _blockRollingFile.flush();
}

void DAPTracker::recordBlock(Blockchain &blockchain, const MinerGroup &minerGroup) {
    if (_outputBase.empty()) return;

    BlockHeight height    = blockchain.getMaxHeightPub();
    BlockTime   timestamp = blockchain.getTime();
    BlockRate   spb       = blockchain.getSecondsPerBlock();

    int minerId = -1;
    Value blockVal(0);
    const Block &head = blockchain.winningHead();
    if (head.height == height && head.miner != nullptr) {
        blockVal = head.value;
        for (size_t i = 0; i < minerGroup.miners.size(); i++) {
            if (head.minedBy(minerGroup.miners[i].get())) {
                minerId = static_cast<int>(i);
                break;
            }
        }
    }

    if (minerId >= 0) {
        for (size_t a = 0; a < _attackers.size(); a++) {
            if (_attackers[a].minerIndex == static_cast<size_t>(minerId)) {
                _runningAtkRevThisDAP[a] += rawValue(blockVal);
            }
        }
    }

    BlockRecord rec;
    rec.height            = height;
    rec.timestamp         = timestamp;
    rec.minerId           = minerId;
    rec.blockValue        = blockVal;
    rec.secondsPerBlock   = spb;
    rec.atkRevenueThisDAP = _runningAtkRevThisDAP;
    rec.cumulativeRA      = _runningCumRA;
    rec.gammaPerMiner = _gammaPerMiner;
    rec.alphas.resize(_attackers.size());
    for (size_t a = 0; a < _attackers.size(); a++) {
        rec.alphas[a] = _attackers[a].alpha;
    }

    if (_blockWindow.size() >= 200) {
    _blockMasterBuffer.push_back(_blockWindow.front());
    _blockWindow.pop_front();
    }

    _blockWindow.push_back(rec);
   
}

void DAPTracker::writeEpochHeader(std::ostream &os) const {
    os << "dap_index,start_height,end_height,start_time,end_time,"
       << "total_blocks_on_chain,total_blocks_mined,seconds_per_block";
    for (size_t a = 0; a < _attackers.size(); a++) {
        std::string n = _attackers[a].name.empty()
                        ? std::to_string(a) : _attackers[a].name;
        os << "," << n << "_atk_blocks"
           << "," << n << "_atk_revenue"
           << "," << n << "_honest_cf"
           << "," << n << "_ra_this_dap"
           << "," << n << "_cumulative_ra"
           << "," << n << "_rrr";
    }
    os << "\n";
}


void DAPTracker::writeEpochRow(std::ostream &os, const DAPRecord &d,
                                const RevenueAdvantagePoint &pt) const {
    os << d.dapIndex                      << ","
       << rawHeight(d.startHeight)        << ","
       << rawHeight(d.endHeight)          << ","
       << rawTime(d.startTime)            << ","
       << rawTime(d.endTime)              << ","
       << rawCount(d.totalBlocksOnChain)  << ","
       << rawCount(d.totalBlocksMined)    << ","
       << rawRate(d.difficultyRate);
    for (size_t a = 0; a < _attackers.size(); a++) {
        const auto &m = pt.attackerMetrics[a];
        os << "," << m.atkBlocksThisDAP
           << "," << m.atkRevenueThisDAP
           << "," << m.honestCounterfactualThisDAP
           << "," << m.revenueAdvantageThisDAP
           << "," << m.cumulativeRevenueAdvantage
           << "," << m.rrr;
    }
    os << "\n";
}


void DAPTracker::rewriteEpochRolling() {
    if (!_epochRollingFile.is_open()) return;
    _epochRollingFile.close();
    _epochRollingFile.open(_outputBase + "epoch_rolling.csv", std::ios::trunc);
    writeEpochHeader(_epochRollingFile);
    for (const auto &d : _epochWindow) {
        int idx = d.dapIndex;
        if (idx < static_cast<int>(_revAdvantageCurve.size()))
            writeEpochRow(_epochRollingFile, d, _revAdvantageCurve[idx]);
    }
    _epochRollingFile.flush();
}

void DAPTracker::pauseForAgent() {
    if (_outputBase.empty()) return;

    const std::string lockPath = "Claude/sim_pause.lock";
    const int timeoutSeconds   = 30;
    const int pollMs           = 200;

    {
        std::ofstream lock(lockPath);
        if (!lock.is_open()) {
            std::cerr << "[agent] Warning: could not create sim_pause.lock\n";
            return;
        }
    }

    std::cerr << "[agent] Epoch " << _currentDAP
              << " closed — pausing for agent (max " << timeoutSeconds << "s)...\n";

    auto start = std::chrono::steady_clock::now();

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(pollMs));
        std::ifstream check(lockPath);
        if (!check.is_open()) {
            std::cerr << "[agent] Lock released — resuming sim.\n";
            return;
        }
        check.close();

        auto elapsed = std::chrono::steady_clock::now() - start;
        if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count()
                >= timeoutSeconds) {
            std::cerr << "[agent] Timeout waiting for agent — resuming anyway.\n";

            std::remove(lockPath.c_str());
            return;
        }
    }
}

void DAPTracker::flushBlockRolling() {
    if (_blockMasterLog.is_open()) {
        for (const auto &r : _blockMasterBuffer)
            writeBlockRow(_blockMasterLog, r);
        _blockMasterBuffer.clear();
        _blockMasterLog.flush();
    }
    if (_epochMasterLog.is_open())
        _epochMasterLog.flush();
    rewriteBlockRolling();
}
