#ifndef COMMANDRESULT_H
#define COMMANDRESULT_H

#include <memory>
#include <tuple>

#include "utilities.h"

namespace parchis
{

enum class SkipResultCode
{
    Success,
    FailGameFinished,
    FailAllDiceUsed,
    FailSomeCommandsAvailable
};

enum class RollDiceResultCode
{
    Success,
    FailGameFinished,
    FailPlayerFinished,
    FailNotAllDiceUsed
};

enum class MovePawnResultCode
{
    Success,
    FailGameFinished,
    FailPlayerFinished,
    FailAllDiceUsed,
    FailTired,
    FailInNest,
    FailCaptured,
    FailInPenAndWrongDie,
    FailEndLocationNonexistent,
    FailEndLocationWithFriendlyPawn,
    FailEndLocationSafeWithEnemyPawn,
    FailPathBlocked
};

enum class BirthResultCode
{
    Success,
    FailGameFinished,
    FailPlayerFinished,
    FailAllDiceUsed,
    FailOnlyAllowedOnYourTurn,
    FailWrongDie,
    FailNoPawns,
    FailEndLocationWithFriendlyPawn,
    FailEndLocationSafeWithEnemyPawn,
};

enum class RansomResultCode
{
    Success,
    FailGameFinished,
    FailPlayerFinished,
    FailAllDiceUsed,
    FailSelf,
    FailPlayerDoesNotExist,
    FailOnlyAllowedOnYourTurn,
    FailWrongDie,
    FailNoPawns
};

union CommandResultCode
{
    CommandResultCode(SkipResultCode _code = SkipResultCode::Success) : skip{_code} {}
    CommandResultCode(RollDiceResultCode _code) : rollDice{_code} {}
    CommandResultCode(MovePawnResultCode _code) : movePawn{_code} {}
    CommandResultCode(BirthResultCode _code) : birth{_code} {}
    CommandResultCode(RansomResultCode _code) : ransom{_code} {}

    bool success() const
    {
        return skip == SkipResultCode::Success;
    }

    SkipResultCode skip;
    RollDiceResultCode rollDice;
    MovePawnResultCode movePawn;
    BirthResultCode birth;
    RansomResultCode ransom;
};

struct Command
{
    enum class Kind
    {
        Skip,
        RollDice,
        MovePawn,
        Birth,
        Ransom
    };

    Command() = default;
    Command(Kind _kind, int _param = -1) : kind{_kind}, param{_param} {}

    Kind kind;
    int param;
};

inline bool operator==(Command command1, Command command2)
{
    return std::tie(command1.kind, command1.param) == std::tie(command2.kind, command2.param);
}

inline bool operator!=(Command command1, Command command2)
{
    return !(command1 == command2);
}

}


namespace std
{

template<>
struct hash<parchis::Command>
{
    std::size_t operator()
        (parchis::Command command) const
    {
        std::size_t seed = static_cast<std::size_t>(command.kind);
        hash_combine(seed, command.param);

        return seed;
    }
};

}
#endif // COMMANDRESULT_H
