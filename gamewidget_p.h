#ifndef GAMEWIDGET_P_H
#define GAMEWIDGET_P_H

#include "gamewidget.h"

#include <QGraphicsEffect>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QSequentialAnimationGroup>
#include <functional>
#include <vector>
#include <unordered_map>

#include "board.h"
#include "mainwindow.h"

namespace graphics
{

struct PawnItemCommonProps
{
    std::function<QPixmap(int)> playerPawnPixmapFun;
    std::function<QPointF(LocationExtId)> locationExtIdPosFun;
};

class PawnItem : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT
    Q_PROPERTY(QPointF pos READ pos WRITE setPos)

signals:
    void mousePressed(QGraphicsSceneMouseEvent * event);
    void hoverMoved(QGraphicsSceneHoverEvent * event);
    void hoverLeft(QGraphicsSceneHoverEvent * event);

public:
    enum { Type = UserType + 1 };

    enum HighlightKind : unsigned int
    {
        NoHighlight = 0,
        Moving = 1,
        Captured = 2,
        CanMoveThisAction = 4,
        CanMoveThisTurn = 8,
        Tired = 16,
    };

    PawnItem(PawnItemCommonProps * commonProps, PawnId id, LocationId locationId);

    int type() const override { return Type; }
    PawnId id() const { return _id; }
    unsigned int highlight() { return _highlight; }

    void updatePixmap();
    void setId(PawnId value);
    void setHighlight(unsigned int kind);
    void moveToLocationId(LocationId value);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent * event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent * event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent * event) override;

private:
    const PawnItemCommonProps * const _commonProps;
    PawnId _id;
    unsigned int _highlight;
};

struct ArrowItemCommonProps
{
    std::function<QPointF(LocationExtId)> locationExtIdPosFun;
    qreal arrowSize = 0;
    qreal penWidth = 0;
};

class ArrowItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 2 };

    ArrowItem(ArrowItemCommonProps * commonProps, LocationExtId fromLocationExtId,
              LocationExtId toLocationExtId);

    int type() const override { return Type; }

    void setEndPoints(LocationExtId fromLocationExtId, LocationExtId toLocationExtId);

protected:
    QRectF boundingRect() const override;
    void paint(QPainter * painter, const QStyleOptionGraphicsItem * option,
               QWidget * widget) override;

private:
    const ArrowItemCommonProps * const _commonProps;
    QPointF _fromLocationPos;
    QPointF _toLocationPos;
};

struct MarkerItemCommonProps
{
    std::function<QPointF(LocationExtId)> locationExtIdPosFun;
    QSizeF locationSize;
    qreal penWidth;
};

class MarkerItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
    Q_PROPERTY(QPointF pos READ pos WRITE setPos)

public:
    enum { Type = UserType + 3 };

    MarkerItem(MarkerItemCommonProps * commonProps, LocationExtId locationExtId);

    int type() const override { return Type; }
    LocationExtId locationExtId() const { return _locationExtId; }

    void setLocationExtId(LocationExtId value);

private:
    const MarkerItemCommonProps * const _commonProps;
    LocationExtId _locationExtId;
};

struct NestItemCommonProps
{
    qreal penWidthHighlight = 0;
    qreal penWidthNoHighlight = 0;
};

class NestItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT

signals:
    void mousePressed(QGraphicsSceneMouseEvent * event);
    void hoverMoved(QGraphicsSceneHoverEvent * event);
    void hoverLeft(QGraphicsSceneHoverEvent * event);

public:
    enum { Type = UserType + 4 };

    NestItem(NestItemCommonProps * commonProps, int player = -1, QRectF rect = QRectF{});

    int type() const override { return Type; }
    int player() const { return _player; }

    void setPlayer(int player) { _player = player; }
    void setHighlight(bool value);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent * event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent * event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent * event) override;

private:
    const NestItemCommonProps * const _commonProps;
    int _player;
};

struct CaptivityItemCommonProps
{
    qreal penWidthHighlight = 0;
    qreal penWidthNoHighlight = 0;
};

class CaptivityItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT

signals:
    void mousePressed(QGraphicsSceneMouseEvent * event);
    void hoverMoved(QGraphicsSceneHoverEvent * event);
    void hoverLeft(QGraphicsSceneHoverEvent * event);

public:
    enum { Type = UserType + 5 };

    CaptivityItem(CaptivityItemCommonProps * commonProps, int player = -1, QRectF rect = QRectF{});

    int type() const override { return Type; }
    int player() const { return _player; }

    void setPlayer(int player) { _player = player; }
    void setHighlight(bool value);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent * event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent * event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent * event) override;

