#ifndef GAME_H
#define GAME_H

#include <cstddef>
#include <memory>
#include <utility>
#include <unordered_map>
#include <vector>

#include "action.h"
#include "commandresult.h"
#include "constants.h"
#include "dice.h"
#include "dicegenerators.h"
#include "gamestate.h"

//TODO : make a class or something that allows to temporarily change const game

namespace parchis
{

class Board;
class PlayerSettings;

class Game
{
public:
    Game(DiceGenerator<dieCount> diceGenerator
         = DefaultDiceGenerator<dieSideCount, dieCount>{})
    {
        _diceGenerator = diceGenerator;
        reset();
    }

    bool isFinished() const { return _gameState.isFinished; }
    const Board & board() const { return _gameState.board; }
    int playerActing() const { return _gameState.playerActing; }
    int playerWithTurn() const { return _gameState.playerWithTurn; }
    const PlayerSettings & playerSettings() const { return _gameState.playerSettings; }
    const Dice<dieCount> & dice() const { return _gameState.dice; }
    int diceUsed() const { return _gameState.diceUsed; }

    std::pair<CommandResultCode, ActionUptr> createCommandAction(Command command) const;
    std::vector<std::pair<Command, ActionUptr>> availableCommands() const;

    void startOver(std::vector<int> playerSideMap);
    void reset() { startOver({}); }
    ActionUptr takeAction(const Action & action);
    template<class T> void setDiceGenerator(T value) { _diceGenerator = value; }

    static int squareSide(int mainOrRelSquare)
    {
        return ((mainOrRelSquare + squaresInSide / 2 - 1) % squaresInMain / squaresInSide);
    }

    static int penEntrySquare(int side)
    {
        return relSquareToMainSquare(penEntryDistanceFromOrigin, side);
    }

    static int penExitSquare(int side)
    {
        return relSquareToMainSquare(penExitDistanceFromOrigin, side);
    }

    static int jumpDestinationSquare(int mainOrRelSquare)
    {
        if(mainOrRelSquare % squaresInSide == jumpDistanceFromOrigin)
            return relSquareToMainSquare(squaresInMain - jumpDistanceFromOrigin,
                                              (squareSide(mainOrRelSquare) + 1) %
                                              sideCount);
        if(mainOrRelSquare % squaresInSide == squaresInSide - jumpDistanceFromOrigin)
            return relSquareToMainSquare(jumpDistanceFromOrigin,
                                              (squareSide(mainOrRelSquare) - 1 +
                                               sideCount) % sideCount);
        return -1;
    }

    static bool isSquarePenEntry(int mainOrRelSquare)
    {
        return mainOrRelSquare % squaresInSide == penEntryDistanceFromOrigin;
    }

    static bool isSquarePenExit(int mainOrRelSquare)
    {
        return mainOrRelSquare % squaresInSide == penExitDistanceFromOrigin;
    }

    static bool isLocationSafe(LocationId locationId)
    {
        return locationId.section.kind != Section::Kind::Main ||
               locationId.square % squaresInSide == squaresInSide / 2;
    }

    static int sideToRelSide(int side, int baseSide)
    {
        return (sideCount + side - baseSide) % sideCount;
    }

    static int relSideToSide(int relSide, int baseSide)
    {
        return (relSide + baseSide) % sideCount;
    }

    static int relSquareToMainSquare(int relSquare, int side)
    {
        return (relSquare + squaresInSide * side) % squaresInMain;
    }

    static int mainSquareToRelSquare(int mainSquare, int side)
    {
        return (mainSquare + squaresInSide * (sideCount - side)) % squaresInMain;
    }

    static int locationIdToRelSquare(LocationId locationId, int side)
    {
        if(locationId.section.kind == Section::Kind::Main)
            return Game::mainSquareToRelSquare(locationId.square, side);
        else if(locationId.section.kind == Section::Kind::House)
            return squaresInMain + locationId.square;
        return -1;
    }

    static LocationId relSquareToLocationId(int relSquare, int side, int player)
    {
        if(relSquare < 0)
            return {{Section::Kind::Nest}};
        else if(relSquare < squaresInMain)
            return {{Section::Kind::Main}, relSquareToMainSquare(relSquare, side)};
        else if(relSquare < relSquaresCount)
            return {{Section::Kind::House, player}, relSquare - squaresInMain};
        return {{Section::Kind::Nest}};
    }

private:
    GameState _gameState;
    DiceGenerator<dieCount> _diceGenerator;
};

}

#endif // GAME_H
