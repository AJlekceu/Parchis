#include "gamewindow.h"
#include "ui_gamewindow.h"

GameWindow::GameWindow(QWidget * parent) :
    QMainWindow(parent),
    ui(new Ui::GameWindow)
{
    _gameWidget = new graphics::GameWidget{parchis::DefaultDiceGenerator<parchis::dieSideCount, parchis::dieCount>{123}}; //todo undo

    ui->setupUi(this);
    setCentralWidget(_gameWidget);
    ui->btnUndoAction->setParent(_gameWidget);
    ui->btnRedoAction->setParent(_gameWidget);
    ui->btnAI->setParent(_gameWidget);
}

GameWindow::~GameWindow()
{
    delete ui;
}

void GameWindow::on_btnUndoAction_clicked()
{
    _gameWidget->undoActions(1);
    _gameWidget->refreshPawns();
}

void GameWindow::on_btnRedoAction_clicked()
{
    _gameWidget->redoActions(1);
    _gameWidget->refreshPawns();
}

void GameWindow::on_btnAI_clicked()
{
    _gameWidget->doCommand();
}
