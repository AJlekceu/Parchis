#include "aiengine.h"

#include <array>
#include <bitset>
#include <cassert>
#include <memory>
#include <numeric>

#include <QtDebug> //TODO remove
#include <QString> //TODO remove

    //TODO : eliminate repetitive clearDistanceAhead calls or make it o(1)

using namespace parchis;

enum class TransitionKind
{
    None,
    ForwardJump,
    BackwardJump,
    PenEntry
};

struct CommandTree
{
    Command command;
    ConstActionUptr action;
    std::list<std::unique_ptr<CommandTree>> children;
};

struct PositionValue
{
    int playerCount = 0;
    double diffs[sideCount] = {0};
    double sum = 99999999;// //TODO fix?
};

using Score = double;
using CommandSequence = std::list<std::pair<Command, ConstActionUptr>>;

Score playerScore(PositionValue posValue, int player)
{
    Score ret = posValue.diffs[player] - (posValue.sum - posValue.diffs[player]) /
            (posValue.playerCount - 1);
    return ret;
}

AiEngine::AiEngine()
{

}

void assertRelSquareIsLegal(int relSquare)
{
    assert(relSquare >= 0 && relSquare < squaresInMain + pawnsPerPlayer);
}

int nextForwardJump(int relSquare)
{
    assertRelSquareIsLegal(relSquare);

    int ret = ((relSquare + squaresInSide - jumpDistanceFromOrigin) / squaresInSide) *
            squaresInSide + jumpDistanceFromOrigin;

    return ret < squaresInMain ? ret : -1;
}

int nextBackwardJump(int relSquare)
{
    assertRelSquareIsLegal(relSquare);

    int ret = (relSquare + jumpDistanceFromOrigin) / squaresInSide *
            squaresInSide + squaresInSide - jumpDistanceFromOrigin;

    return ret < squaresInMain ? ret : -1;
}

int nextPenEntry(int relSquare)
{
    assertRelSquareIsLegal(relSquare);

    int ret = ((relSquare + squaresInSide - penEntryDistanceFromOrigin) / squaresInSide) *
            squaresInSide + penEntryDistanceFromOrigin;

    return ret < squaresInMain ? ret : -1;
}

int previousForwardJump(int relSquare)
{
    assertRelSquareIsLegal(relSquare);

    if(relSquare >= squaresInMain)
        relSquare = squaresInMain;

    int ret = ((relSquare + squaresInSide - jumpDistanceFromOrigin - 1) / squaresInSide - 1) *
            squaresInSide + jumpDistanceFromOrigin;

    return ret >= 0 ? ret : -1;
}

int previousBackwardJump(int relSquare)
{
    assertRelSquareIsLegal(relSquare);

    if(relSquare >= squaresInMain)
        relSquare = squaresInMain;

    int ret = ((relSquare + jumpDistanceFromOrigin - 1) / squaresInSide - 1) *
            squaresInSide + squaresInSide - jumpDistanceFromOrigin;

    return ret >= 0 ? ret : -1;
}

int previousPenEntry(int relSquare)
{
    assertRelSquareIsLegal(relSquare);

    if(relSquare >= squaresInMain)
        relSquare = squaresInMain;

    int ret = ((relSquare + squaresInSide - penEntryDistanceFromOrigin - 1) / squaresInSide - 1) *
            squaresInSide + penEntryDistanceFromOrigin;

    return ret >= 0 ? ret : -1;
}

std::pair<int, TransitionKind> nextJump(int relSquare)
{
    assertRelSquareIsLegal(relSquare);

    std::pair<int, TransitionKind> retCandidate1
        {nextForwardJump(relSquare), TransitionKind::ForwardJump};
    std::pair<int, TransitionKind> retCandidate2
        {nextBackwardJump(relSquare), TransitionKind::BackwardJump};

    if(retCandidate1.first == -1)
    {
        if(retCandidate2.first == -1)
            return {-1, TransitionKind::None};
        return retCandidate2;
    }

    if(retCandidate2.first == -1)
        return retCandidate1;

    return retCandidate1.first < retCandidate2.first ?
                    retCandidate1 : retCandidate2;
}

std::pair<int, TransitionKind> previousJump(int relSquare)
{
    assertRelSquareIsLegal(relSquare);

    std::pair<int, TransitionKind> retCandidate1
        {previousForwardJump(relSquare), TransitionKind::ForwardJump};
    std::pair<int, TransitionKind> retCandidate2
        {previousBackwardJump(relSquare), TransitionKind::BackwardJump};

    if(retCandidate1.first == -1 && retCandidate2.first == -1)
        return {-1, TransitionKind::None};

    return retCandidate1.first > retCandidate2.first ? retCandidate1 : retCandidate2;
}

