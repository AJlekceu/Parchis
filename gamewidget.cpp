#include "gamewidget.h"
#include "gamewidget_p.h"

#include <QtDebug>

#include <QAnimationGroup>
#include <QBitmap>
#include <QGraphicsColorizeEffect>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QList>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <algorithm>
#include <cassert>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <cmath>

#include "game.h"
#include "actions.h"
#include "visitor.h"
#include "utilities.h"

/*int clearDistanceAhead(const parchis::Game & game, int relSquare, int player); //TODO remove
enum TransitionKind;
std::pair<int, TransitionKind> nextTransition(int relSquare);
std::pair<int, TransitionKind> previousTransition(int relSquare);
#include <bitset>
double distanceChance(const parchis::Game & game, int player, int fromRelSquare, int distance,
                      std::bitset<parchis::dieSideCount + 1> skippableDieValues);
struct CommandTree
{
    parchis::Command command;
    parchis::ConstActionUptr action;
    std::list<std::unique_ptr<CommandTree>> children;
};
std::unique_ptr<CommandTree> buildCommandTree(parchis::Game & game);
std::list<std::pair<parchis::Command, parchis::ConstActionUptr>> chooseCommandSequence(parchis::Game & game);
*/
namespace graphics
{

using parchis::Game;
using parchis::PawnRelocation;
using parchis::ActionComplex;
using parchis::ActionPawnTired;
using parchis::ActionPawnRelocation;
using parchis::ActionDiceUsed;
using parchis::ActionDice;
using parchis::ActionActionAndTurn;
using parchis::ActionPlayerFinished;
using parchis::ActionGameFinished;

static void movePawnActionsApply(Game * game, int pawnIndex,
                                 std::function<void(const Action &)> actionFun);

class BornPawnSearchVisitor final :
        public Loki::BaseVisitor,
        public Loki::Visitor<ActionComplex, void, true>,
        public Loki::Visitor<ActionPawnRelocation, void, true>
{
public:
    BornPawnSearchVisitor(GameWidget * gameWidget) : _gameWidget{gameWidget}
    {}

    int bornPawnIndex() const { return _bornPawnIndex; }

    void visit(const ActionComplex & action) override
    {
        _bornPawnIndex = -1;

        for(const auto & subAction : action.subActions())
        {
            subAction->accept(*this);
            if(_bornPawnIndex != -1)
                return;
        }
    }

    void visit(const ActionPawnRelocation & action) override
    {
        _bornPawnIndex = -1;

        for(const auto & [pawnId, toLocationId] : action.pawnRelocations())
        {
            if(_gameWidget->board().pawn(pawnId).locationId ==
                    LocationId{{Section::Kind::Nest}})
            {
                _bornPawnIndex = pawnId.index;
                return;
            }
        }
    }

private:
    GameWidget * _gameWidget;
    int _bornPawnIndex = -1;
};

class ActionAnimationVisitor final :
        public Loki::BaseVisitor,
        public Loki::Visitor<ActionComplex, void, true>,
        public Loki::Visitor<ActionPawnRelocation, void, true>
{
public:
    ActionAnimationVisitor(GameWidget * gameWidget)
        : _gameWidget{gameWidget}, _gameWidgetPrivate{GameWidgetPrivate::get(gameWidget)}
    {
    }

    std::unique_ptr<QAbstractAnimation> & animation() { return _animation; }

    void visit(const ActionComplex & action) override
    {
        _animation = nullptr;

        if(action.subActions().size() == 0)
            return;

        std::unique_ptr<QParallelAnimationGroup> ret =
                std::make_unique<QParallelAnimationGroup>();

        for(const auto & subAction : action.subActions())
        {
            subAction->accept(*this);
            if(_animation)
                ret->addAnimation(_animation.release());
        }

        if(ret->animationCount() != 0)
            _animation = std::move(ret);
    }

    void visit(const ActionPawnRelocation & action) override
    {
        _animation = nullptr;

        if(action.pawnRelocations().size() == 0)
            return;

        std::unique_ptr<QParallelAnimationGroup> ret =
                std::make_unique<QParallelAnimationGroup>();
        std::unordered_map<PawnId, std::unique_ptr<QSequentialAnimationGroup>> pawnAnimations;
        int zValue = 0;

        for(const auto & [pawnId, toLocationId] : action.pawnRelocations())
        {
            PawnItem * pawnItem = _gameWidgetPrivate->pawnItems.at(pawnId);
            QPointF toLocationPos = _gameWidgetPrivate->locationExtIdPos({toLocationId.section,
                                                                          toLocationId.square,
                                                                          pawnId.player});
            std::unique_ptr<QPropertyAnimation> pawnSubAnimation =
                    std::make_unique<QPropertyAnimation>(pawnItem, "pos");
            auto [pawnAnimationIter, isPawnNew] = pawnAnimations.emplace(pawnId, nullptr);
            auto & pawnAnimation = pawnAnimationIter->second;

            if(isPawnNew)
            {
                pawnAnimation = std::make_unique<QSequentialAnimationGroup>();

                QObject::connect(pawnAnimation.get(), &QSequentialAnimationGroup::stateChanged,
                                 [pawnItem, pawnZValue = ++zValue]
                                 (QAbstractAnimation::State newState,
                                 QAbstractAnimation::State /* oldState */)
                {
                    if(newState == QAbstractAnimation::Running)
                    {
                        pawnItem->setZValue(pawnZValue);
                    }
                });
            }

            pawnSubAnimation->setEndValue(toLocationPos);
            pawnAnimation->addAnimation(pawnSubAnimation.release());
        }

        for(auto & [pawnId, pawnAnimation] : pawnAnimations)
        {
            Q_UNUSED(pawnId);

            int pawnSubAnimationCount = pawnAnimation->animationCount();

            for(int i = 0; i < pawnSubAnimationCount; ++i)
            {
                QPropertyAnimation * pawnSubAnimation
                        = static_cast<QPropertyAnimation *>(pawnAnimation->animationAt(i));
                QEasingCurve curve;

                if(pawnSubAnimationCount == 1)
                    curve = QEasingCurve::InOutCubic;
                else if(i == 0)
                    curve = QEasingCurve::InCubic;
                else if(i == pawnSubAnimationCount - 1)
                    curve = QEasingCurve::OutCubic;
                else
                    curve = QEasingCurve::Linear;

                pawnSubAnimation->setEasingCurve(curve);
                pawnSubAnimation->setDuration(600 / pawnSubAnimationCount); //TODO 600 заменить константой
            }

            ret->addAnimation(pawnAnimation.release());
        }

        _animation = std::move(ret);
    }

private:
    GameWidget * const _gameWidget;
    GameWidgetPrivate * const _gameWidgetPrivate;
    std::unique_ptr<QAbstractAnimation> _animation = nullptr;
};

class ActionMarkingVisitor final :
        public Loki::BaseVisitor,
        public Loki::Visitor<ActionComplex, void, true>,
        public Loki::Visitor<ActionPawnRelocation, void, true>
{
public:
    ActionMarkingVisitor(GameWidget * gameWidget)
        : _gameWidget{gameWidget}, _gameWidgetPrivate{GameWidgetPrivate::get(gameWidget)}
    { }

    void visit(const ActionComplex & action) override
    {
        for(const auto & subAction : action.subActions())
            subAction->accept(*this);
    }

    void visit(const ActionPawnRelocation & action) override
    {
        std::unordered_map<PawnId, LocationId> pawnLocations;

        for(const auto & [pawnId, toLocationId] : action.pawnRelocations())
        {
            auto pawnLocationIter = pawnLocations.find(pawnId);

            LocationId fromLocationId = pawnLocationIter == pawnLocations.cend() ?
                        _gameWidget->board().pawn(pawnId).locationId :
                        pawnLocationIter->second;

            if(fromLocationId.section.kind == Section::Kind::Nest)
            {
                _gameWidgetPrivate->playerNestItemMap.at(pawnId.player)->
                        setHighlight(true);
            }
            else if(fromLocationId.section.kind == Section::Kind::Captivity)
            {
                _gameWidgetPrivate->playerCaptivityItemMap.at(fromLocationId.section.index)->
                        setHighlight(true);
            }
            else if(toLocationId.section.kind != Section::Kind::Nest &&
                    toLocationId.section.kind != Section::Kind::Captivity)
            {
                LocationExtId fromLocationExtId{
                    fromLocationId.section, fromLocationId.square, pawnId.player};
                LocationExtId toLocationExtId{
                    toLocationId.section, toLocationId.square, pawnId.player};

                _gameWidgetPrivate->addArrowItem(
                            new ArrowItem{&_gameWidgetPrivate->arrowItemCommonProps,
                                          fromLocationExtId, toLocationExtId});
            }

            pawnLocations[pawnId] = toLocationId;
        }

        for(const auto & [pawnId, locationId] : pawnLocations)
        {
            LocationExtId locationExtId{locationId.section, locationId.square, pawnId.player};

            if(locationId.section.kind != Section::Kind::Nest &&
                    locationId.section.kind != Section::Kind::Captivity)
                _gameWidgetPrivate->addMarkerItem(
                            new MarkerItem{&_gameWidgetPrivate->markerItemCommonProps,
                                           locationExtId});
        }
    }

private:
    GameWidget * const _gameWidget;
    GameWidgetPrivate * const _gameWidgetPrivate;
};

void movePawnActionsApply(const Game & game, int pawnIndex,
                          std::function<void(const Action &)> actionFun)
{
    bool stop = game.playerWithTurn() != game.playerActing();

    do
    {
        auto [resultCode, action] = game.createCommandAction({Command::Kind::MovePawn, pawnIndex});

        if(resultCode.success())
            actionFun(*action);
        else
            stop = true;
    } while(!stop);
}

PawnItem::PawnItem(PawnItemCommonProps * commonProps, PawnId id, LocationId locationId)
    : _commonProps{commonProps}
{
    setId(id);
    setHighlight(PawnItem::NoHighlight);
    moveToLocationId(locationId);
    setAcceptHoverEvents(true);
}

void PawnItem::updatePixmap()
{
    setPixmap(_commonProps->playerPawnPixmapFun(id().player));
    setOffset(-0.5 * QPointF(pixmap().width(), pixmap().height()));
}

void PawnItem::setId(PawnId value)
{
    _id = value;
    updatePixmap();
}

void PawnItem::setHighlight(unsigned int kind)
{
    HighlightEffect * effect = nullptr;

    if(!kind)
    {
    }
    else
    {
        effect = new HighlightEffect;

        if(kind & PawnItem::Moving)
        {
            effect->setColor(QColor{Qt::blue}); //TODO move constants to header
        }
        else if(kind & PawnItem::Captured)
        {
            effect->setColor(QColor{Qt::red});
        }
        else if(kind & PawnItem::CanMoveThisAction)
        {
            effect->setColor(QColor{Qt::green});
        }
        else if(kind & PawnItem::CanMoveThisTurn)
        {
            effect->setColor(QColor{Qt::yellow});
        }
        else if(kind & PawnItem::Tired)
        {
            effect->setColor(QColor{Qt::magenta});
        }
    }

    setGraphicsEffect(effect);
    _highlight = kind;
}

void PawnItem::moveToLocationId(LocationId value)
{
    setPos(_commonProps->locationExtIdPosFun({value.section, value.square, id().player}));
}

void PawnItem::mousePressEvent(QGraphicsSceneMouseEvent * event)
{
    emit mousePressed(event);
}

void PawnItem::hoverMoveEvent(QGraphicsSceneHoverEvent * event)
{
    emit hoverMoved(event);
}

void PawnItem::hoverLeaveEvent(QGraphicsSceneHoverEvent * event)
{
    emit hoverLeft(event);
}

ArrowItem::ArrowItem(ArrowItemCommonProps * commonProps, LocationExtId fromLocationExtId,
                     LocationExtId toLocationExtId)
    : _commonProps{commonProps}
{
    setEndPoints(fromLocationExtId, toLocationExtId);
}

void ArrowItem::setEndPoints(LocationExtId fromLocationExtId, LocationExtId toLocationExtId)
{
    prepareGeometryChange();

    _fromLocationPos = _commonProps->locationExtIdPosFun(fromLocationExtId);
    _toLocationPos = _commonProps->locationExtIdPosFun(toLocationExtId);
}

QRectF ArrowItem::boundingRect() const
{
    qreal extra = (_commonProps->penWidth + _commonProps->arrowSize) / 2.0;

    return QRectF{_fromLocationPos, QSizeF{_toLocationPos.x() - _fromLocationPos.x(),
                                           _toLocationPos.y() - _fromLocationPos.y()}}
        .normalized()
        .adjusted(-extra, -extra, extra, extra);
}

void ArrowItem::paint(QPainter * painter, const QStyleOptionGraphicsItem * /* option */,
                      QWidget * /* widget */)
{
    QLineF line{_fromLocationPos, _toLocationPos};
    qreal pi = 3.14159;

    if (qFuzzyCompare(line.length(), qreal(0.)))
        return;

    // Draw the line itself
    painter->setPen(QPen{Qt::black, _commonProps->penWidth, Qt::SolidLine, Qt::RoundCap,
                         Qt::RoundJoin});
    painter->drawLine(line);

    // Draw the arrows
    double angle = ::acos(line.dx() / line.length());
    if (line.dy() >= 0)
        angle = 2 * pi - angle;

    QPointF arrowHeadP1 = _toLocationPos +
            QPointF(sin(angle - pi / 3) * _commonProps->arrowSize,
                    cos(angle - pi / 3) * _commonProps->arrowSize);
    QPointF arrowHeadP2 = _toLocationPos +
            QPointF(sin(angle - pi + pi / 3) * _commonProps->arrowSize,
                    cos(angle - pi + pi / 3) * _commonProps->arrowSize);

    painter->setBrush(Qt::black);
    painter->drawPolygon(QPolygonF() << line.p2() << arrowHeadP1 << arrowHeadP2);
}

MarkerItem::MarkerItem(MarkerItemCommonProps * commonProps, LocationExtId locationExtId)
    : _commonProps{commonProps}
{
    setLocationExtId(locationExtId);
    setPen({Qt::black, _commonProps->penWidth, Qt::DashLine, Qt::RoundCap, Qt
            ::RoundJoin});
}

void MarkerItem::setLocationExtId(LocationExtId value)
{
    QPointF locationPos = _commonProps->locationExtIdPosFun(value);

    _locationExtId = value;
    setRect(QRectF{locationPos, _commonProps->locationSize}.
            translated(-_commonProps->locationSize.width() / 2,
                       -_commonProps->locationSize.height() / 2));
}

NestItem::NestItem(NestItemCommonProps * commonProps, int player, QRectF rect)
    : _commonProps{commonProps}
{
    _player = player;
    setAcceptHoverEvents(true);
    setRect(rect);
    setZValue(std::numeric_limits<qreal>::max()); //TODO change to 1
}

void NestItem::setHighlight(bool value)
{
    if(value)
        setPen(QPen{Qt::black, _commonProps->penWidthHighlight}); //TODO move any constants to NestItemCommonProps
    else
        setPen(QPen{Qt::black, _commonProps->penWidthNoHighlight});
}

void NestItem::mousePressEvent(QGraphicsSceneMouseEvent * event)
{
    emit mousePressed(event);
}

void NestItem::hoverMoveEvent(QGraphicsSceneHoverEvent * event)
{
    emit hoverMoved(event);
}

void NestItem::hoverLeaveEvent(QGraphicsSceneHoverEvent * event)
{
    emit hoverLeft(event);
}

CaptivityItem::CaptivityItem(CaptivityItemCommonProps * commonProps, int player, QRectF rect)
    : _commonProps{commonProps}
{
    _player = player;
    setAcceptHoverEvents(true);
    setRect(rect);
    setZValue(std::numeric_limits<qreal>::max());
}

void CaptivityItem::setHighlight(bool value)
{
    if(value)
        setPen(QPen{Qt::black, _commonProps->penWidthHighlight}); //TODO move any constants to CaptivityItemCommonProps
    else
        setPen(QPen{Qt::black, _commonProps->penWidthNoHighlight});
}

void CaptivityItem::mousePressEvent(QGraphicsSceneMouseEvent * event)
{
    emit mousePressed(event);
}

void CaptivityItem::hoverMoveEvent(QGraphicsSceneHoverEvent * event)
{
    emit hoverMoved(event);
}

void CaptivityItem::hoverLeaveEvent(QGraphicsSceneHoverEvent * event)
{
    emit hoverLeft(event);
}

QRectF HighlightEffect::boundingRectFor(const QRectF & sourceRect) const
{
    return sourceRect.adjusted(-mOffset.x(), -mOffset.y(), mOffset.x(), mOffset.y());
}

void HighlightEffect::draw(QPainter * painter)
{
    QPoint offset;
    QPixmap pixmap;

    // if ( sourceIsPixmap() ) // doesn't seem to work, return false
    // {
    // No point in drawing in device coordinates (pixmap will be scaled anyways).
    pixmap = sourcePixmap(Qt::LogicalCoordinates, &offset);
    //}

    QRectF bound = boundingRectFor( pixmap.rect() );

    painter->save();
    painter->setPen( Qt::NoPen );
    painter->setBrush( mColor );
    QPointF p(offset.x()-mOffset.x(), offset.y()-mOffset.y());
    bound.moveTopLeft( p );
    painter->drawRoundedRect(bound, 5, 5, Qt::RelativeSize);
    painter->drawPixmap(offset, pixmap);
    painter->restore();
}

QPointF GameWidgetPrivate::locationExtIdPos(LocationExtId locationExtId) const
{
    auto iter = locationExtIdPosMap.find({locationExtId.section, locationExtId.square});

    if(iter != locationExtIdPosMap.cend())
        return iter->second;
    return locationExtIdPosMap.at(locationExtId);
}

void GameWidgetPrivate::addPawnItem(PawnItem * value)
{
    scene.addItem(value);
    pawnItems.emplace(value->id(), value);
    connect(value, &PawnItem::mousePressed, this, &GameWidgetPrivate::pawnMousePress);
    connect(value, &PawnItem::hoverMoved, this, &GameWidgetPrivate::pawnHoverMove);
    connect(value, &PawnItem::hoverLeft, this, &GameWidgetPrivate::pawnHoverLeave);
}

void GameWidgetPrivate::addArrowItem(ArrowItem * value)
{
    scene.addItem(value);
    arrowItems.emplace_back(value);
}

void GameWidgetPrivate::addMarkerItem(MarkerItem * value)
{
    scene.addItem(value);
    markerItems.emplace_back(value);
}

void GameWidgetPrivate::addNestItem(NestItem * value)
{
    scene.addItem(value);
    if(playerNestItemMap.size() < value->player() + 1)
        playerNestItemMap.resize(value->player() + 1);
    playerNestItemMap.at(value->player()) = value;
    connect(value, &NestItem::mousePressed, this, &GameWidgetPrivate::nestMousePress);
    connect(value, &NestItem::hoverMoved, this, &GameWidgetPrivate::nestHoverMove);
    connect(value, &NestItem::hoverLeft, this, &GameWidgetPrivate::nestHoverLeave);
}

void GameWidgetPrivate::addCaptivityItem(CaptivityItem * value)
{
    scene.addItem(value);
    if(playerCaptivityItemMap.size() < value->player() + 1)
        playerCaptivityItemMap.resize(value->player() + 1);
    playerCaptivityItemMap.at(value->player()) = value;
    connect(value, &CaptivityItem::mousePressed, this, &GameWidgetPrivate::captivityMousePress);
    connect(value, &CaptivityItem::hoverMoved, this, &GameWidgetPrivate::captivityHoverMove);
    connect(value, &CaptivityItem::hoverLeft, this, &GameWidgetPrivate::captivityHoverLeave);
}

void GameWidgetPrivate::takeActionsAux(const std::list<ConstActionUptr> & actions)
{
    Q_Q(GameWidget);

    actionHistory.resize(nextActionIndex);
    clearCommandCache();

    for(const auto & action : actions)
    {
        actionHistory.emplace_back(game.takeAction(*action));
        emit q->actionTaken(*action);
    }

    nextActionIndex += actions.size();
    nextActionIterator = actionHistory.end();
}

void GameWidgetPrivate::refreshPawnsAux(const Location & location,
                                        const std::vector<QPointF> & playerPosMap,
                                        std::function<int(PawnId)> playerFun)
{
    Q_Q(GameWidget);

    const qreal intervalSame = 15;
    const qreal intervalDifferent = 30;

    int playerCount = q->playerSettings().playerCount();
    std::vector<qreal> playerShift(playerCount, 0);
    int lastPlayer = -1; //TODO доделать разрыв между разными пешками

    for(PawnId pawnId : location.pawnIds)
    {
        PawnItem * pawn = pawnItems.at(pawnId);
        int player = playerFun(pawnId);
        int side = q->playerSettings().playerSideMap().at(player);

        playerShift.at(player) += pawn->pixmap().width() / 2 + intervalSame / 2;
        //TODO pixmap().height() тоже надо
        if(lastPlayer != -1 && pawnId.player != lastPlayer)
            playerShift.at(player) += intervalDifferent;
        pawn->setPos(playerPosMap.at(player) + (side % 2 == 0 ?
                                                    QPointF{playerShift.at(player), 0} :
                                                    QPointF{0, playerShift.at(player)}));
        playerShift.at(player) += pawn->pixmap().width() / 2 + intervalSame / 2;
    }

    for(PawnId pawnId : location.pawnIds)
    {
        PawnItem * pawn = pawnItems.at(pawnId);
        int player = playerFun(pawnId);
        int side = q->playerSettings().playerSideMap().at(player);

        if(side % 2 == 0)
            pawn->moveBy(-playerShift.at(player) / 2, 0);
        else
            pawn->moveBy(0, -playerShift.at(player) / 2);
    }

    //TODO make it so the pawns always fit in the nests and captivities
}

void GameWidgetPrivate::startAnimation(const Action & action)
{
    std::list<ConstActionUptr> actions;

    actions.emplace_back(action.clone());
    startAnimation(actions);
}

void GameWidgetPrivate::startAnimation(const std::list<ConstActionUptr> & actions)
{
    finishAnimation();
    addAnimation(actions);
}

void GameWidgetPrivate::addAnimation(const Action & action)
{
    std::list<ConstActionUptr> actions;

    actions.emplace_back(action.clone());
    addAnimation(actions);
}

void GameWidgetPrivate::addAnimation(const std::list<ConstActionUptr> & actions)
{
    Q_Q(GameWidget);

    ActionAnimationVisitor actionAnimationVisitor{q};

    if(!q->isAnimationRunning())
    {
        animationSequence = new QSequentialAnimationGroup;

        QObject::connect(animationSequence, &QSequentialAnimationGroup::stateChanged,
                         [q, this]
                         (QAbstractAnimation::State newState,
                         QAbstractAnimation::State /* oldState */)
        {
            if(newState == QAbstractAnimation::Stopped)
            {
                animationSequence = nullptr;
                emit q->animationFinished();
            }
            else if(newState == QAbstractAnimation::Running)
            {
                emit q->animationStarted();
            }
        });

        q->refreshPawns();
    }

    for(const auto & action : actions)
    {
        action->accept(actionAnimationVisitor);

        QAbstractAnimation * actionAnimation = actionAnimationVisitor.animation().release();

        if(actionAnimation)
        {
            animationSequence->addAnimation(actionAnimation);

            QObject::connect(actionAnimation, &QSequentialAnimationGroup::finished,
                             [action = action->clone(), q, this]()
            {
                gameVisual.takeAction(*action);
                q->refreshPawns();
            });
        }
    }

    animationSequence->start(QAbstractAnimation::DeleteWhenStopped);
}

void GameWidgetPrivate::finishAnimation()
{
    Q_Q(GameWidget);

    if(q->isAnimationRunning())
        animationSequence->setCurrentTime(animationSequence->totalDuration());
}

void GameWidgetPrivate::clearHistory()
{
    actionHistory.clear();
    nextActionIterator = actionHistory.end();
    nextActionIndex = 0;
}

void GameWidgetPrivate::clearCommandCache()
{
    cachedCommands.clear();
}

void GameWidgetPrivate::pawnMousePress(QGraphicsSceneMouseEvent * event)
{
    Q_Q(GameWidget);

    PawnItem * pawnItem = qobject_cast<PawnItem *>(QObject::sender());

    assert(pawnItem);

    Command command{Command::Kind::MovePawn, pawnItem->id().index};

    auto actionFun = [q] (const Action & action)
    {
        q->takeActionAnimated(action);
    };

    if(q->isAnimationRunning() || pawnItem->id().player != q->playerActing())
    {
    }
    else if(event->button() == Qt::LeftButton)
    {
        auto [resultCode, action] = q->createCommandAction(command);

        if(resultCode.success())
        {
            actionFun(*action);
            emit q->commandTaken(command);
        }
    }
    else if(event->button() == Qt::RightButton)
    {
        movePawnActionsApply(game, pawnItem->id().index, actionFun);
        emit q->commandTaken(command);
    }
}

void GameWidgetPrivate::pawnHoverMove(QGraphicsSceneHoverEvent * /*event*/)
{
    Q_Q(GameWidget);

    PawnItem * pawnItem = qobject_cast<PawnItem *>(QObject::sender());

    assert(pawnItem);

    if(!q->isAnimationRunning() && pawnItem->id().player == q->playerActing())
    {
        q->hideActionMarking();
        pawnItem->setHighlight(pawnItem->highlight() | PawnItem::Moving); //TODO move to actionMarkingVisitor
        q->showMovePawnActions(pawnItem->id().index);
    }
}

void GameWidgetPrivate::pawnHoverLeave(QGraphicsSceneHoverEvent * /*event*/)
{
    Q_Q(GameWidget);

    PawnItem * pawnItem = qobject_cast<PawnItem *>(QObject::sender());

    assert(pawnItem);

    if(!q->isAnimationRunning() && pawnItem->id().player == q->playerActing())
        q->hideActionMarking();
}

void GameWidgetPrivate::nestMousePress(QGraphicsSceneMouseEvent * event)
{
    Q_Q(GameWidget);

    NestItem * nestItem = qobject_cast<NestItem *>(QObject::sender());

    assert(nestItem);

    Command command{Command::Kind::Birth};

    auto actionFun = [q] (const Action & action)
    {
        q->takeActionAnimated(action);
    };

    if(q->isAnimationRunning() || nestItem->player() != q->playerActing())
    {
    }
    else if(event->button() == Qt::LeftButton)
    {
        auto [resultCode, action] = q->createCommandAction(command);

        if(resultCode.success())
        {
            actionFun(*action);
            emit q->commandTaken(command);
        }
    }
    else if(event->button() == Qt::RightButton)
    {
        auto [resultCode, action] = q->createCommandAction(command);
        BornPawnSearchVisitor bornPawnSearchVisitor{q};

        if(resultCode.success())
        {
            action->accept(bornPawnSearchVisitor);
            assert(bornPawnSearchVisitor.bornPawnIndex() != -1);
            actionFun(*action);
            movePawnActionsApply(game, bornPawnSearchVisitor.bornPawnIndex(), actionFun);
            emit q->commandTaken(command);
        }
    }
}

void GameWidgetPrivate::nestHoverMove(QGraphicsSceneHoverEvent * /*event*/)
{
    Q_Q(GameWidget);

    NestItem * nestItem = qobject_cast<NestItem *>(QObject::sender());

    assert(nestItem);

    if(!q->isAnimationRunning() && nestItem->player() == q->playerActing())
    {
        q->hideActionMarking();
        q->showBirthActions();
    }
}

void GameWidgetPrivate::nestHoverLeave(QGraphicsSceneHoverEvent * /*event*/)
{
    Q_Q(GameWidget);

    NestItem * nestItem = qobject_cast<NestItem *>(QObject::sender());

    assert(nestItem);

    if(!q->isAnimationRunning() && nestItem->player() == q->playerActing())
        q->hideActionMarking();
}

void GameWidgetPrivate::captivityMousePress(QGraphicsSceneMouseEvent * event)
{
    Q_Q(GameWidget);

    CaptivityItem * captivityItem = qobject_cast<CaptivityItem *>(QObject::sender());

    assert(captivityItem);

    Command command{Command::Kind::Ransom, captivityItem->player()};

    if(q->isAnimationRunning())
    {
    }
    else if(event->button() == Qt::LeftButton || event->button() == Qt::RightButton)
    {
        auto [resultCode, action] = q->createCommandAction(command);

        if(resultCode.success())
        {
            q->takeActionAnimated(*action);
            emit q->commandTaken(command);
        }
    }
}

void GameWidgetPrivate::captivityHoverMove(QGraphicsSceneHoverEvent * /*event*/)
{
    Q_Q(GameWidget);

    CaptivityItem * captivityItem = qobject_cast<CaptivityItem *>(QObject::sender());

    assert(captivityItem);

    if(!q->isAnimationRunning())
    {
        q->hideActionMarking();
        q->showRansomAction(captivityItem->player());
    }
}

void GameWidgetPrivate::captivityHoverLeave(QGraphicsSceneHoverEvent * /*event*/)
{
    Q_Q(GameWidget);

    CaptivityItem * captivityItem = qobject_cast<CaptivityItem *>(QObject::sender());

    assert(captivityItem);

    if(!q->isAnimationRunning())
        q->hideActionMarking();
}

GameWidget::GameWidget(DiceGenerator<dieCount> diceGenerator)
    : d_ptr{new GameWidgetPrivate{this, diceGenerator}}
{
    Q_D(GameWidget);

    startOver({0, 1, 2, 3});

    QRectF sceneRect{QPointF{0, 0}, QPointF{570, 570}};

    d->scene.setSceneRect(sceneRect);
    d->locationSize = QSizeF{37, 37};
    d->horNestSize = QSizeF{500, d->locationSize.height()};
    d->horCaptivitySize = QSizeF{500, d->locationSize.height()};
    d->verNestSize = QSizeF{d->locationSize.width(), 500};
    d->verCaptivitySize = QSizeF{d->locationSize.width(), 500};
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 0}, QPointF{286, 490});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 1}, QPointF{249, 490});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 2}, QPointF{212, 490});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 3}, QPointF{176, 490});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 4}, QPointF{140, 490});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 5}, QPointF{103, 490});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 6}, QPointF{67, 490});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 7}, QPointF{67, 454});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 8}, QPointF{67, 417});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 9}, QPointF{67, 381});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 10}, QPointF{67, 344});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 11}, QPointF{67, 308});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 12}, QPointF{67, 272});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 13}, QPointF{67, 235});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 14}, QPointF{67, 199});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 15}, QPointF{67, 162});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 16}, QPointF{67, 125});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 17}, QPointF{67, 89});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 18}, QPointF{67, 53});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 19}, QPointF{103, 53});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 20}, QPointF{140, 53});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 21}, QPointF{176, 53});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 22}, QPointF{212, 53});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 23}, QPointF{249, 53});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 24}, QPointF{286, 53});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 25}, QPointF{322, 53});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 26}, QPointF{359, 53});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 27}, QPointF{396, 53});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 28}, QPointF{432, 53});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 29}, QPointF{468, 53});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 30}, QPointF{504, 53});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 31}, QPointF{504, 89});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 32}, QPointF{504, 125});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 33}, QPointF{504, 162});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 34}, QPointF{504, 199});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 35}, QPointF{504, 235});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 36}, QPointF{504, 272});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 37}, QPointF{504, 308});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 38}, QPointF{504, 344});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 39}, QPointF{504, 381});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 40}, QPointF{504, 417});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 41}, QPointF{504, 454});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 42}, QPointF{504, 490});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 43}, QPointF{468, 490});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 44}, QPointF{432, 490});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 45}, QPointF{396, 490});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 46}, QPointF{359, 490});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Main}, 47}, QPointF{322, 490});

    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Pen, 0}, 0}, QPointF{176, 454});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Pen, 0}, 1}, QPointF{140, 454});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Pen, 0}, 2}, QPointF{103, 454});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Pen, 1}, 0}, QPointF{103, 162});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Pen, 1}, 1}, QPointF{103, 125});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Pen, 1}, 2}, QPointF{103, 89});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Pen, 2}, 0}, QPointF{396, 89});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Pen, 2}, 1}, QPointF{432, 89});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Pen, 2}, 2}, QPointF{468, 89});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Pen, 3}, 0}, QPointF{468, 381});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Pen, 3}, 1}, QPointF{468, 417});
    d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Pen, 3}, 2}, QPointF{468, 454});

    std::map<std::pair<int, int>, QPointF> sideAndSquareHousePosMap;
    std::vector<QPointF> sideNestPosMap{{286, 670-50}, {-100+50, 272}, {286, -100+50}, {670-50, 272}};
    std::vector<QPointF> sideCaptivityPosMap{{286, 670}, {-100, 272}, {286, -100}, {670, 272}};

    sideAndSquareHousePosMap.emplace(std::make_pair(0, 0), QPointF{286, 454});
    sideAndSquareHousePosMap.emplace(std::make_pair(0, 1), QPointF{286, 417});
    sideAndSquareHousePosMap.emplace(std::make_pair(0, 2), QPointF{286, 381});
    sideAndSquareHousePosMap.emplace(std::make_pair(0, 3), QPointF{286, 344});
    sideAndSquareHousePosMap.emplace(std::make_pair(0, 4), QPointF{286, 308});
    sideAndSquareHousePosMap.emplace(std::make_pair(1, 0), QPointF{103, 272});
    sideAndSquareHousePosMap.emplace(std::make_pair(1, 1), QPointF{140, 272});
    sideAndSquareHousePosMap.emplace(std::make_pair(1, 2), QPointF{176, 272});
    sideAndSquareHousePosMap.emplace(std::make_pair(1, 3), QPointF{212, 272});
    sideAndSquareHousePosMap.emplace(std::make_pair(1, 4), QPointF{249, 272});
    sideAndSquareHousePosMap.emplace(std::make_pair(2, 0), QPointF{286, 89});
    sideAndSquareHousePosMap.emplace(std::make_pair(2, 1), QPointF{286, 125});
    sideAndSquareHousePosMap.emplace(std::make_pair(2, 2), QPointF{286, 162});
    sideAndSquareHousePosMap.emplace(std::make_pair(2, 3), QPointF{286, 199});
    sideAndSquareHousePosMap.emplace(std::make_pair(2, 4), QPointF{286, 235});
    sideAndSquareHousePosMap.emplace(std::make_pair(3, 0), QPointF{468, 272});
    sideAndSquareHousePosMap.emplace(std::make_pair(3, 1), QPointF{432, 272});
    sideAndSquareHousePosMap.emplace(std::make_pair(3, 2), QPointF{396, 272});
    sideAndSquareHousePosMap.emplace(std::make_pair(3, 3), QPointF{359, 272});
    sideAndSquareHousePosMap.emplace(std::make_pair(3, 4), QPointF{322, 272});

    for(int player = 0; player < playerSettings().playerCount(); ++player)
    {
        for(int square = 0; square < pawnsPerPlayer; ++square)
        {
            d->locationExtIdPosMap.
                    emplace(LocationExtId{{Section::Kind::House, player}, square},
                            sideAndSquareHousePosMap.at(
                                    {playerSettings().playerSideMap().at(player), square}));
        }
    }

    for(int player = 0; player < playerSettings().playerCount(); ++player)
    {
        d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Nest}, -1, player},
                                     sideNestPosMap.at(
                                         playerSettings().playerSideMap().at(player)));
    }

    for(int player = 0; player < playerSettings().playerCount(); ++player)
    {
        d->locationExtIdPosMap.emplace(LocationExtId{{Section::Kind::Captivity, player}},
                                     sideCaptivityPosMap.at(
                                         playerSettings().playerSideMap().at(player)));
    }

    setPlayerPawnPixmapMap({QPixmap{R"(D:\Misc\Projects\VB\Старые\Jumanji\Графа\NEW\glass.png)"},
                            QPixmap{R"(D:\Misc\Projects\VB\Старые\Jumanji\Графа\NEW\glass2.png)"},
                            QPixmap{R"(D:\Misc\Projects\VB\Старые\Jumanji\Графа\NEW\glass3.png)"},
                            QPixmap{R"(D:\Misc\Projects\VB\Старые\Jumanji\Графа\NEW\ACP1.png)"}});

    d->pawnItemCommonProps.locationExtIdPosFun =
    d->arrowItemCommonProps.locationExtIdPosFun =
    d->markerItemCommonProps.locationExtIdPosFun = [d](LocationExtId locationExtId)
    {
        return d->locationExtIdPos(locationExtId);
    };

    d->pawnItemCommonProps.playerPawnPixmapFun = [d](int player)
    {
        return d->playerPawnPixmapMap.at(player);
    };

    d->arrowItemCommonProps.arrowSize = 10;
    d->arrowItemCommonProps.penWidth = 2;

    d->markerItemCommonProps.locationSize = d->locationSize;
    d->markerItemCommonProps.penWidth = 3;

    d->nestItemCommonProps.penWidthHighlight = 3;
    d->nestItemCommonProps.penWidthNoHighlight = 1;

    d->captivityItemCommonProps.penWidthHighlight = 3;
    d->captivityItemCommonProps.penWidthNoHighlight = 1;

    for(int player = 0; player < playerSettings().playerCount(); ++player)
    {
        for(int index = 0; index < pawnsPerPlayer; ++index)
        {
            d->addPawnItem(new PawnItem{&d->pawnItemCommonProps, {player, index},
                                        {{Section::Kind::Nest}}});
        }
    }

    for(int player = 0; player < playerSettings().playerCount(); ++player)
    {
        int side = playerSettings().playerSideMap().at(player);
        NestItem * nestItem = new NestItem{&d->nestItemCommonProps, player};
        CaptivityItem * captivityItem = new CaptivityItem{&d->captivityItemCommonProps, player};

        nestItem->setRect(QRectF{d->locationExtIdPos({{Section::Kind::Nest}, -1, player}),
                                 QSize{0, 0}});
        captivityItem->setRect(QRectF{d->locationExtIdPos({{Section::Kind::Captivity, player}}),
                                      QSize{0, 0}});

        if(side % 2 == 0)
        {
            nestItem->setRect(nestItem->rect().adjusted(
                                -d->horNestSize.width() / 2,
                                -d->horNestSize.height () / 2,
                                d->horNestSize.width() / 2,
                                d->horNestSize.height() / 2));
            captivityItem->setRect(captivityItem->rect().adjusted(
                                -d->horCaptivitySize.width() / 2,
                                -d->horCaptivitySize.height () / 2,
                                d->horCaptivitySize.width() / 2,
                                d->horCaptivitySize.height() / 2));
        }
        else
        {
            nestItem->setRect(nestItem->rect().adjusted(
                                -d->verNestSize.width() / 2,
                                -d->verNestSize.height () / 2,
                                d->verNestSize.width() / 2,
                                d->verNestSize.height() / 2));
            captivityItem->setRect(captivityItem->rect().adjusted(
                                -d->verCaptivitySize.width() / 2,
                                -d->verCaptivitySize.height () / 2,
                                d->verCaptivitySize.width() / 2,
                                d->verCaptivitySize.height() / 2));
        }

        d->addNestItem(nestItem);
        d->addCaptivityItem(captivityItem);
    }

    d->scene.setBackgroundBrush(QBrush(QPixmap{R"(D:\Misc\Projects\VB\Старые\Jumanji\Графа\Доска\kletki5.bmp)"}));

    setCacheMode(CacheBackground);
    setViewportUpdateMode(BoundingRectViewportUpdate);
    setRenderHint(QPainter::Antialiasing);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setAlignment(Qt::AlignCenter);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(570);
    setMinimumWidth(570);
    setMouseTracking(true);
    setScene(&d->scene);

    connect(&d->mainWindow, &MainWindow::actionShown, [this](const Action & action)
    {
        hideActionMarking();
        showAction(action);
    });

    connect(&d->mainWindow, &MainWindow::actionTaken, [this](const Action & action)
    {
        takeActionAnimated(action);
    });

    connect(this, &GameWidget::actionTaken, &d->mainWindow, &MainWindow::showGame);

    connect(this, &GameWidget::animationFinished, [this]()
    {
        bool stop = true;

        do
        {
            stop = true;

            auto [skipResultCode, skipAction] = this->createCommandAction({Command::Kind::Skip});

            if(skipResultCode.success())
            {
                stop = false;
                takeAction(*skipAction);
                emit commandTaken({Command::Kind::Skip});
                refreshPawns();
            }

            auto [rollDiceResultCode, rollDiceAction] =
                    this->createCommandAction({Command::Kind::RollDice});

            if(rollDiceResultCode.success())
            {
                stop = false;
                takeAction(*rollDiceAction);
                emit commandTaken({Command::Kind::RollDice});
                refreshPawns();
            }

            if(stop && playerActing() != 0)
                doCommand();
        } while(!stop);
    });

    /*takeAction(*Action::create<ActionPawnRelocation>
                       (PawnId{1, 0}, LocationId{{Section::Kind::Main}, 12}));*/

    refreshPawns();
    d->mainWindow.show();
    d->mainWindow.showGame();

    /*for(int i = 0; i < 53; ++i) //TODO REMOVE
        qDebug() << i << previousTransition(i).first;*/
