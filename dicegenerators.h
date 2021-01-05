#ifndef DICEGENERATORS_H
#define DICEGENERATORS_H

#include <functional>
#include <utility>

#include "dice.h"

namespace parchis::_private
{

class DefaultDieGeneratorBase
{
public:
    DefaultDieGeneratorBase(int dieSideCount);
    DefaultDieGeneratorBase(int dieSideCount, unsigned int seed);
    DefaultDieGeneratorBase(const DefaultDieGeneratorBase & other);
    DefaultDieGeneratorBase(DefaultDieGeneratorBase && other) noexcept
        : _d{other._d} { other._d = nullptr; }

    ~DefaultDieGeneratorBase();

    DefaultDieGeneratorBase & operator=(const DefaultDieGeneratorBase & other);
    DefaultDieGeneratorBase & operator=(DefaultDieGeneratorBase && other) noexcept
        { swap(other); return *this; }

    int operator()() noexcept;

    void swap(DefaultDieGeneratorBase & other) noexcept
        { std::swap(_d, other._d); }

private:
    struct Private;

    Private * d() { return _d; }
    const Private * d() const { return _d; }

    Private * _d;
};

}

namespace parchis
{

template <int dieCount>
using DiceGenerator = std::function<Dice<dieCount>()>;

template <int dieSideCount>
class DefaultDieGenerator final : public _private::DefaultDieGeneratorBase
{
public:
    DefaultDieGenerator() : DefaultDieGeneratorBase{dieSideCount} {}
    DefaultDieGenerator(unsigned int seed) : DefaultDieGeneratorBase{dieSideCount, seed} {}

    int operator()() { return DefaultDieGeneratorBase::operator()(); }
};

template <int dieSideCount, int dieCount>
class DefaultDiceGenerator final
{
public:
    DefaultDiceGenerator() = default;
    DefaultDiceGenerator(unsigned int seed) : _dieGenerator{seed} {}

    Dice<dieCount> operator()()
    {
        Dice<dieCount> dice;

        for(auto & die : dice)
            die = _dieGenerator();
        return dice;
    }

private:
    DefaultDieGenerator<dieSideCount> _dieGenerator;
};

inline void swap(parchis::_private::DefaultDieGeneratorBase & a,
                 parchis::_private::DefaultDieGeneratorBase & b)
{
    a.swap(b);
}

}

#endif // DICEGENERATORS_H