private:
    const CaptivityItemCommonProps * const _commonProps;
    int _player;
};

class HighlightEffect : public QGraphicsEffect
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor)
    Q_PROPERTY(QPointF offset READ offset WRITE setOffset)

public:
    HighlightEffect(qreal offset = 1.5) :
        QGraphicsEffect(), mColor( 255, 255, 0, 128 ), mOffset( offset, offset )
    {}

    virtual QRectF boundingRectFor(const QRectF& sourceRect) const;
    QColor color() const { return mColor; }
    void setColor(QColor color) { mColor = color; }
    QPointF offset() const { return mOffset; }
    void setOffset(QPointF offset) { mOffset = offset; }

protected:
    virtual void draw(QPainter *painter);

private:
    QColor mColor;
    QPointF mOffset;
};

class GameWidgetPrivate final : public QObject
{
    Q_OBJECT

public:
    GameWidgetPrivate(GameWidget * q, DiceGenerator<dieCount> diceGenerator)
        : q_ptr{q}, game{diceGenerator}, nextActionIterator{actionHistory.begin()},
          scene{q}, mainWindow{&game}
    { }

    ~GameWidgetPrivate() { finishAnimation(); }

    QPointF locationExtIdPos(LocationExtId locationExtId) const;

    void addPawnItem(PawnItem * value);
    void addArrowItem(ArrowItem * value);
    void addMarkerItem(MarkerItem * value);
    void addNestItem(NestItem * value);
    void addCaptivityItem(CaptivityItem * value);
    void takeActionsAux(const std::list<ConstActionUptr> & actions);
    void refreshPawnsAux(const Location & location, const std::vector<QPointF> & playerPosMap,
                         std::function<int(PawnId)> playerFun);
    void startAnimation(const Action & action);
    void startAnimation(const std::list<ConstActionUptr> & actions);
    void addAnimation(const Action & action);
    void addAnimation(const std::list<ConstActionUptr> & actions);
    void finishAnimation();
    void clearHistory();
    void clearCommandCache();

    static GameWidgetPrivate * get(GameWidget * gameWidget) { return gameWidget->d_func(); }
    static const GameWidgetPrivate * get(const GameWidget * gameWidget)
    { return gameWidget->d_func(); }

    GameWidget * q_ptr;
    parchis::Game game;
    parchis::Game gameVisual{nullptr};
    std::list<ConstActionUptr> actionHistory;
    decltype(actionHistory)::iterator nextActionIterator;
    int nextActionIndex = 0;
    mutable std::unordered_map<Command,
                               std::pair<CommandResultCode, ConstActionSptr>> cachedCommands;
    QGraphicsScene scene;
    QSequentialAnimationGroup * animationSequence = nullptr;
    QSizeF locationSize;
    QSizeF horNestSize;
    QSizeF horCaptivitySize;
    QSizeF verNestSize;
    QSizeF verCaptivitySize;
    std::vector<QPixmap> playerPawnPixmapMap;
    PawnItemCommonProps pawnItemCommonProps;
    ArrowItemCommonProps arrowItemCommonProps;
    MarkerItemCommonProps markerItemCommonProps;
    NestItemCommonProps nestItemCommonProps;
    CaptivityItemCommonProps captivityItemCommonProps;
    std::unordered_map<PawnId, PawnItem *> pawnItems;
    std::vector<ArrowItem *> arrowItems;
    std::vector<MarkerItem *> markerItems;
    std::vector<NestItem *> playerNestItemMap;
    std::vector<CaptivityItem *> playerCaptivityItemMap;
    std::unordered_map<LocationExtId, QPointF> locationExtIdPosMap;
    MainWindow mainWindow;

public slots:
    void pawnMousePress(QGraphicsSceneMouseEvent * event);
    void pawnHoverMove(QGraphicsSceneHoverEvent * event);
    void pawnHoverLeave(QGraphicsSceneHoverEvent * event);
    void nestMousePress(QGraphicsSceneMouseEvent * event);
    void nestHoverMove(QGraphicsSceneHoverEvent * event);
    void nestHoverLeave(QGraphicsSceneHoverEvent * event);
    void captivityMousePress(QGraphicsSceneMouseEvent * event);
    void captivityHoverMove(QGraphicsSceneHoverEvent * event);
    void captivityHoverLeave(QGraphicsSceneHoverEvent * event);

private:
    Q_DECLARE_PUBLIC(GameWidget)
};

}

#endif // GAMEWIDGET_P_H
