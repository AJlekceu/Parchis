#ifndef ACTIONS_H
#define ACTIONS_H

#include "action.h"
#include "board.h"
#include "constants.h"
#include "dice.h"
#include "visitor.h"
#include <list>

namespace parchis
{

class ActionComplex : public Action
{
public:
    LOKI_DEFINE_CONST_VISITABLE()
    CLONABLE()

public:
    using Container = std::list<std::unique_ptr<const Action>>;

    ActionComplex(Container && subActions)
        : _subActions{std::make_shared<Container>(std::move(subActions))} {}

    ActionUptr commit(GameState & gameState) const override;
    const Container & subActions() const { return *_subActions; }

private:
    std::shared_ptr<const Container> _subActions;
};

class ActionPawnTired : public Action
{
public:
    LOKI_DEFINE_CONST_VISITABLE()
    CLONABLE()

public:
    using Container = std::list<std::pair<PawnId, bool>>;

    ActionPawnTired(const Container & params)
        : _params{std::make_shared<Container>(params)} {}
    ActionPawnTired(Container && params)
        : _params{std::make_shared<Container>(std::move(params))} {}
    ActionPawnTired(PawnId pawnId, bool tired);

    ActionUptr commit(GameState & gameState) const override;
    const Container & params() const { return *_params; }

private:
    std::shared_ptr<const Container> _params;
};

struct PawnRelocation
{
    PawnRelocation(PawnId _pawnId, LocationId _toLocationId)
        : pawnId{_pawnId}, toLocationId{_toLocationId} {}

    PawnId pawnId;
    LocationId toLocationId;
};

class ActionPawnRelocation : public Action
{
public:
    LOKI_DEFINE_CONST_VISITABLE()
    CLONABLE()

public:
    using Container = std::list<PawnRelocation>;

    ActionPawnRelocation(const Container & pawnRelocations)
        : _pawnRelocations{std::make_shared<Container>(pawnRelocations)} {}
    ActionPawnRelocation(Container && pawnRelocations)
        : _pawnRelocations{std::make_shared<Container>(std::move(pawnRelocations))} {}
    ActionPawnRelocation(const PawnRelocation & pawnRelocation);
    ActionPawnRelocation(PawnId pawnId, LocationId locationId)
        : ActionPawnRelocation(PawnRelocation{pawnId, locationId}) {}

    ActionUptr commit(GameState & gameState) const override;
    const Container & pawnRelocations() const { return *_pawnRelocations; }

private:
    std::shared_ptr<const Container> _pawnRelocations;
};

class ActionDiceUsed : public Action
{
public:
    LOKI_DEFINE_CONST_VISITABLE()
    CLONABLE()

public:
    ActionDiceUsed(int diceUsed) : _diceUsed{diceUsed} {}

    ActionUptr commit(GameState & gameState) const override;
    int diceUsed() const { return _diceUsed; }

private:
    int _diceUsed;
};

class ActionDice : public Action
{
public:
    LOKI_DEFINE_CONST_VISITABLE()
    CLONABLE()

public:
    ActionDice(const Dice<dieCount> & dice) : _dice{dice} {}
    ActionDice(Dice<dieCount> && dice) : _dice{std::move(dice)} {}

    ActionUptr commit(GameState & gameState) const override;
    const Dice<dieCount> & dice() const { return _dice; }

private:
    Dice<dieCount> _dice;
};

class ActionActionAndTurn : public Action
{
public:
    LOKI_DEFINE_CONST_VISITABLE()
    CLONABLE()

public:
    ActionActionAndTurn(int playerTakingAction, int playerHavingTurn)
        : _playerTakingAction{playerTakingAction},
          _playerHavingTurn{playerHavingTurn} {}

    ActionUptr commit(GameState & gameState) const override;
    int playerTakingAction() const { return _playerTakingAction; }
    int playerHavingTurn() const { return _playerHavingTurn; }

private:
    int _playerTakingAction;
    int _playerHavingTurn;
};

class ActionPlayerFinished : public Action
{
public:
    LOKI_DEFINE_CONST_VISITABLE()
    CLONABLE()

public:
    ActionPlayerFinished(int player, bool finished)
        : _player{player}, _finished{finished} {}

    ActionUptr commit(GameState & gameState) const override;
    int player() const { return _player; }
    bool finished() const { return _finished; }

private:
    int _player;
    bool _finished;
};

class ActionGameFinished : public Action
{
public:
    LOKI_DEFINE_CONST_VISITABLE()
    CLONABLE()

public:
    ActionGameFinished(bool value)
        : _value(value) {}

    ActionUptr commit(GameState & gameState) const override;
    int value() const { return _value; }

private:
    bool _value;
};

class ActionFullVisitor : public Loki::BaseVisitor,
                          public Loki::Visitor<Action, void, true>,
                          public Loki::Visitor<ActionComplex, void, true>,
                          public Loki::Visitor<ActionPawnTired, void, true>,
                          public Loki::Visitor<ActionPawnRelocation, void, true>,
                          public Loki::Visitor<ActionPlayerFinished, void, true>,
                          public Loki::Visitor<ActionDiceUsed, void, true>,
                          public Loki::Visitor<ActionDice, void, true>,
                          public Loki::Visitor<ActionActionAndTurn, void, true>
{ };

}

#endif // ACTIONS_H