/*        double pMax = 0;
    int iMax, jMax;

    for(int i = 0; i < 53; ++i)
    {
        for(int j = 0; j < 53; ++j)
        {
            double p = distanceChance(d->game, 0, i, j - i, std::numeric_limits<long>::max());

            qDebug() << i << j << p;
            if(p > pMax)
            {
                iMax = i;
                jMax = j;
                pMax = p;
            }
        }
    }

    qDebug() << iMax << jMax << pMax;*/
    //qDebug() << distanceChance(d->game, 0, 0, 11, std::numeric_limits<long>::max());

    /*takeAction(*rollDiceAction().second);
    takeAction(*Action::create<ActionPawnRelocation>
               (PawnId{0, 0}, LocationId{{Section::Kind::Captivity, 1}}));
    refreshPawns();
    auto lol = buildCommandTree(d->game);
    auto lol2 = chooseCommandSequence(d->game);*/
}

GameWidget::~GameWidget()
{
    Q_D(GameWidget);

    delete d;
}

bool GameWidget::isFinished() const
{
    Q_D(const GameWidget);

    return d->game.isFinished();
}

const Board & GameWidget::board() const
{
    Q_D(const GameWidget);

    return d->game.board();
}

int GameWidget::playerActing() const
{
    Q_D(const GameWidget);

    return d->game.playerActing();
}

