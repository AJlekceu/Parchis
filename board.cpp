#include "board.h"

#include <limits>

namespace parchis
{

int Location::findPawnIndex(int player) const
{
    auto firstPawnIter = pawnIds.lower_bound({player, 0});

    if(firstPawnIter == pawnIds.cend() || firstPawnIter->player != player)
        return -1;
    return firstPawnIter->index;
}

PawnId Location::firstPawnId() const
{
    if(pawnCount() == 0)
        throw std::out_of_range("Location contains no pawns");
    return *pawnIds.cbegin();
}

std::size_t Location::pawnCount(int player) const
{
    const auto maxForPawnIndexType
            = std::numeric_limits<decltype(std::declval<PawnId>().index)>::max();
    auto firstPawnIter = pawnIds.lower_bound({player, 0});

    if(firstPawnIter == pawnIds.cend() || firstPawnIter->player != player)
        return 0;

    auto secondPawnIter =
            pawnIds.upper_bound({player, maxForPawnIndexType});

    return static_cast<std::size_t>(std::distance(firstPawnIter, secondPawnIter));
}

void Board::reset()
{
    _pawns.clear();
    _locations.clear();
    _tiredPawnIds.clear();
}

void Board::relocatePawn(PawnId pawnId, LocationId destLocationId)
{
    Pawn & pawn = _pawns.at(pawnId);
    Location & destLocation = _locations.at(destLocationId);
    Location & srcLocation = _locations.at(pawn.locationId);

    destLocation.pawnIds.emplace(pawnId);
    srcLocation.pawnIds.erase(pawnId);
    pawn.locationId = destLocationId;
}

void Board::setPawnTired(PawnId pawnId, bool value)
{
    bool & pawnTired = _pawns.at(pawnId).tired;

    if(value)
        _tiredPawnIds.emplace(pawnId);
    else
        _tiredPawnIds.erase(pawnId);
    pawnTired = value;
}

}
