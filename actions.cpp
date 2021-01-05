#include "actions.h"

#include "gamestate.h"

namespace parchis
{

ActionUptr ActionComplex::commit(GameState & gameState) const
{
    Container inverseSubActions;

    for(const auto & subAction : subActions())
        inverseSubActions.emplace_front(subAction->commit(gameState));

    return Action::create<ActionComplex>(std::move(inverseSubActions));
}

ActionPawnTired::ActionPawnTired(PawnId pawnId, bool tired)
{
    Container params;

    params.emplace_back(pawnId, tired);
    _params = std::make_shared<Container>(std::move(params));
}

ActionUptr ActionPawnTired::commit(GameState & gameState) const
{
    Container inverseParams;

    for(const auto & [pawnId, tired] : params())
    {
        inverseParams.emplace_front(pawnId, gameState.board.pawn(pawnId).tired);
        gameState.board.setPawnTired(pawnId, tired);
    }

    return Action::create<ActionPawnTired>(std::move(inverseParams));
}

ActionPawnRelocation::ActionPawnRelocation(const PawnRelocation & pawnRelocation)
{
    Container pawnRelocations;

    pawnRelocations.emplace_back(pawnRelocation);
    _pawnRelocations = std::make_shared<Container>(std::move(pawnRelocations));
}

ActionUptr ActionPawnRelocation::commit(GameState & gameState) const
{
    Container inversePawnRelocations;

    for(const auto & [pawnId, toLocationId] : pawnRelocations())
    {
        inversePawnRelocations.emplace_front(pawnId, gameState.board.pawn(pawnId).locationId);
        gameState.board.relocatePawn(pawnId, toLocationId);
    }

    return Action::create<ActionPawnRelocation>(std::move(inversePawnRelocations));
}

ActionUptr ActionDiceUsed::commit(GameState & gameState) const
{
    ActionUptr inverseAction = Action::create<ActionDiceUsed>(gameState.diceUsed);

    gameState.diceUsed = diceUsed();
    return inverseAction;
}

ActionUptr ActionDice::commit(GameState & gameState) const
{
    ActionUptr inverseAction = Action::create<ActionDice>(gameState.dice);

    gameState.dice = dice();
    return inverseAction;
}

ActionUptr ActionActionAndTurn::commit(GameState & gameState) const
{
    ActionUptr inverseAction = Action::create<ActionActionAndTurn>(gameState.playerActing,
                                                                   gameState.playerWithTurn);

    gameState.playerActing = playerTakingAction();
    gameState.playerWithTurn = playerHavingTurn();
    return inverseAction;
}

ActionUptr ActionPlayerFinished::commit(GameState & gameState) const
{
    bool isPlayerFinished = gameState.playerSettings.playersFinishedMap().at(player());
    ActionUptr inverseAction = Action::create<ActionPlayerFinished>(player(), isPlayerFinished);

    gameState.playerSettings.setPlayerFinished(player(), finished());
    return inverseAction;
}

ActionUptr ActionGameFinished::commit(GameState & gameState) const
{
    ActionUptr inverseAction = Action::create<ActionGameFinished>(gameState.isFinished);

    gameState.isFinished = value();
    return inverseAction;
}

}
