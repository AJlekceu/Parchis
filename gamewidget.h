#ifndef GAMEWIDGET_H
#define GAMEWIDGET_H

#include <QWidget>
#include <QGraphicsView>

#include <functional>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>

#include "action.h"
#include "commandresult.h"
#include "board.h"
#include "constants.h"
#include "dice.h"
#include "dicegenerators.h"

namespace parchis
{

class PlayerSettings;

}

namespace graphics
{

using parchis::Action;
using parchis::ActionUptr;
using parchis::ConstActionUptr;
using parchis::ActionSptr;
using parchis::ConstActionSptr;
using parchis::Command;
using parchis::CommandResultCode;
using parchis::SkipResultCode;
using parchis::RollDiceResultCode;
using parchis::MovePawnResultCode;
using parchis::BirthResultCode;
using parchis::RansomResultCode;

using parchis::Section;
using parchis::PawnId;
using parchis::LessForPawnIds;
using parchis::LocationId;
using parchis::LessForLocationIds;
using parchis::Pawn;
using parchis::Location;
using parchis::Board;

using parchis::pawnsPerPlayer;
using parchis::dieSideCount;
using parchis::sideCount;
using parchis::dieCount;
using parchis::squaresInSide;
using parchis::squaresInMain;
using parchis::squaresInPen;
using parchis::birthDieValue;
using parchis::ransomDieValue;
using parchis::penEntryDistanceFromOrigin;
using parchis::penExitDistanceFromOrigin;
using parchis::jumpDistanceFromOrigin;
using parchis::penDieValues;

using parchis::Dice;
using parchis::DiceGenerator;
using parchis::DefaultDiceGenerator;

using parchis::PlayerSettings;

class GameWidgetPrivate;

struct LocationExtId : LocationId
{
    LocationExtId(Section _section = Section{}, int _square = -1, int _forPlayer = -1)
        : LocationId{_section, _square}, forPlayer{_forPlayer} {}

    int forPlayer;
};

inline bool operator==(LocationExtId locationExtId1, LocationExtId locationExtId2)
{
    return locationExtId1.section.kind == locationExtId2.section.kind &&
           locationExtId1.section.index == locationExtId2.section.index &&
           locationExtId1.square == locationExtId2.square &&
           locationExtId1.forPlayer == locationExtId2.forPlayer;
}

inline bool operator!=(LocationExtId locationExtId1, LocationExtId locationExtId2)
{
    return !(locationExtId1 == locationExtId2);
}

class GameWidget final : public QGraphicsView
    /*  TODO
        introduce a new class Scene?
        replace events with slot-signal memes
    */
{
    Q_OBJECT

signals:
    void actionTaken(const Action & action);
    void commandTaken(const Command & command);
    void animationStarted();
    void animationFinished();

public:
    GameWidget(DiceGenerator<dieCount> diceGenerator
               = DefaultDiceGenerator<dieSideCount, dieCount>{});

    ~GameWidget();

    bool isFinished() const;
    const Board & board() const;
    int playerActing() const;
    int playerWithTurn() const;
    const PlayerSettings & playerSettings() const;
    const Dice<dieCount> & dice() const;
    int diceUsed() const;

    std::pair<CommandResultCode, ConstActionSptr> createCommandAction(Command command) const;
    std::vector<std::pair<Command, ActionUptr>> availableCommands() const;

    void startOver(std::vector<int> playerSideMap);
    void reset();
    template<class T> void setDiceGenerator(T value);

    static int squareSide(int mainOrRelSquare);
    static int penEntrySquare(int side);
    static int penExitSquare(int side);
    static int jumpDestinationSquare(int mainOrRelSquare);
    static bool isSquarePenEntry(int mainOrRelSquare);
    static bool isSquarePenExit(int mainOrRelSquare);
    static bool isLocationSafe(LocationId locationId);
    static int sideToRelSide(int side, int baseSide);
    static int relSideToSide(int relSide, int baseSide);
    static int relSquareToMainSquare(int relSquare, int player);
    static int mainSquareToRelSquare(int mainSquare, int player);
    static int locationIdToRelSquare(LocationId locationId, int side);
    static LocationId relSquareToLocationId(int relSquare, int side, int player);

    bool isAnimationRunning() const;
    const std::vector<QPixmap> & playerPawnPixmapMap() const;

    void setPlayerPawnPixmapMap(std::vector<QPixmap> value);

    void doCommand(); //TODO remove, нужна только для дебага

public slots:
    void finishAnimation();
    void takeAction(const Action & action);
    void takeActions(const std::list<ConstActionUptr> & actions);
    void takeActionAnimated(const Action & action);
    void takeActionsAnimated(const std::list<ConstActionUptr> & actions);
    void undoActions(int count);
    void redoActions(int count);

    void showAction(const Action & action);
    void showMovePawnAction(int pawnIndex);
    void showMovePawnActions(int pawnIndex);
    void showBirthAction();
    void showBirthActions();
    void showRansomAction(int captorPlayer);
    void hideActionMarking();
    void hideAllMarking();

    void refreshPawns();

private:
    GameWidgetPrivate * const d_ptr;
    Q_DECLARE_PRIVATE(GameWidget)
};

}


namespace std
{

template<>
struct hash<graphics::LocationExtId>
{
    std::size_t operator()(graphics::LocationExtId locationExtId) const
    {
        std::size_t seed = static_cast<std::size_t>(locationExtId.section.kind);
        hash_combine(seed, locationExtId.section.index);
        hash_combine(seed, locationExtId.square);
        hash_combine(seed, locationExtId.forPlayer);

        return seed;
    }
};

}

#endif // GAMEWIDGET_H