int GameWidget::playerWithTurn() const
{
    Q_D(const GameWidget);

    return d->game.playerWithTurn();
}

const PlayerSettings & GameWidget::playerSettings() const
{
    Q_D(const GameWidget);

    return d->game.playerSettings();
}

const Dice<dieCount> & GameWidget::dice() const
{
    Q_D(const GameWidget);

    return d->game.dice();
}

int GameWidget::diceUsed() const
{
    Q_D(const GameWidget);

    return d->game.diceUsed();
}

std::pair<CommandResultCode, ConstActionSptr> GameWidget::createCommandAction(Command command) const
{
    Q_D(const GameWidget);

    auto commandIter = d->cachedCommands.find(command);

    if(commandIter != d->cachedCommands.cend())
        return commandIter->second;

    return d->cachedCommands[command] = d->game.createCommandAction(command);
}

std::vector<std::pair<Command, ActionUptr>> GameWidget::availableCommands() const
{
    Q_D(const GameWidget);

    return d->game.availableCommands();
}

void GameWidget::startOver(std::vector<int> playerSideMap)
{
    Q_D(GameWidget);

    finishAnimation();
    hideAllMarking();
    d->clearHistory();
    d->clearCommandCache();
    d->game.startOver(playerSideMap);
    d->gameVisual.startOver(std::move(playerSideMap));
}

