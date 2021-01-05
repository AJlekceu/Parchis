#include "playerparticipation.h"

namespace parchis
{
    int PlayerParticipationBase::nextPlayerPlaying(int player) const
    {
        if(playersPlayingSet().empty())
            return -1;

        auto iter = playersPlayingSet().upper_bound(player);

        return iter == playersPlayingSet().cend() ? *playersPlayingSet().cbegin() : *iter;
    }

    void PlayerParticipationBase::startOver(PlayersMap playersStartingMap)
    {
        _playersStartedMap = playersStartingMap;
        _playersFinishedMap.reset();
        _playersPlayingSet.clear();
        _playersFinishedList.clear();

        for(int i = 0; i < playersStartedMap().size(); ++i)
        {
            if(playersStartedMap()[i])
                _playersPlayingSet.emplace_hint(_playersPlayingSet.cend(), i);
        }
    }

    bool PlayerParticipationBase::setPlayerFinished(int player, bool value)
    {
        if(player < 0 || player >= numberOfSides || !playersStartedMap()[player])
            return false;

        if(value)
        {
            if(playersFinishedMap()[player])
                return false;
            _playersFinishedMap.set(player);
            _playersPlayingSet.erase(player);
            _playersFinishedList.emplace_back(player);
            return true;
        }
        else
        {
            if(!playersFinishedMap()[player])
                return false;
            _playersFinishedMap.reset(player);
            _playersPlayingSet.emplace(player);
            _playersFinishedList.erase(std::find(_playersFinishedList.cbegin(), _playersFinishedList.cend(), player));
            return true;
        }
    }
}
