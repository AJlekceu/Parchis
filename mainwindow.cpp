#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <functional>
#include <iterator>
#include <map>

#include "actions.h"

const QVector<QString> MainWindow::commandKinds
    = {"Skip", "RollDice", "MovePawn", "Birth", "Ransom"};
const QVector<QString> MainWindow::sectionKinds
    = {"Main", "Pen", "Nest", "Captivity", "House"};

MainWindow::MainWindow(const parchis::Game * game, QWidget * parent) :
    QMainWindow(parent),
    _game(game),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->lstwCommand, &QListWidget::currentRowChanged, this, &MainWindow::showAction);
    connect(ui->lstwCommand, &QListWidget::itemActivated, [this]()
    {
        takeAction(ui->lstwCommand->currentRow());
    });

    showGame();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showGame()
{
    int i = 0;
    QString str;

    ui->lstwState->clear();
    ui->lstwCommand->clear();
    ui->lstwAction->clear();
    ui->lstwPawns->clear();
    ui->lstwLocations->clear();
    ui->lstwPawn->clear();

    ui->lstwState->addItem("---playerSettings---");
    ui->lstwState->addItem(QString("playerCount: %1")
                           .arg(_game->playerSettings().playerCount()));

    str = "playersFinishedList: ";
    for(int player : _game->playerSettings().playersFinishedList())
        str += QString::number(player) + ", ";
    ui->lstwState->addItem(str);

    str = "playersFinishedMap: ";
    for(bool finished : _game->playerSettings().playersFinishedMap())
        str += QString::number(finished) + ", ";
    ui->lstwState->addItem(str);

    str = "playersPlayingSet: ";
    for(int player : _game->playerSettings().playersPlayingSet())
        str += QString::number(player) + ", ";
    ui->lstwState->addItem(str);

    str = "playerSideMap: ";
    for(int side : _game->playerSettings().playerSideMap())
        str += QString::number(side) + ", ";
    ui->lstwState->addItem(str);

    ui->lstwState->addItem("---game---");
    ui->lstwState->addItem(QString("playerHavingTurn: %1").arg(_game->playerWithTurn()));
    ui->lstwState->addItem(QString("playerTakingAction: %1").arg(_game->playerActing()));
    ui->lstwState->addItem(QString("isFinished: %1").arg(_game->isFinished()));
    ui->lstwState->addItem(QString("diceUsed: %1").arg(_game->diceUsed()));

    i = 0;

    for(auto die : _game->dice())
    {
        ui->lstwState->addItem(QString("dice[%1]: ").arg(i) + QString::number(die));
        ++i;
    }

    i = 0;

    for(const auto & [command, action] : _game->availableCommands())
    {
        Q_UNUSED(action);

        ui->lstwCommand->addItem(QString("availableCommands[%1]: kind %2, param %3").arg(
                                    QString::number(i),
                                    commandKinds[static_cast<int>(command.kind)],
                                    QString::number(command.param)));
        ++i;
    }

    i = 0;

    std::map<parchis::PawnId, const parchis::Pawn *, parchis::LessForPawnIds> pawns;

    for(const auto & [pawnId, pawn] : _game->board().pawns())
        pawns.emplace(pawnId, &pawn);

    for(const auto & [pawnId, pawn] : pawns)
    {
        ui->lstwPawns->addItem(QString("pawns[%1,%2]: kind %3, index %4, square %5, tired %6").arg(
                                   QString::number(pawnId.player),
                                   QString::number(pawnId.index),
                                   sectionKinds[static_cast<int>(pawn->locationId.section.kind)],
                                   QString::number(pawn->locationId.section.index),
                                   QString::number(pawn->locationId.square),
                                   QString::number(pawn->tired)));
        ++i;
    }

    std::map<parchis::LocationId, const parchis::Location*, parchis::LessForLocationIds> locations;

    for(const auto & [locationId, location] : _game->board().locations())
        locations.emplace(locationId, &location);

    for(const auto & [locationId, location] : locations)
    {
        QString item = QString("locations[%1,%2,%3]: ").arg(
                    sectionKinds[static_cast<int>(locationId.section.kind)],
                    QString::number(locationId.section.index),
                    QString::number(locationId.square));
        if(location->pawnCount() > 1)
            item += "...";
        else if(location->pawnCount() == 1)
            item += QString("pawn[%1,%2]").arg(
                        QString::number(location->firstPawnId().player),
                        QString::number(location->firstPawnId().index));

        ui->lstwLocations->addItem(item);
        ++i;
    }
}

