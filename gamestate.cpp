#include "gamestate.h"

namespace parchis
{

void GameState::reset()
{
    isFinished = true;
    board.reset();
    playerSettings.reset();
    playerActing = -1;
    playerWithTurn = -1;
    dice.fill(0);
    diceUsed = dieCount;
}

}
