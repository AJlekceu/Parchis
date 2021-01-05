#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>

#include <memory>

#include "game.h"

namespace Ui {

class MainWindow;

}


class MainWindow : public QMainWindow
{
    Q_OBJECT

signals:
    void actionTaken(const parchis::Action & action);
    void actionShown(const parchis::Action & action);

public:
    static const QVector<QString> commandKinds;
    static const QVector<QString> sectionKinds;

    explicit MainWindow(const parchis::Game * game, QWidget * parent = nullptr);
    ~MainWindow();

public slots:
    void showGame();
    void takeAction(int actionIndex);
    void showAction(int actionIndex);

private:
    int chosenActionIndex() const;

    const parchis::Game * _game;
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
