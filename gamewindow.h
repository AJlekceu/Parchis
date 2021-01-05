#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QMainWindow>

#include "gamewidget.h"

namespace Ui {

class GameWindow;

}


class GameWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit GameWindow(QWidget * parent = nullptr);
    ~GameWindow();

private slots:
    void on_btnUndoAction_clicked();

    void on_btnRedoAction_clicked();

    void on_btnAI_clicked();

private:
    Ui::GameWindow *ui;
    graphics::GameWidget * _gameWidget;
};

#endif // GAMEWINDOW_H
