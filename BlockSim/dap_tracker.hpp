//
//  dap_tracker.hpp
//  BlockSim

#ifndef dap_tracker_hpp
#define dap_tracker_hpp
 
#include "typeDefs.hpp"
#include <vector>
#include <functional>
#include <iostream>
#include <fstream>
#include <deque>
#include <string>

 
class Blockchain;
class Miner;
class MinerGroup;
class Block;

struct AttackerInfo {
    size_t minerIndex;
    double alpha; 
    std::string name;
    AttackerInfo(size_t idx, double a, const std::string &n = "")
        : minerIndex(idx), alpha(a), name(n) {}
};


struct MinerDAPStats {
    BlockCount blocksOnChain;
    Value      revenue;   
    double     miningCostSnapshot; 
    double     costDuringDAP;      // incremental cost for this DAP alone
 
    MinerDAPStats()
        : blocksOnChain(0), revenue(0),
          miningCostSnapshot(0), costDuringDAP(0) {}
};

struct DAPRecord {
    int         dapIndex;
    BlockHeight startHeight;
    BlockHeight endHeight;
    BlockTime   startTime;
    BlockTime   endTime;
    BlockRate   difficultyRate; 
    BlockCount  totalBlocksOnChain;
    BlockCount totalBlocksMined; 
 
    std::vector<MinerDAPStats> minerStats; 
 
    DAPRecord()
        : dapIndex(0), startHeight(0), endHeight(0),
          startTime(0), endTime(0), difficultyRate(0),
          totalBlocksOnChain(0), totalBlocksMined(0) {}
};

struct AttackerDAPMetrics {
    // Per-DAP values
    double atkRevenueThisDAP;
    double atkBlocksThisDAP;
    double honestCounterfactualThisDAP;
    double revenueAdvantageThisDAP;
    double cumulativeAtkRevenue;
    double cumulativeHonestCounterfactual;
    double cumulativeRevenueAdvantage;   
 

    double rrr; 
 
    AttackerDAPMetrics()
        : atkRevenueThisDAP(0), atkBlocksThisDAP(0),
          honestCounterfactualThisDAP(0), revenueAdvantageThisDAP(0),
          cumulativeAtkRevenue(0), cumulativeHonestCounterfactual(0),
          cumulativeRevenueAdvantage(0), rrr(0) {}
};

struct RevenueAdvantagePoint {
    int dapIndex;
    std::vector<AttackerDAPMetrics> attackerMetrics;  // one per attacker
 
    RevenueAdvantagePoint() : dapIndex(0) {}
};

struct BlockRecord {
    BlockHeight height;
    BlockTime   timestamp;
    int         minerId;     
    Value       blockValue;
    BlockRate   secondsPerBlock;
    // per-attacker running totals (indexed by attacker order)
    std::vector<double> atkRevenueThisDAP;
    std::vector<double> cumulativeRA;
};

// to be used by external code to make decisions (e.g., strategy scheduler)
using DAPBoundaryCallback = std::function<void(
    int dapIndex,
    const DAPRecord &record,
    const RevenueAdvantagePoint &point
)>;

class DAPTracker {
    public:

    // Set Up (Note: attackerAlpha is the combined hashrate of all attackers--assumed to be symmetric)
    DAPTracker(int dapLength, size_t numMiners,std::vector<AttackerInfo> attackers, double blockReward, double txFeeRate);

    void flushBlockRolling();

    void reset(BlockRate initialSecondsPerBlock);

    // Core Interfaces

    /// Helpers
    int getCurrentDAP() const {return _currentDAP;}
    int getDAPLength() const {return _dapLength; }

    size_t numAttackers() const { return _attackers.size(); }

    const std::vector<AttackerInfo> &attackers() const { return _attackers; }
    

    const std::vector<DAPRecord> &history() const {return _dapHistory; }


    // Callback helper
    void setDAPBoundaryCallback(DAPBoundaryCallback cb) {
        _boundaryCallback = std::move(cb);
    }

    void setOutputBase(const std::string &base);
    void recordBlock(Blockchain &blockchain, const MinerGroup &minerGroup);

    // Output 
    void printSummary(std::ostream &os) const;

    void finalize(Blockchain &blockchain, const MinerGroup &minerGroup); 

    bool checkAndProcessDAP(Blockchain &blockchain, const MinerGroup &minerGroup); 

    const std::vector<RevenueAdvantagePoint> &revenueAdvantageCurve() const {
    return _revAdvantageCurve;
    }

     ~DAPTracker() {
    if (_blockRollingFile.is_open()) _blockRollingFile.close();
    if (_blockMasterLog.is_open())   _blockMasterLog.close();
    if (_epochRollingFile.is_open()) _epochRollingFile.close();
    if (_epochMasterLog.is_open())   _epochMasterLog.close();
}

    private:

    std::string _outputBase;
    std::deque<BlockRecord>  _blockWindow;   // max 200
    std::deque<DAPRecord>    _epochWindow;   // max 2
   

    std::ofstream _blockRollingFile;
    std::ofstream _blockMasterLog;
    std::ofstream _epochRollingFile;
    std::ofstream _epochMasterLog;

    void recomputeAllDAPs(const Blockchain &blockchain, const MinerGroup &minerGroup);

    void snapShotCosts(const MinerGroup &minerGroup, DAPRecord &record);

    void applyDifficultyAdjustment(Blockchain &blockchain, const DAPRecord &completeDAP);

    void rebuildRevenueAdvantageCurve();

    int _dapLength;
    size_t _numMiners;
    std::vector<AttackerInfo> _attackers;

    double _blockReward;
    double _txFeeRate;
    double _expectedBlockValue;

    int _currentDAP;
    BlockRate _baseSecondsPerBlock;

    std::vector<BlockRecord> _blockMasterBuffer;
    std::vector<double> _prevCostSnapshot;
    std::vector<BlockCount> _prevBlocksMinedSnapshot; 
    std::vector<DAPRecord> _dapHistory;

    std::vector<RevenueAdvantagePoint> _revAdvantageCurve;
    DAPBoundaryCallback _boundaryCallback;
    std::vector<double> _runningAtkRevThisDAP;
    std::vector<double> _runningCumRA;  

    void writeBlockHeader(std::ostream &os) const;
    void writeBlockRow(std::ostream &os, const BlockRecord &r) const;
    void writeEpochHeader(std::ostream &os) const;
    void writeEpochRow(std::ostream &os, const DAPRecord &d,
                    const RevenueAdvantagePoint &pt) const;
    void rewriteBlockRolling();
    void rewriteEpochRolling();

    void pauseForAgent();

   
};



#endif /* dap_tracker_hpp */