void GameWidget::reset()
{
    startOver({});
}

template<class T> void GameWidget::setDiceGenerator(T value)
{
    Q_D(GameWidget);

    d->game.setDiceGenerator(value);
}

int GameWidget::squareSide(int mainOrRelSquare)
{
    return Game::squareSide(mainOrRelSquare);
}

int GameWidget::penEntrySquare(int side)
{
    return Game::penEntrySquare(side);
}

int GameWidget::penExitSquare(int side)
{
    return Game::penExitSquare(side);
}

int GameWidget::jumpDestinationSquare(int mainOrRelSquare)
{
    return Game::jumpDestinationSquare(mainOrRelSquare);
}

bool GameWidget::isSquarePenEntry(int mainOrRelSquare)
{
    return Game::isSquarePenEntry(mainOrRelSquare);
}

bool GameWidget::isSquarePenExit(int mainOrRelSquare)
{
    return Game::isSquarePenExit(mainOrRelSquare);
}

bool GameWidget::isLocationSafe(LocationId locationId)
{
    return Game::isLocationSafe(locationId);
}

int sideToRelSide(int side, int baseSide)
{
    return Game::sideToRelSide(side, baseSide);
}

int relSideToSide(int relSide, int baseSide)
{
    return Game::relSideToSide(relSide, baseSide);
}