void MainWindow::takeAction(int commandIndex)
{
    auto commands = _game->availableCommands();

    if(commandIndex < 0 || commandIndex >= commands.size())
        return;

    parchis::ActionUptr action
            = std::move(std::next(commands.begin(), commandIndex)->second);

    emit actionTaken(*action);
    showGame();
}

void MainWindow::showAction(int commandIndex)
{
    class ActionShowActionVisitor : public parchis::ActionFullVisitor
    {
    public:
        ActionShowActionVisitor(Ui::MainWindow * ui) : _ui{ui} {}

        void visit(const parchis::Action & /*action*/) override
        {
            _ui->lstwAction->addItem(")))");
        }

        void visit(const parchis::ActionComplex & action) override
        {
            _ui->lstwAction->addItem(_indent + QString("complex: %1 subActions")
                                               .arg(action.subActions().size()));
            _ui->lstwAction->addItem(_indent + QString("{"));
            increaseIndent();
            for(const auto & subAction : action.subActions())
                subAction->accept(*this);
            decreaseIndent();
            _ui->lstwAction->addItem(_indent + QString("}"));
        }

        void visit(const parchis::ActionPawnTired & action) override
        {
            _ui->lstwAction->addItem(_indent + QString("pawnTired: %1 items")
                                               .arg(action.params().size()));
            _ui->lstwAction->addItem(_indent + QString("{"));
            increaseIndent();
            for(const auto & param : action.params())
                _ui->lstwAction->addItem(_indent + QString("pawn[%1,%2], tired: %3").arg(
                                             QString::number(param.first.player),
                                             QString::number(param.first.index),
                                             QString::number(param.second)));
            decreaseIndent();
            _ui->lstwAction->addItem(_indent + QString("}"));
        }

        void visit(const parchis::ActionPawnRelocation & action) override
        {
            _ui->lstwAction->addItem(_indent + QString("pawnRelocation: %1 items")
                                               .arg(action.pawnRelocations().size()));
            _ui->lstwAction->addItem(_indent + QString("{"));
            increaseIndent();
            for(const auto & [pawnId, toLocationId] : action.pawnRelocations())
                _ui->lstwAction->addItem(_indent + QString("pawn[%1,%2], toLocation[%3,%4,%5]").arg(
                        QString::number(pawnId.player),
                        QString::number(pawnId.index),
                        sectionKinds[static_cast<int>(toLocationId.section.kind)],
                        QString::number(toLocationId.section.index),
                        QString::number(toLocationId.square)));
            decreaseIndent();
            _ui->lstwAction->addItem(_indent + QString("}"));
        }

        void visit(const parchis::ActionPlayerFinished & action) override
        {
            _ui->lstwAction->addItem(_indent + QString("playerFinished: %1").arg(action.player()));
        }

        void visit(const parchis::ActionDiceUsed & action) override
        {
            _ui->lstwAction->addItem(_indent + QString("diceUsed: %1").arg(action.diceUsed()));
        }

        void visit(const parchis::ActionDice & action) override
        {
            _ui->lstwAction->addItem(_indent + QString("dice: %1, %2").arg(
                                         QString::number(action.dice()[0]),
                                         QString::number(action.dice()[1])));
        }

        void visit(const parchis::ActionActionAndTurn & action) override
        {
            _ui->lstwAction->addItem(_indent + QString("actionAndTurn: %1, %2").arg(
                                         QString::number(action.playerTakingAction()),
                                         QString::number(action.playerHavingTurn())));
        }

    private:
        void increaseIndent()
        {
            _indent += "  ";
        }
        void decreaseIndent()
        {
            _indent.chop(2);
        }

        Ui::MainWindow *_ui;
        QString _indent;
    } visitor{ui};

    auto commands = _game->availableCommands();

    ui->lstwAction->clear();
    if(commandIndex < 0 || commandIndex >= commands.size())
        return;

    parchis::ActionUptr action = std::move(std::next(commands.begin(), commandIndex)->second);

    ui->lstwAction->clear();
    action->accept(visitor);

    emit actionShown(*action);
}

int MainWindow::chosenActionIndex() const
{
    if(ui->lstwCommand->selectionModel()->selectedIndexes().size() == 0)
        return -1;

    return ui->lstwCommand->currentRow();
}
