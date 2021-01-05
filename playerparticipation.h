#ifndef PLAYERPARTICIPATION_H
#define PLAYERPARTICIPATION_H

#include <bitset>
#include <set>
#include <vector>

namespace parchis
{
    template<int maxNumberOfPlayers>
    class PlayerParticipationBase
    {
    public:
        using PlayersMap = std::bitset<maxNumberOfPlayers>;

        PlayersMap playersStartedMap() const { return _playersStartedMap; }
        PlayersMap playersFinishedMap() const { return _playersFinishedMap; }
        PlayersMap playersPlayingMap() const { return playersStartedMap() & ~playersFinishedMap(); }
        const std::set<int> & playersPlayingSet() const { return _playersPlayingSet; }
        const std::vector<int> & playersFinishedList() const { return _playersFinishedList; }
        int firstPlayerPlaying() const { return nextPlayerPlaying(-1); }
        int nextPlayerPlaying(int player) const;

        void reset() { startOver(PlayersMap{}); }
        void startOver(PlayersMap playersStartingMap);
        bool setPlayerFinished(int player, bool value);

    private:
        PlayersMap _playersStartedMap;
        PlayersMap _playersFinishedMap;
        std::set<int> _playersPlayingSet;
        std::vector<int> _playersFinishedList;
    };
}

#endif // PLAYERPARTICIPATION_H