int GameWidget::relSquareToMainSquare(int relSquare, int player)
{
    return Game::relSquareToMainSquare(relSquare, player);
}

int GameWidget::mainSquareToRelSquare(int mainSquare, int player)
{
    return Game::mainSquareToRelSquare(mainSquare, player);
}

int GameWidget::locationIdToRelSquare(LocationId locationId, int side)
{
    return Game::locationIdToRelSquare(locationId, side);
}

LocationId GameWidget::relSquareToLocationId(int relSquare, int side, int player)
{
    return Game::relSquareToLocationId(relSquare, side, player);
}

bool GameWidget::isAnimationRunning() const
{
    Q_D(const GameWidget);

    return d->animationSequence != nullptr;
}

const std::vector<QPixmap> & GameWidget::playerPawnPixmapMap() const
{
    Q_D(const GameWidget);

    return d->playerPawnPixmapMap;
}

void GameWidget::setPlayerPawnPixmapMap(std::vector<QPixmap> value)
{
    Q_D(GameWidget);

    d->playerPawnPixmapMap = std::move(value);
    for(const auto & [pawnId, pawnItem] : d->pawnItems)
    {
        Q_UNUSED(pawnId);
        pawnItem->updatePixmap();
    }
}

void GameWidget::doCommand()
{
   /* Q_D(GameWidget);

    auto commandSequence = chooseCommandSequence(d->game);

    if(commandSequence.empty())
        return;
    takeActionAnimated(*commandSequence.front().second);*/
}

