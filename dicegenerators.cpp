#include "dicegenerators.h"

#include <random>
#include <utility>

namespace parchis::_private
{

struct DefaultDieGeneratorBase::Private
{
    Private(int dieSideCount)
        : distribution{1, dieSideCount} {}
    Private(int dieSideCount, unsigned int seed)
        : generator{seed}, distribution{1, dieSideCount} {}

    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution;
};

DefaultDieGeneratorBase::DefaultDieGeneratorBase(
        int dieSideCount)
    : _d{new Private{dieSideCount}}
{ }

DefaultDieGeneratorBase::DefaultDieGeneratorBase(
        int dieSideCount, unsigned int seed)
    : _d{new Private{dieSideCount, seed}}
{ }

DefaultDieGeneratorBase::DefaultDieGeneratorBase(
        const DefaultDieGeneratorBase & other)
    : _d{new Private{*other._d}}
{ }

DefaultDieGeneratorBase::~DefaultDieGeneratorBase()
{
    delete d();
}

DefaultDieGeneratorBase & DefaultDieGeneratorBase::operator=(
        const DefaultDieGeneratorBase & other)
{
    DefaultDieGeneratorBase copy{other};

    swap(copy);
    return *this;
}

int DefaultDieGeneratorBase::operator()() noexcept
{
    return d()->distribution(d()->generator);
}

}
