//
//  game_ttp.hpp
//  BlockSim
//
#ifndef game_ttp_hpp
#define game_ttp_hpp
 
#include "game.hpp"

class DAPTracker;

GameResult runGameTTP(MinerGroup &minerGroup,
                      Blockchain &blockchain,
                      GameSettings gameSettings,
                      DAPTracker &dapTracker);

#endif /* game_ttp_hpp */