void GameWidget::finishAnimation()
{
    Q_D(GameWidget);

    d->finishAnimation();
}

void GameWidget::takeAction(const Action & action)
{
    std::list<ConstActionUptr> actions;

    actions.emplace_back(action.clone());
    takeActions(actions);
}

void GameWidget::takeActions(const std::list<ConstActionUptr> & actions)
{
    Q_D(GameWidget);

    finishAnimation();
    d->takeActionsAux(actions);
    for(const auto & action : actions)
        d->gameVisual.takeAction(*action);
}

void GameWidget::takeActionAnimated(const Action & action)
{
    std::list<ConstActionUptr> actions;

    actions.emplace_back(action.clone());
    takeActionsAnimated(actions);
}

void GameWidget::takeActionsAnimated(const std::list<ConstActionUptr> & actions)
{
    Q_D(GameWidget);

    hideAllMarking();
    d->takeActionsAux(actions);
    d->addAnimation(actions);
}

void GameWidget::undoActions(int count)
{
    Q_D(GameWidget);

    int stopIndex = std::max(d->nextActionIndex - count, 0);

    d->clearCommandCache();
    finishAnimation();

    while(d->nextActionIndex > stopIndex)
    {
        --d->nextActionIndex;
        --d->nextActionIterator;
        d->gameVisual.takeAction(**d->nextActionIterator);
        *d->nextActionIterator = d->game.takeAction(**d->nextActionIterator);
        emit actionTaken(**d->nextActionIterator);
    }
}

