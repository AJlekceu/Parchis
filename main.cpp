#include <QApplication>

#include "mainwindow.h"
#include "gamewindow.h"

#include <list>
#include <cassert>
#include <cmath>

#include <QtDebug>

std::list<std::pair<parchis::Command, parchis::ConstActionUptr>> chooseCommandSequence(parchis::Game & game); //todo remove

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    GameWindow gw;

    /*parchis::Game game(parchis::DefaultDiceGenerator<parchis::dieSideCount, parchis::dieCount>{123});
    std::vector<std::vector<int>> victories;
    int wins[4] = {0};

    for(int i = 0; i < 100; ++i)
    {
        game.startOver({0, 1, 2, 3});

        while(!game.isFinished())
        {
            int playerTakingAction = game.playerTakingAction();

            if(game.diceUsed() == 2)
            {
                game.takeAction(*game.rollDiceAction().second);
                continue;
            }

            if(game.playerTakingAction() % 2 == 0)
            {
                auto commandSequence = chooseCommandSequence(game);

                assert(!commandSequence.empty());
                while(!commandSequence.empty() && game.playerTakingAction() == playerTakingAction)
                {
                    game.takeAction(*commandSequence.front().second);
                    commandSequence.pop_front();
                }
                //game.takeAction(*game.availableCommands().front().second);
            }
            else
            {
                auto commands = game.availableCommands();
                unsigned int number = rand() % commands.size();
                game.takeAction(*commands.at(number).second);
            }
        }
        ++wins[game.playerSettings().playersFinishedList().front()];
        victories.push_back(game.playerSettings().playersFinishedList());
        qDebug() << "Game #" << i;
    }*/

    gw.show();
    return a.exec();
}
