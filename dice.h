#ifndef DICE_H
#define DICE_H

#include <array>

namespace parchis
{

template <int dieCount>
using Dice = std::array<int, dieCount>;

}

#endif // DICE_H
