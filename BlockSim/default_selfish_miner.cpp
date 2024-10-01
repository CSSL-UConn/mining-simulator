//
//  default_selfish_miner.cpp
//  BlockSim
//
//  Created by Harry Kalodner on 6/6/16.
//  Copyright © 2016 Harry Kalodner. All rights reserved.
//

#include "default_selfish_miner.hpp"
#include "block.hpp"
#include "blockchain.hpp"
#include "miner.hpp"
#include "selfish_miner.hpp"
#include "default_miner.hpp"
#include "logging.h"
#include "utils.hpp"
#include "strategy.hpp"

#include <assert.h>
#include <iostream>
#include <random>


using std::placeholders::_1;
using std::placeholders::_2;

Block &blockToMineOn(const Miner &me, const Blockchain &blockchain, double gamma);

std::unique_ptr<Strategy> createDefaultSelfishStrategy(bool noiseInTransactions, double gamma) {
    auto mineFunc = std::bind(blockToMineOn, _1, _2, gamma);
    auto valueFunc = std::bind(defaultValueInMinedChild, _1, _2, noiseInTransactions);
    
    return std::make_unique<Strategy>("default-selfish", mineFunc, valueFunc);
}

Block &blockToMineOn(const Miner &me, const Blockchain &chain, double gamma) {
    
    std::vector<Block*> possiblities = chain.oldestBlocks(chain.getMaxHeightPub());
    if (possiblities.size() == 1) { //no forking
        return *possiblities[0];
    }
    else if (possiblities.size() == 2) { //fork between the selfish miner and the rest of the network
        //mineHere should already be set to the side of the fork not the selfish miner
        Block *selfishBlock = nullptr;
        Block *defaultBlock = nullptr;
        if (ownBlock(&me, possiblities[0])) {
            defaultBlock = possiblities[0];
            selfishBlock = possiblities[1];
        } else {
            defaultBlock = possiblities[1];
            selfishBlock = possiblities[0];
        }

        COMMENTARY("Next block reward default" << defaultBlock->nextBlockReward() << "\n");
        COMMENTARY("Default Block total tx fees" << defaultBlock->txFeesInChain << "\n");
        COMMENTARY("Default block value (block reward + tx fees) " << defaultBlock->value << "\n");
        COMMENTARY("default block total value"<< defaultBlock->valueInChain << "\n");
        COMMENTARY("Remaining:" << chain.rem(*defaultBlock) << "\n");

        COMMENTARY("Selfish Block total tx fees" << selfishBlock->txFeesInChain << "\n");
        COMMENTARY("Selfish block value (block reward + tx fees) " << selfishBlock->value << "\n");
        COMMENTARY("Selfish block total value"<< selfishBlock->valueInChain << "\n");
        COMMENTARY("Tip" << selfishBlock->tip << "\n");

        COMMENTARY("Remaining:" << chain.rem(*selfishBlock) << "\n");
        //with chance gamma, mine on the selfish miner's block, otherwise not
        if (selectRandomChance() < gamma) {
            return *selfishBlock;
            COMMENTARY("Having to mine on selfish block due to gamma. ");
            
        } else {
            
            return *defaultBlock;
        }
    } else if (possiblities.size() == 3){ // we must choose between multiple selfish miners, and honest miner
    
        // select the two miners that are selfish
        Block *Selfish1Block = nullptr;
        Block *Selfish2Block = nullptr;
        Block *defaultBlock = nullptr;
        if (ownBlock(&me, possiblities[0])) {
            defaultBlock = possiblities[0];
            Selfish1Block = possiblities[1];
            Selfish2Block = possiblities[2];
        } else if (ownBlock(&me, possiblities[1])){
            defaultBlock = possiblities[1];
            Selfish1Block = possiblities[0]; 
            Selfish2Block = possiblities[2]; 
        } else {
            defaultBlock = possiblities[2];
            Selfish1Block = possiblities[0]; 
            Selfish2Block = possiblities[1];  
        }

        
        //with chance gamma, mine on the selfish miner's block, otherwise not
        if (selectRandomChance() < gamma) { // mine on selfish block in general

            std::default_random_engine generator;
            std::uniform_int_distribution<int> distribution(0,1);
            int coin_toss = distribution(generator);
            // models the choice of theta (default uniform)
            return ( coin_toss )?   *Selfish1Block : *Selfish2Block; // coin toss: decide whether we want to mine on which selfish block in particular
            COMMENTARY("Having to mine on selfish block due to gamma. ");
        } else {
            return *defaultBlock;
        }
    } else if (possiblities.size() == 4){ 

        // select the two miners that are selfish
        Block *Selfish1Block = nullptr;
        Block *Selfish2Block = nullptr;
        Block *Selfish3Block = nullptr;
        Block *defaultBlock = nullptr;
        if (ownBlock(&me, possiblities[0])) {
            defaultBlock = possiblities[0];
            Selfish1Block = possiblities[1];
            Selfish2Block = possiblities[2];
            Selfish3Block = possiblities[3]; 
        } else if (ownBlock(&me, possiblities[1])){
            defaultBlock = possiblities[1];
            Selfish1Block = possiblities[0]; 
            Selfish2Block = possiblities[2];
            Selfish3Block = possiblities[3]; 
        } else if (ownBlock(&me, possiblities[2])) {
            defaultBlock = possiblities[2];
            Selfish1Block = possiblities[0]; 
            Selfish2Block = possiblities[1]; 
            Selfish3Block = possiblities[3]; 
        } else {
            defaultBlock = possiblities[3];
            Selfish1Block = possiblities[0]; 
            Selfish2Block = possiblities[1]; 
            Selfish3Block = possiblities[2];  
        }
    
        //with chance gamma, mine on the selfish miner's block, otherwise not
        if (selectRandomChance() < gamma) { // mine on selfish block in general

            
            double choice = selectRandomChance();
            // theta is set to uniform
            if (choice < .33) {
                return *Selfish1Block;
            } else if (choice < .66) {
                return *Selfish2Block;
            } else {
                return *Selfish3Block;
            }
        } else {
            return *defaultBlock;
        }
    
    } else { 
        ERROR(possiblities.size());
        ERROR("\n#####ERROR UNFORSEEN CIRCUMSTANCES IN LOGIC FOR SELFISH MINING SIM###\n\n" << std::endl);
        return *possiblities[0];
    }
}
