#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "board.h"
#include "constants.h"
#include "dice.h"
#include "playersettings.h"

namespace parchis
{

struct GameState
{
    GameState() { reset(); }

    void reset();

    bool isFinished;
    Board board;
    PlayerSettings playerSettings;
    int playerActing;
    int playerWithTurn;
    Dice<dieCount> dice;
    int diceUsed;
};

}

#endif // GAMESTATE_H