std::pair<int, TransitionKind> nextTransition(int relSquare)
{
    assertRelSquareIsLegal(relSquare);

    std::pair<int, TransitionKind> retCandidate1 = nextJump(relSquare);
    std::pair<int, TransitionKind> retCandidate2{nextPenEntry(relSquare), TransitionKind::PenEntry};

    if(retCandidate1.first == -1)
    {
        if(retCandidate2.first == -1)
            return {-1, TransitionKind::None};
        return retCandidate2;
    }

    if(retCandidate2.first == -1)
        return retCandidate1;

    return retCandidate1.first < retCandidate2.first ? retCandidate1 : retCandidate2;
}

std::pair<int, TransitionKind> previousTransition(int relSquare)
{
    assertRelSquareIsLegal(relSquare);

    std::pair<int, TransitionKind> retCandidate1 = previousJump(relSquare);
    std::pair<int, TransitionKind> retCandidate2
        {previousPenEntry(relSquare), TransitionKind::PenEntry};

    if(retCandidate1.first == -1 && retCandidate2.first == -1)
        return {-1, TransitionKind::None};

    return retCandidate1.first > retCandidate2.first ? retCandidate1 : retCandidate2;
}

int clearDistanceAhead(const Game & game, int player, int srcRelSquare, int maxDistance)
{
    int side = game.playerSettings().playerSideMap().at(player);

    assertRelSquareIsLegal(srcRelSquare);
    assert(maxDistance >= 0);

    int distanceToCheck = srcRelSquare + maxDistance >= relSquaresCount ?
                relSquaresCount - srcRelSquare - 1 :
                maxDistance;

    int ret = 0;
    int checkedRelSquare = srcRelSquare;

    for(; ret < distanceToCheck; ++ret)
    {
        ++checkedRelSquare;
        if(game.board().location(
                    Game::relSquareToLocationId(checkedRelSquare, side, player)).
                hasPawns())
            break;
    }

    return ret;
}

double pureDistance1DieChance(int distance)
{
    return distance <= 0 || distance > dieSideCount ? 0 : 11./36;
}

double pureDistance1DieChance(int distance, std::bitset<dieSideCount + 1> skippableDieValues)
{
    if(distance <= 0 || distance > dieSideCount)
        return 0;

    int unskippedValueCount = dieSideCount - distance -
            (skippableDieValues >> (distance + 1)).count(); //TODO get rid of the warning

    return 11./36 - unskippedValueCount * 2./36;
}

double pureDistance2DiceChance(int distance)
{
    constexpr std::array<double, 13> distribution =
    {0, 0, 1./36, 2./36, 3./36, 4./36, 5./36,
     6./36, 5./36, 4./36, 3./36, 2./36, 1./36};

    return distance <= 0 || distance >= distribution.size() ? 0 : distribution[distance];
}

double directDistance1DieChance(const Game & game, int player, int srcRelSquare, int distance,
                                std::bitset<dieSideCount + 1> skippableDieValues)
{
    assertRelSquareIsLegal(srcRelSquare);
    assertRelSquareIsLegal(srcRelSquare + distance);

    double ret = pureDistance1DieChance(distance, skippableDieValues);

    if(ret == 0 || clearDistanceAhead(game, player, srcRelSquare, distance - 1) != distance - 1)
        return 0;
    return ret;
}

double directDistance2DiceChance(const Game & game, int player, int srcRelSquare, int distance)
{
    assertRelSquareIsLegal(srcRelSquare);
    assertRelSquareIsLegal(srcRelSquare + distance);

    double ret = pureDistance2DiceChance(distance);

    if(ret == 0 || clearDistanceAhead(game, player, srcRelSquare, distance - 1) != distance - 1)
        return 0;

    int side = game.playerSettings().playerSideMap().at(player);
    int transitionSrcRelSquare = srcRelSquare;
    TransitionKind transitionKind;
    int distanceToCheckFrom = (distance + 1) / 2;
    int distanceToCheckTo = std::min(distance, dieSideCount + 1);

    //Исключение тех 2к-комбинаций, которые не обеспечат distance из-за переходов в промежутке
    //между srcRelSquare и srcRelSquare + distance

    while(true)
    {
        std::tie(transitionSrcRelSquare, transitionKind) =
                nextTransition(transitionSrcRelSquare);

        if(transitionSrcRelSquare == -1)
            break;

        int srcToTransitionSrcDistance = transitionSrcRelSquare - srcRelSquare;

        if(srcToTransitionSrcDistance < distanceToCheckFrom)
            continue;
        if(srcToTransitionSrcDistance >= distanceToCheckTo)
            break;

        bool transitionBlocked = false;

        if(transitionKind != TransitionKind::PenEntry)
        {
            LocationId transitionSrcLocationId =
                    Game::relSquareToLocationId(transitionSrcRelSquare, side, player);
            LocationId transitionDestLocationId =
                    {{Section::Kind::Main},
                     Game::jumpDestinationSquare(transitionSrcLocationId.square)};
            const Location & transitionDestLocation =
                    game.board().location(transitionDestLocationId);

            if(transitionDestLocation.hasPawns(player))
                transitionBlocked = true;
        }

        if(!transitionBlocked)
        {
            if(srcToTransitionSrcDistance == distance / 2)
                ret -= 1./36;
            else
                ret -= 2./36;
        }
    }

    assert(ret >= 0);

    return ret;
}

