#ifndef BOARD_H
#define BOARD_H

#include <cstddef>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "utilities.h"

namespace parchis
{

struct PawnId;
struct LessForPawnIds;
struct LocationId;
struct LessForLocationIds;
struct Pawn;
struct Location;
class Board;

}


namespace std
{

template<>
struct hash<parchis::PawnId>
{
    std::size_t operator()(parchis::PawnId pawnId) const;
};

template<>
struct hash<parchis::LocationId>
{
    std::size_t operator()(parchis::LocationId locationId) const;
};

}


namespace parchis
{

struct Section
{
    enum class Kind
    {
        Main,
        Pen,
        Nest,
        Captivity,
        House
    };

    Section(Kind _kind = Kind::Nest, int _index = -1) : kind{_kind}, index{_index} {}

    Kind kind;
    int index;
};

struct PawnId
{
    PawnId() : PawnId{-1, -1} {}
    PawnId(int _player, int _index) : player{_player}, index{_index} {}

    int player;
    int index;
};

struct LessForPawnIds
{
    bool operator()(PawnId pawnId1, PawnId pawnId2) const
    {
        return std::tie(pawnId1.player, pawnId1.index) <
               std::tie(pawnId2.player, pawnId2.index);
    }
};

struct LocationId
{
    LocationId(Section _section = Section{}, int _square = -1)
        : section{_section}, square{_square} {}

    Section section;
    int square;
};

struct LessForLocationIds
{
    bool operator()(LocationId locationId1, LocationId locationId2) const
    {
        return std::tie(
                    locationId1.section.kind, locationId1.section.index, locationId1.square) <
               std::tie(
                    locationId2.section.kind, locationId2.section.index, locationId2.square);
    }
};

struct Pawn
{
    Pawn(LocationId _locationId = LocationId{}, bool _tired = false)
        : locationId{_locationId}, tired{_tired} {}

    LocationId locationId;
    bool tired;
};

struct Location
{
public:
    int findPawnIndex(int player) const;
    PawnId firstPawnId() const;
    std::size_t pawnCount(int player) const;
    std::size_t pawnCount() const { return pawnIds.size(); }
    bool hasPawns(int player) const { return findPawnIndex(player) != -1; }
    bool hasPawns() const { return pawnCount() != 0; }

    std::set<PawnId, LessForPawnIds> pawnIds; //TODO не вести учёт конкретных ID, а лишь количества?
};

class Board
{
public:
    //TODO: consider replacing unordered_map with something faster
    const std::unordered_map<PawnId, Pawn> & pawns() const { return _pawns; }
    const std::unordered_map<LocationId, Location> & locations() const { return _locations; }
    //TODO: consider removing if unused
    const std::unordered_set<PawnId> & tiredPawnIds() const { return _tiredPawnIds; }
    const Pawn & pawn(PawnId pawnId) const { return pawns().at(pawnId); }
    const Location & location(LocationId locationId) const
        { return locations().at(locationId); }

    void reset();

    template<class PawnIds, class LocationIds>
    void initialize(const PawnIds & pawnIds, const LocationIds & locationIds);

    void relocatePawn(PawnId pawnId, LocationId destLocationId);
    void setPawnTired(PawnId pawnId, bool value);

private:
    std::unordered_map<PawnId, Pawn> _pawns;
    std::unordered_map<LocationId, Location> _locations;
    std::unordered_set<PawnId> _tiredPawnIds;
};

template<class PawnIds, class LocationIds>
void Board::initialize(const PawnIds & pawnIds, const LocationIds & locationIds)
{
    reset();

    for(LocationId locationId : locationIds)
        _locations.emplace(std::piecewise_construct,
                           std::forward_as_tuple(locationId),
                           std::forward_as_tuple());

    for(PawnId pawnId : pawnIds)
    {
        _pawns.emplace(std::piecewise_construct,
                       std::forward_as_tuple(pawnId),
                       std::forward_as_tuple(Pawn{{{Section::Kind::Nest}}, false}));
        _locations.at({{Section::Kind::Nest}}).pawnIds.emplace(pawnId);
    }
}

inline bool operator==(Section section1, Section section2)
{
    return std::tie(section1.kind, section1.index) == std::tie(section2.kind, section2.index);
}

inline bool operator!=(Section section1, Section section2)
{
    return !(section1 == section2);
}

inline bool operator==(PawnId pawnId1, PawnId pawnId2)
{
    return std::tie(pawnId1.player, pawnId1.index) == std::tie(pawnId2.player, pawnId2.index);
}

inline bool operator!=(PawnId pawnId1, PawnId pawnId2)
{
    return !(pawnId1 == pawnId2);
}

inline bool operator==(LocationId locationId1, LocationId locationId2)
{
    return std::tie(
                locationId1.section.kind, locationId1.section.index, locationId1.square) ==
            std::tie(
                locationId2.section.kind, locationId2.section.index, locationId2.square);
}

inline bool operator!=(LocationId locationId1, LocationId locationId2)
{
    return !(locationId1 == locationId2);
}

}


namespace std
{

inline std::size_t hash<parchis::PawnId>::operator()(parchis::PawnId pawnId) const
{
    std::size_t seed = pawnId.player;
    hash_combine(seed, pawnId.index);

    return seed;
}

inline std::size_t hash<parchis::LocationId>::operator()(parchis::LocationId locationId) const
{
    std::size_t seed = static_cast<std::size_t>(locationId.section.kind);
    hash_combine(seed, locationId.section.index);
    hash_combine(seed, locationId.square);

    return seed;
}

}

#endif // BOARD_H
