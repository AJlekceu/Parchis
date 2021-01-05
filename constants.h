#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <array>

namespace parchis
{

const int pawnsPerPlayer = 5;
const int dieSideCount = 6;
const int sideCount = 4;
const int dieCount = 2;
const int squaresInSide = 12;
const int squaresInMain = squaresInSide * sideCount;
const int squaresInPen = 3;
const int relSquaresCount = squaresInMain + pawnsPerPlayer;
const int birthDieValue = 6;
const int ransomDieValue = 6;
const int penEntryDistanceFromOrigin = 3;
const int penExitDistanceFromOrigin = 5;
const int jumpDistanceFromOrigin = 2;
constexpr const std::array<int, squaresInPen> penDieValues = {1, 3, 6};

static_assert(squaresInSide % 2 == 0, "squaresInSide should be even");
static_assert(squaresInPen > 1, "squaresInPen should be greater than 1");
static_assert(penEntryDistanceFromOrigin > 0,
              "penEntryDistanceFromOrigin should be greater than 0");
static_assert(penExitDistanceFromOrigin <= squaresInSide / 2,
              "penExitDistanceFromOrigin should be less than or equal to squaresInSide / 2");

}

#endif // CONSTANTS_H