double distance1DieChance(const Game & game, int player, int srcRelSquare, int distance,
                          std::bitset<dieSideCount + 1> skippableDieValues)
{
    assertRelSquareIsLegal(srcRelSquare);
    assertRelSquareIsLegal(srcRelSquare + distance);

    double ret = directDistance1DieChance(game, player, srcRelSquare, distance, skippableDieValues);
    int destRelSquare = srcRelSquare + distance;
    int jumpSrcRelSquare = destRelSquare > squaresInMain ?
                -1 : Game::jumpDestinationSquare(destRelSquare);

    if(jumpSrcRelSquare != -1)
    {
        int side = game.playerSettings().playerSideMap().at(player);
        LocationId jumpSrcLocationId = Game::relSquareToLocationId(jumpSrcRelSquare, side, player);

        if(!game.board().location(jumpSrcLocationId).hasPawns())
            ret += directDistance1DieChance(game, player, srcRelSquare,
                                            jumpSrcRelSquare - srcRelSquare, skippableDieValues);
    }

    return ret;
}

double distance2DiceChance(const Game & game, int player, int srcRelSquare, int distance)
{
    struct jumpInfo
    {
        int srcToJumpSrcDistance;
        int jumpDestRelSquare;
    };

    assertRelSquareIsLegal(srcRelSquare);
    assertRelSquareIsLegal(srcRelSquare + distance);

    double ret = 0;
    int side = game.playerSettings().playerSideMap().at(player);
    int destRelSquares[2];
    std::vector<jumpInfo> jumpInfos;

    destRelSquares[0] = srcRelSquare + distance;
    destRelSquares[1] = destRelSquares[0] > squaresInMain ?
                -1 : Game::jumpDestinationSquare(destRelSquares[0]);

    if(destRelSquares[1] != -1 &&
            game.board().location(Game::relSquareToLocationId(destRelSquares[1], side, player)).
            hasPawns())
        destRelSquares[1] = -1;

    int jumpSrcRelSquareCandidate = nextJump(srcRelSquare).first;
    int distanceToCheck = clearDistanceAhead(game, player, srcRelSquare, dieSideCount + 1);

    while(jumpSrcRelSquareCandidate != -1 &&
          jumpSrcRelSquareCandidate - srcRelSquare < distanceToCheck)
    {
        LocationId jumpSrcLocationId =
                Game::relSquareToLocationId(jumpSrcRelSquareCandidate, side, player);
        LocationId jumpDestLocationId = {{Section::Kind::Main},
                               Game::jumpDestinationSquare(jumpSrcLocationId.square)};
        const Location & jumpDestLocation = game.board().location(jumpDestLocationId);

        if(!jumpDestLocation.hasPawns())
        {
            int jumpDestRelSquare = Game::locationIdToRelSquare(jumpDestLocationId, side);

            jumpInfos.push_back({jumpSrcRelSquareCandidate - srcRelSquare, jumpDestRelSquare});
        }

        jumpSrcRelSquareCandidate = nextJump(jumpSrcRelSquareCandidate).first;
    }

    for(int destRelSquare : destRelSquares)
    {
        if(destRelSquare == -1)
            break;
        ret += directDistance2DiceChance(game, player, srcRelSquare, destRelSquare - srcRelSquare);

        for(auto [srcToJumpSrcDistance, jumpDestRelSquare] : jumpInfos)
        {
            int jumpDestToDestDistance = destRelSquare - jumpDestRelSquare;

            if(jumpDestToDestDistance > 0 && srcToJumpSrcDistance >= jumpDestToDestDistance &&
                    clearDistanceAhead(game, player, jumpDestRelSquare,
                                       jumpDestToDestDistance - 1) == jumpDestToDestDistance - 1)
            {
                if(srcToJumpSrcDistance == jumpDestToDestDistance)
                    ret += 1./36;
                else
                    ret += 2./36;
            }
        }
    }

    return ret;
}