void GameWidget::redoActions(int count)
{
    Q_D(GameWidget);

    int stopIndex = std::min(d->nextActionIndex + count, int(d->actionHistory.size())); //TODO narrowing conversion, what do?

    d->clearCommandCache();
    finishAnimation();

    while(d->nextActionIndex < stopIndex)
    {
        d->gameVisual.takeAction(**d->nextActionIterator);
        *d->nextActionIterator = d->game.takeAction(**d->nextActionIterator);
        emit actionTaken(**d->nextActionIterator);
        ++d->nextActionIndex;
        ++d->nextActionIterator;
    }
}

void GameWidget::showAction(const Action & action)
{
    if(isAnimationRunning())
        return;

    ActionMarkingVisitor actionMarkingVisitor{this};

    action.accept(actionMarkingVisitor);
}

void GameWidget::showMovePawnAction(int pawnIndex)
{
    if(isAnimationRunning())
        return;

    ActionMarkingVisitor actionMarkingVisitor{this};
    auto [resultCode, action] = createCommandAction({Command::Kind::MovePawn, pawnIndex});

    if(resultCode.success())
        action->accept(actionMarkingVisitor);
}

void GameWidget::showMovePawnActions(int pawnIndex)
{
    Q_D(GameWidget);

    if(isAnimationRunning())
        return;

    ActionMarkingVisitor actionMarkingVisitor{this};
    std::list<ConstActionUptr> inverseActions;

    movePawnActionsApply(d->game, pawnIndex, [d, &actionMarkingVisitor, &inverseActions]
                         (const Action & action)
    {
        action.accept(actionMarkingVisitor);
        inverseActions.emplace_front(d->game.takeAction(action));
    });

    for(ConstActionUptr & inverseAction : inverseActions)
        d->game.takeAction(*inverseAction);
}

