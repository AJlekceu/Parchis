#ifndef ACTION_H
#define ACTION_H

#include <memory>

#include "visitor.h"

#define CLONABLE() \
    protected: \
    auto cloneImpl() const -> decltype(clone()) override \
    { return decltype(clone()){new std::decay_t<decltype(*this)>{*this}}; } \
    private:

namespace parchis
{

struct GameState;
class Action;
using ActionUptr = std::unique_ptr<Action>;
using ConstActionUptr = std::unique_ptr<const Action>;
using ActionSptr = std::shared_ptr<Action>;
using ConstActionSptr = std::shared_ptr<const Action>;

class Action : public Loki::BaseVisitable<void, Loki::DefaultCatchAll, true>
{
public:
    LOKI_DEFINE_CONST_VISITABLE()

    virtual ~Action() = default;
    virtual ActionUptr commit(GameState & gameState) const = 0;

    ActionUptr clone() const { return cloneImpl(); }

    template <class ActionT, class ... Args>
    static ActionUptr create(Args && ... args)
    {
        return ActionUptr{new ActionT{std::forward<Args>(args) ...}};
    }

protected:
    virtual ActionUptr cloneImpl() const = 0;
};

}

#endif // ACTION_H