double distanceChance(const Game & game, int player, int fromRelSquare, int distance,
                      std::bitset<dieSideCount + 1> skippableDieValues)
{
    return distance1DieChance(game, player, fromRelSquare, distance, skippableDieValues) +
            distance2DiceChance(game, player, fromRelSquare, distance);
}

std::unique_ptr<CommandTree> buildCommandTree(Game & game)
{
    std::unique_ptr<CommandTree> ret = std::make_unique<CommandTree>();
    auto commandActionPairs = game.availableCommands();

    ret->command = Command{Command::Kind::Skip};

    if(!commandActionPairs.empty() &&
            commandActionPairs.at(0).first != Command{Command::Kind::RollDice})
    {
        for(auto & [command, action] : commandActionPairs)
        {
            ActionUptr inverseAction = game.takeAction(*action);
            ret->children.emplace_back(buildCommandTree(game));
            ret->children.back()->command = command;
            ret->children.back()->action = std::move(action);
            game.takeAction(*inverseAction);
        }
    }

    return ret;
}

PositionValue evaluatePosition(const Game & game)
{
    const double pawnInMainSafeBonusTerm = 2;
    const double pawnInPenTerm[sideCount][squaresInPen] = {{-12, -7, -2},
                                                           {0, 5, 10},
                                                           {12, 17, 22},
                                                           {24, 29, 34}};
    const double pawnInNestTerm = -6;
    const double pawnInCaptivityTerm = -15;
    const double pawnInCaptivity2Term = 5;
    const double pawnInHouseTerm[pawnsPerPlayer] = {48, 49.5, 51, 52.5, 54};

    PositionValue ret;

    ret.playerCount = game.playerSettings().playerCount();

    for(const auto & [pawnId, pawn] : game.board().pawns())
    {
        const int & player = pawnId.player;
        const int & side = game.playerSettings().playerSideMap().at(player);
        const LocationId & locationId = pawn.locationId;
        int relSide;

        switch(locationId.section.kind)
        {
        case Section::Kind::Main:
            ret.diffs[player] += Game::mainSquareToRelSquare(locationId.square, side);
            if(Game::isLocationSafe(locationId))
                ret.diffs[player] += pawnInMainSafeBonusTerm;
            break;
        case Section::Kind::Pen:
            relSide = Game::sideToRelSide(locationId.section.index, side);

            ret.diffs[player] += pawnInPenTerm[relSide][locationId.square];
            break;
        case Section::Kind::Nest:
            ret.diffs[player] += pawnInNestTerm;
            break;
        case Section::Kind::Captivity:
            ret.diffs[player] += pawnInCaptivityTerm;
            ret.diffs[locationId.section.index] += pawnInCaptivity2Term;
            break;
        case Section::Kind::House:
            ret.diffs[player] += pawnInHouseTerm[locationId.square];
            break;
        }
    }

    ret.sum = std::accumulate(std::begin(ret.diffs), std::begin(ret.diffs) + ret.playerCount, 0.);
    return ret;
}

std::pair<CommandSequence, PositionValue>
        chooseCommandSequenceAux(Game & game, CommandTree & commandTree)
{
    static QString indent = "";
    CommandSequence bestSequence;
    PositionValue bestPosValue;

    bestPosValue.playerCount = game.playerSettings().playerCount();

    if(commandTree.children.empty())
    {
        bestPosValue = evaluatePosition(game);
    }
    else
    {
        int player = game.playerActing();

        for(const auto & child : commandTree.children)
        {
            ActionUptr inverseAction = game.takeAction(*child->action);

            indent = indent + "  ";
            auto [sequence, posValue] = chooseCommandSequenceAux(game, *child);

            if(playerScore(posValue, player) > playerScore(bestPosValue, player))
            {
                bestSequence = std::move(sequence);
                bestPosValue = posValue;
            }

            game.takeAction(*inverseAction);
        }
    }

    const QVector<QString> commandKinds
        = {"Skip", "RollDice", "MovePawn", "Birth", "Ransom"}; //TODO remove
    qDebug() << indent << commandKinds[static_cast<int>(commandTree.command.kind)] << commandTree.command.param << "|" << playerScore(bestPosValue, 0) << playerScore(bestPosValue, 1) << playerScore(bestPosValue, 2) << playerScore(bestPosValue, 3); //TODO remove
    indent.chop(2);

    bestSequence.push_front({commandTree.command, std::move(commandTree.action)});
    return {std::move(bestSequence), bestPosValue};
}

CommandSequence chooseCommandSequence(Game & game)
{
    std::unique_ptr<CommandTree> commandTree = buildCommandTree(game);

    CommandSequence sequence = chooseCommandSequenceAux(game, *commandTree).first;
    qDebug() << "-------";//TODO remove

    sequence.pop_front();
    return sequence;
}