void GameWidget::showBirthAction()
{
    if(isAnimationRunning())
        return;

    ActionMarkingVisitor actionMarkingVisitor{this};
    auto [resultCode, action] = createCommandAction({Command::Kind::Birth});

    if(resultCode.success())
        action->accept(actionMarkingVisitor);
}

void GameWidget::showBirthActions()
{
    Q_D(GameWidget);

    if(isAnimationRunning())
        return;

    ActionMarkingVisitor actionMarkingVisitor{this};
    std::list<ConstActionUptr> inverseActions;
    auto [resultCode, action] = createCommandAction({Command::Kind::Birth});

    auto actionFun = [d, &actionMarkingVisitor, &inverseActions]
            (const Action & action)
    {
        action.accept(actionMarkingVisitor);
        inverseActions.emplace_front(d->game.takeAction(action));
    };

    if(resultCode.success())
    {
        BornPawnSearchVisitor bornPawnSearchVisitor{this};

        action->accept(bornPawnSearchVisitor);
        assert(bornPawnSearchVisitor.bornPawnIndex() != -1);
        actionFun(*action);
        movePawnActionsApply(d->game, bornPawnSearchVisitor.bornPawnIndex(), actionFun);
    }

    for(ConstActionUptr & inverseAction : inverseActions)
        d->game.takeAction(*inverseAction);
}

void GameWidget::showRansomAction(int captorPlayer)
{
    if(isAnimationRunning())
        return;

    ActionMarkingVisitor actionMarkingVisitor{this};
    auto [resultCode, action] = createCommandAction({Command::Kind::Ransom, captorPlayer});

    if(resultCode.success())
        action->accept(actionMarkingVisitor);
}

void GameWidget::hideActionMarking()
{
    Q_D(GameWidget);

    for(auto & [pawnId, pawnItem] : d->pawnItems)
    {
        Q_UNUSED(pawnId);

        pawnItem->setHighlight(pawnItem->highlight() &
                               ~(PawnItem::Moving | PawnItem::Captured));
    }

    for(ArrowItem * arrowItem : d->arrowItems)
    {
        d->scene.removeItem(arrowItem);
        delete arrowItem;
    }

    d->arrowItems.clear();

    for(MarkerItem * markerItem : d->markerItems)
    {
        d->scene.removeItem(markerItem);
        delete markerItem;
    }

    d->markerItems.clear();

    for(int player = 0; player < playerSettings().playerCount(); ++player)
    {
        d->playerNestItemMap.at(player)->setHighlight(false);
        d->playerCaptivityItemMap.at(player)->setHighlight(false);
    }
}

void GameWidget::hideAllMarking()
{
    Q_D(GameWidget);

    hideActionMarking();

    for(auto & [pawnId, pawnItem] : d->pawnItems)
    {
        Q_UNUSED(pawnId);

        pawnItem->setHighlight(PawnItem::NoHighlight); //TODO keep Tired?
    }
}

void GameWidget::refreshPawns()
{
    Q_D(GameWidget);

    for(const auto & [pawnId, pawn] : d->gameVisual.board().pawns())
    {
        PawnItem * pawnItem = d->pawnItems.at(pawnId);

        pawnItem->setZValue(0);
        pawnItem->setHighlight(pawn.tired ? pawnItem->highlight() | PawnItem::Tired :
                                            pawnItem->highlight() & !PawnItem::Tired);
        pawnItem->moveToLocationId(pawn.locationId);
    }

    int playerCount = playerSettings().playerCount();
    LocationId nestId{{Section::Kind::Nest}};
    const Location & nest = d->gameVisual.board().location(nestId);
    std::vector<QPointF> playerNestPosMap(playerCount);
    std::vector<QPointF> playerCaptivityPosMap(playerCount);

    for(int player = 0; player < playerCount; ++player)
    {
        LocationExtId nestExtId{{Section::Kind::Nest}, -1, player};
        LocationId captivityId{{Section::Kind::Captivity, player}};
        const Location & captivity = d->gameVisual.board().location(captivityId);
        LocationExtId captivityExtId{{Section::Kind::Captivity, player}};

        playerNestPosMap[player] = d->locationExtIdPos(nestExtId);
        playerCaptivityPosMap[player] = d->locationExtIdPos(captivityExtId);
        d->refreshPawnsAux(captivity, playerCaptivityPosMap,
                           [player](PawnId) { return player; });
    }

    d->refreshPawnsAux(nest, playerNestPosMap, [](PawnId pawnId) { return pawnId.player; });
}

}

