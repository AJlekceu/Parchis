#include "game.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>

#include "actions.h"

//TODO: подумать насчёт uint-ов, сделать проверку на запредельные значения аргументов во всем проекте

namespace parchis
{

static std::vector<PawnId> pawnIds(int playerCount);
static std::vector<LocationId> locationIds(int playerCount);
static int commandCost(Command::Kind kind);
static LocationId nextLocation(LocationId locationId, int side, int player, int distance);
static LocationId nextLocation(LocationId locationId, int side, int player);
static std::pair<int, int> nextActionAndTurn(const PlayerSettings & playerSettings,
                                             int playerActing, int playerWithTurn,
                                             const Dice<dieCount> & dice, int diceUsed,
                                             Command command);
static ActionUptr skipActionUnchecked(const Game & game);
static void takeLocation(const Game & game, ActionPawnTired::Container & tiredPawns,
                         ActionPawnRelocation::Container & pawnRelocations,
                         PawnId movedPawnId, LocationId destLocationId);
static void penShift(const Game & game, ActionPawnTired::Container & tiredPawns,
                     ActionPawnRelocation::Container & pawnRelocations,
                     PawnId movedPawnId, LocationId destLocationId);
static void completeSubactions(const PlayerSettings & playerSettings,
                               int playerActing, int playerWithTurn,
                               const Dice<dieCount> & dice, int diceUsed,
                               Command command, ActionComplex::Container & subactions);
static std::pair<SkipResultCode, ActionUptr> createSkipAction(const Game & game);
static std::pair<RollDiceResultCode, ActionUptr> createRollDiceAction(const Game & game,
        const DiceGenerator<dieCount> & diceGenerator);
static std::pair<MovePawnResultCode, ActionUptr> createMovePawnAction(const Game & game,
                                                                      int pawnIndex);
static std::pair<BirthResultCode, ActionUptr> createBirthAction(const Game & game);
static std::pair<RansomResultCode, ActionUptr> createRansomAction(const Game & game,
                                                                  int captorPlayer);

static std::vector<PawnId> pawnIds(int playerCount)
{
    std::vector<PawnId> ret;

    for(int player = 0; player < playerCount; ++player)
    {
        for(int index = 0; index < pawnsPerPlayer; ++index)
            ret.emplace_back(player, index);
    }

    return ret;
}

static std::vector<LocationId> locationIds(int playerCount)
{
    std::vector<LocationId> ret;

    for(int square = 0; square < squaresInMain; ++square)
        ret.push_back({{Section::Kind::Main}, square});

    for(int index = 0; index < sideCount; ++index)
    {
        for(int square = 0; square < squaresInPen; ++square)
            ret.push_back({{Section::Kind::Pen, index}, square});
    }

    for(int player = 0; player < playerCount; ++player)
    {
        ret.push_back({{Section::Kind::Captivity, player}});
        for(int square = 0; square < pawnsPerPlayer; ++square)
            ret.push_back({{Section::Kind::House, player}, square});
    }

    ret.push_back({{Section::Kind::Nest}});
    return ret;
}

static int commandCost(Command::Kind kind)
{
    switch(kind)
    {
    case Command::Kind::RollDice:
        return -dieCount;
    case Command::Kind::Ransom:
        return 0;
    default:
        return 1;
    }
}

static LocationId nextLocation(LocationId locationId, int side, int player, int distance)
{
    int relSquare = Game::locationIdToRelSquare(locationId, side);

    if(relSquare == -1)
        throw std::invalid_argument("Not applicable for this kind of section");

    int nextRelSquare = relSquare + distance;
    LocationId ret = Game::relSquareToLocationId(nextRelSquare, side, player);

    if(ret.section.kind == Section::Kind::Nest)
        throw std::invalid_argument("End location does not exist");
    return ret;
}

static LocationId nextLocation(LocationId locationId, int side, int player)
{
    if(locationId.section.kind == Section::Kind::Pen)
    {
        if(locationId.square < squaresInPen - 1)
            return {locationId.section, locationId.square + 1};
        return {{Section::Kind::Main}, Game::penExitSquare(locationId.section.index)};
    }

    return nextLocation(locationId, side, player, 1);
}

static std::pair<int, int> nextActionAndTurn(const PlayerSettings & playerSettings,
                                             int /*playerActing*/, int playerWithTurn,
                                             const Dice<dieCount> & dice, int diceUsed,
                                             Command command)
{
    if(diceUsed == dieCount - commandCost(command.kind))
    {
        bool isPlayerFinished
                = playerSettings.playersFinishedMap().at(playerWithTurn);

        auto compareDiceFun = [firstDie = dice.front()](int die) { return die == firstDie; };

        int nextPlayer = (!isPlayerFinished &&
                          std::all_of(dice.cbegin(), dice.cend(), compareDiceFun) ?
                          playerWithTurn :
                          playerSettings.nextPlayerPlaying(playerWithTurn));

        return {nextPlayer, nextPlayer};
    }

    if(command.kind == Command::Kind::Ransom)
        return {command.param, playerWithTurn};
    return {playerWithTurn, playerWithTurn};
}

static ActionUptr skipActionUnchecked(const Game & game)
{
    ActionComplex::Container subactions;
    completeSubactions(game.playerSettings(), game.playerActing(),
                       game.playerWithTurn(), game.dice(), game.diceUsed(),
                       {Command::Kind::Skip}, subactions);
    return Action::create<ActionComplex>(std::move(subactions));
}

static void takeLocation(const Game & game, ActionPawnTired::Container & tiredPawns,
                         ActionPawnRelocation::Container & pawnRelocations,
                         PawnId movedPawnId, LocationId destLocationId)
{
    //TODO : consider inversing the order of relocations (if I decide to make Location class
    //without std::set)
    const Location & destLocation = game.board().location(destLocationId);

    pawnRelocations.emplace_back(movedPawnId, destLocationId);

    if(destLocation.hasPawns())
    {
        LocationId captivityId{{Section::Kind::Captivity, movedPawnId.player}};

        tiredPawns.emplace_back(movedPawnId, true);

        for(PawnId capturedPawnId : destLocation.pawnIds)
        {
            tiredPawns.emplace_back(capturedPawnId, false);
            pawnRelocations.emplace_back(capturedPawnId, captivityId);
        }
    }
}

static void penShift(const Game & game, ActionPawnTired::Container & tiredPawns,
                     ActionPawnRelocation::Container & pawnRelocations,
                     PawnId movedPawnId, LocationId destLocationId)
{
    //TODO : consider inversing the order of relocations (if I decide to make Location class
    //without std::set
    PawnId pawnId = movedPawnId;
    int playerSide = game.playerSettings().playerSideMap().at(pawnId.player);

    while(true)
    {
        const Location & destLocation = game.board().location(destLocationId);

        if(!destLocation.hasPawns() || (!Game::isLocationSafe(destLocationId) &&
                                        !destLocation.hasPawns(pawnId.player)))
        {
            takeLocation(game, tiredPawns, pawnRelocations, pawnId, destLocationId);
            break;
        }

        pawnRelocations.emplace_back(pawnId, destLocationId);
        pawnId = destLocation.firstPawnId();
        destLocationId = nextLocation(destLocationId, playerSide, pawnId.player);
    }
}

static void completeSubactions(const PlayerSettings & playerSettings,
                               int playerActing, int playerWithTurn,
                               const Dice<dieCount> & dice, int diceUsed,
                               Command command, ActionComplex::Container & subactions)
{
    auto [nextPlayerActing, nextPlayerWithTurn] =
            nextActionAndTurn(playerSettings, playerActing, playerWithTurn, dice,
            diceUsed, command);

    subactions.emplace_back(Action::create<ActionDiceUsed>(diceUsed +
                                                           commandCost(command.kind)));
    subactions.emplace_back(Action::create<ActionActionAndTurn>(nextPlayerActing,
                                                                nextPlayerWithTurn));
}

std::pair<SkipResultCode, ActionUptr> createSkipAction(const Game & game)
{
    if(game.isFinished())
        return {SkipResultCode::FailGameFinished, nullptr};
    if(game.diceUsed() == dieCount)
        return {SkipResultCode::FailAllDiceUsed, nullptr};

    auto commands = game.availableCommands();

    assert(!commands.empty());

    if(commands.at(0).first == Command{Command::Kind::Skip})
        return {SkipResultCode::Success, std::move(commands.at(0).second)};
    return {SkipResultCode::FailSomeCommandsAvailable, nullptr};
}

std::pair<RollDiceResultCode, ActionUptr> createRollDiceAction(const Game & game,
    const DiceGenerator<dieCount> & diceGenerator)
{
    if(game.isFinished())
        return {RollDiceResultCode::FailGameFinished, nullptr};
    if(game.playerSettings().playersFinishedMap().at(game.playerActing()))
        return {RollDiceResultCode::FailPlayerFinished, nullptr};
    if(game.diceUsed() < dieCount)
        return {RollDiceResultCode::FailNotAllDiceUsed, nullptr};

    Dice<dieCount> newDice = diceGenerator();

    ActionPawnTired::Container tiredPawns;
    ActionComplex::Container subactions;

    if constexpr(dieCount > 2)
    {
        std::sort(newDice.begin(), newDice.end(), std::greater<>{});
    }
    else if constexpr(dieCount == 2)
    {
        Dice<dieCount>::value_type & die0 = newDice[0], & die1 = newDice[1];
        if(die0 < die1)
            std::swap(die0, die1);
    }

    subactions.emplace_back(Action::create<ActionDice>(std::move(newDice)));

    for(PawnId tiredPawnId : game.board().tiredPawnIds())
        tiredPawns.emplace_back(tiredPawnId, false);
    if(!tiredPawns.empty())
        subactions.emplace_back(Action::create<ActionPawnTired>(std::move(tiredPawns)));

    completeSubactions(game.playerSettings(), game.playerActing(), game.playerWithTurn(),
                       game.dice(), game.diceUsed(), {Command::Kind::RollDice}, subactions);
    return {RollDiceResultCode::Success, Action::create<ActionComplex>(std::move(subactions))};
}

std::pair<MovePawnResultCode, ActionUptr> createMovePawnAction(const Game & game,
                                                               int pawnIndex)
{
    if(game.isFinished())
        return {MovePawnResultCode::FailGameFinished, nullptr};
    if(game.playerSettings().playersFinishedMap().at(game.playerActing()))
        return {MovePawnResultCode::FailPlayerFinished, nullptr};
    if(game.diceUsed() == dieCount)
        return {MovePawnResultCode::FailAllDiceUsed, nullptr};

    PawnId movedPawnId{game.playerActing(), pawnIndex};
    const Pawn & movedPawn = game.board().pawn(movedPawnId); //TODO: try-catch? if exception is thrown, return appropriate MovePawnResultCode

    if(movedPawn.tired)
        return {MovePawnResultCode::FailTired, nullptr};

    const LocationId & srcLocationId = movedPawn.locationId;

    if(srcLocationId.section.kind == Section::Kind::Nest)
        return {MovePawnResultCode::FailInNest, nullptr};
    if(srcLocationId.section.kind == Section::Kind::Captivity)
        return {MovePawnResultCode::FailCaptured, nullptr};

    ActionPawnTired::Container tiredPawns;
    ActionPawnRelocation::Container pawnRelocations;
    int dieValue = game.dice().at(game.diceUsed());
    int playerSide = game.playerSettings().playerSideMap().at(game.playerActing());
    bool playerFinished = false;

    if(srcLocationId.section.kind == Section::Kind::Pen)
    {
        if(dieValue != penDieValues.at(srcLocationId.square))
            return {MovePawnResultCode::FailInPenAndWrongDie, nullptr};
        penShift(game, tiredPawns, pawnRelocations, movedPawnId,
                 nextLocation(srcLocationId, playerSide, game.playerActing()));
    }
    else
    {
        LocationId destLocationId;

        try
        {
            destLocationId = nextLocation(srcLocationId, playerSide, game.playerActing(),
                                          dieValue);
        }
        catch(std::invalid_argument)
        {
            return {MovePawnResultCode::FailEndLocationNonexistent, nullptr};
        }

        const Location & destLocation = game.board().location(destLocationId);

        if(destLocation.hasPawns(game.playerActing()))
            return {MovePawnResultCode::FailEndLocationWithFriendlyPawn, nullptr};
        if(destLocation.hasPawns() && destLocationId.section.kind == Section::Kind::Main
                && Game::isLocationSafe(destLocationId))
            return {MovePawnResultCode::FailEndLocationSafeWithEnemyPawn, nullptr};

        for(int i = 1; i < dieValue; ++i)
        {
            LocationId pathLocationId = nextLocation(srcLocationId, playerSide,
                                                     game.playerActing(), i);
            const Location & pathLocation = game.board().location(pathLocationId);

            if(pathLocation.hasPawns())
                return {MovePawnResultCode::FailPathBlocked, nullptr};
        }

        takeLocation(game, tiredPawns, pawnRelocations, movedPawnId, destLocationId);

        if(destLocationId.section.kind == Section::Kind::Main)
        {
            if(!destLocation.hasPawns())
            {
                if(Game::isSquarePenEntry(destLocationId.square))
                {
                    int side = Game::squareSide(destLocationId.square);

                    penShift(game, tiredPawns, pawnRelocations, movedPawnId,
                             {{Section::Kind::Pen, side}, 0});
                }
                else
                {
                    int jumpDestSquare = Game::jumpDestinationSquare(destLocationId.square);

                    if(jumpDestSquare != -1)
                    {
                        LocationId jumpDestLocationId =
                            {{Section::Kind::Main}, jumpDestSquare};
                        const Location & jumpDestLocation =
                                game.board().location(jumpDestLocationId);

                        if(!jumpDestLocation.hasPawns(game.playerActing()))
                            takeLocation(game, tiredPawns, pawnRelocations, movedPawnId,
                                         jumpDestLocationId);
                    }
                }
            }
        }
        else if(destLocationId.square == 0)
        {
            playerFinished = true;

            for(int square = 1; square < pawnsPerPlayer; ++square)
            {
                if(!game.board().location({{Section::Kind::House,
                                          game.playerActing()}, square}).hasPawns())
                {
                    playerFinished = false;
                    break;
                }
            }
        }
    }

    ActionComplex::Container subactions;

    if(!tiredPawns.empty())
        subactions.emplace_back(Action::create<ActionPawnTired>(std::move(tiredPawns)));
    if(!pawnRelocations.empty())
        subactions.emplace_back(Action::create<ActionPawnRelocation>
                                (std::move(pawnRelocations)));

    std::unique_ptr<PlayerSettings> newPlayerSettings;

    if(playerFinished)
    {
        newPlayerSettings = std::make_unique<PlayerSettings>(game.playerSettings());
        newPlayerSettings->setPlayerFinished(game.playerActing(), true);
    }

    completeSubactions(playerFinished ? *newPlayerSettings : game.playerSettings(),
                       game.playerActing(), game.playerWithTurn(), game.dice(),
                       game.diceUsed(), {Command::Kind::MovePawn, pawnIndex}, subactions);

    if(playerFinished)
    {
        subactions.emplace_back(Action::create<ActionPlayerFinished>
                                (game.playerActing(), true));

        if(game.playerSettings().playersPlayingSet().size() <= 2)
        {
            subactions.emplace_back(Action::create<ActionGameFinished>(true));
        }
    }

    return {MovePawnResultCode::Success, Action::create<ActionComplex>(std::move(subactions))};
}

std::pair<BirthResultCode, ActionUptr> createBirthAction(const Game & game)
{
    if(game.isFinished())
        return {BirthResultCode::FailGameFinished, nullptr};
    if(game.playerSettings().playersFinishedMap().at(game.playerActing()))
        return {BirthResultCode::FailPlayerFinished, nullptr};
    if(game.diceUsed() == dieCount)
        return {BirthResultCode::FailAllDiceUsed, nullptr};
    if(game.playerWithTurn() != game.playerActing())
        return {BirthResultCode::FailOnlyAllowedOnYourTurn, nullptr};

    int dieValue = game.dice().at(game.diceUsed());

    if(dieValue != birthDieValue)
        return {BirthResultCode::FailWrongDie, nullptr};

    LocationId nestId{{Section::Kind::Nest}};
    const Location & nest = game.board().location(nestId);
    int bornPawnIndex = nest.findPawnIndex(game.playerActing());

    if(bornPawnIndex == -1)
        return {BirthResultCode::FailNoPawns, nullptr};

    LocationId originLocationId = Game::relSquareToLocationId(0,
            game.playerSettings().playerSideMap().at(game.playerActing()),
            game.playerActing());
    const Location & originLocation = game.board().location(originLocationId);

    if(originLocation.hasPawns(game.playerActing()))
        return {BirthResultCode::FailEndLocationWithFriendlyPawn, nullptr};
    if(originLocation.hasPawns() && Game::isLocationSafe(originLocationId))
        return {BirthResultCode::FailEndLocationSafeWithEnemyPawn, nullptr};

    PawnId bornPawnId{game.playerActing(), bornPawnIndex};
    ActionPawnTired::Container tiredPawns;
    ActionPawnRelocation::Container pawnRelocations;
    ActionComplex::Container subactions;

    takeLocation(game, tiredPawns, pawnRelocations, bornPawnId, originLocationId);

    if(!tiredPawns.empty())
        subactions.emplace_back(Action::create<ActionPawnTired>(std::move(tiredPawns)));
    if(!pawnRelocations.empty())
        subactions.emplace_back(Action::create<ActionPawnRelocation>
                                (std::move(pawnRelocations)));

    completeSubactions(game.playerSettings(), game.playerActing(), game.playerWithTurn(),
                       game.dice(), game.diceUsed(), {Command::Kind::Birth}, subactions);
    return {BirthResultCode::Success, Action::create<ActionComplex>(std::move(subactions))};
}

std::pair<RansomResultCode, ActionUptr> createRansomAction(const Game & game,
                                                           int captorPlayer)
{
    if(game.isFinished())
        return {RansomResultCode::FailGameFinished, nullptr};
    if(game.playerSettings().playersFinishedMap().at(game.playerActing()))
        return {RansomResultCode::FailPlayerFinished, nullptr};
    if(game.diceUsed() == dieCount)
        return {RansomResultCode::FailAllDiceUsed, nullptr};
    if(game.playerActing() == captorPlayer)
        return {RansomResultCode::FailSelf, nullptr};
    if(captorPlayer >= game.playerSettings().playerCount()) //TODO : check if captorPlayer < 0? do it in other places like this as well
        return {RansomResultCode::FailPlayerDoesNotExist, nullptr};
    if(game.playerWithTurn() != game.playerActing())
        return {RansomResultCode::FailOnlyAllowedOnYourTurn, nullptr};

    int dieValue = game.dice().at(game.diceUsed());

    if(dieValue != ransomDieValue)
        return {RansomResultCode::FailWrongDie, nullptr};

    LocationId captivityId{{Section::Kind::Captivity, captorPlayer}};
    const Location & captivity = game.board().location(captivityId);
    int ransomedPawnIndex = captivity.findPawnIndex(game.playerActing());

    if(ransomedPawnIndex == -1)
        return {RansomResultCode::FailNoPawns, nullptr};

    PawnId ransomedPawnId{game.playerActing(), ransomedPawnIndex};
    LocationId nestId{{Section::Kind::Nest}};
    ActionComplex::Container subactions;

    subactions.emplace_back(Action::create<ActionPawnRelocation>(PawnRelocation{ransomedPawnId,
                                                                                nestId}));
    completeSubactions(game.playerSettings(), game.playerActing(), game.playerWithTurn(),
                       game.dice(), game.diceUsed(), {Command::Kind::Ransom, captorPlayer},
                       subactions);
    return {RansomResultCode::Success, Action::create<ActionComplex>(std::move(subactions))};
}

std::pair<CommandResultCode, ActionUptr> Game::createCommandAction(Command command) const
{
    switch(command.kind)
    {
    case Command::Kind::Skip:
        return createSkipAction(*this);
    case Command::Kind::RollDice:
        return createRollDiceAction(*this, _diceGenerator);
    case Command::Kind::MovePawn:
        return createMovePawnAction(*this, command.param);
    case Command::Kind::Birth:
        return createBirthAction(*this);
    case Command::Kind::Ransom:
        return createRansomAction(*this, command.param);
    default:
        return {};
    }
}

std::vector<std::pair<Command, ActionUptr>> Game::availableCommands() const
{
    std::vector<std::pair<Command, ActionUptr>> ret;

    if(!isFinished())
    {
        bool isPlayerFinished = playerSettings().playersFinishedMap().at(playerActing());

        if(!isPlayerFinished)
        {
            auto [rollDiceResultCode, rollDiceAction] = createRollDiceAction(*this, _diceGenerator);

            if(rollDiceResultCode == RollDiceResultCode::Success)
                ret.emplace_back(Command{Command::Kind::RollDice}, std::move(rollDiceAction));
            else
            {
                for(int pawnIndex = 0; pawnIndex < pawnsPerPlayer; ++pawnIndex)
                {
                    auto [movePawnResultCode, movePawnAction] = createMovePawnAction(*this,
                            pawnIndex);

                    if(movePawnResultCode == MovePawnResultCode::Success)
                        ret.emplace_back(Command{Command::Kind::MovePawn, pawnIndex},
                                         std::move(movePawnAction));
                }

                auto [birthResultCode, birthAction] = createBirthAction(*this);

                if(birthResultCode == BirthResultCode::Success)
                    ret.emplace_back(Command{Command::Kind::Birth}, std::move(birthAction));

                for(int player = 0; player < sideCount; ++player)
                {
                    auto [ransomResultCode, ransomAction] = createRansomAction(*this, player);

                    if(ransomResultCode == RansomResultCode::Success)
                    {
                        ret.emplace_back(Command{Command::Kind::Ransom, player},
                                         std::move(ransomAction));
                    }
                }
            }
        }

        if(ret.empty() || isPlayerFinished)
            ret.emplace_back(Command{Command::Kind::Skip}, skipActionUnchecked(*this));
    }

    return ret;
}

void Game::startOver(std::vector<int> playerSideMap)
{
    _gameState.reset();
    _gameState.isFinished = playerSideMap.size() < 2;
    _gameState.playerSettings.startOver(std::move(playerSideMap));
    _gameState.board.initialize(pawnIds(playerSettings().playerCount()),
                                locationIds(playerSettings().playerCount()));
    _gameState.playerActing = 0;
    _gameState.playerWithTurn = 0;
}

ActionUptr Game::takeAction(const Action & action)
{
    ActionUptr inverseAction = action.commit(_gameState);

    return inverseAction;
}

}
