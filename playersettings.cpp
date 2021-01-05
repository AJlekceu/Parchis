#include "playersettings.h"

#include <algorithm>

namespace parchis
{

int PlayerSettings::nextPlayerPlaying(int player) const
{
    if(playersPlayingSet().empty())
        return -1;

    auto iter = playersPlayingSet().upper_bound(player);

    return iter == playersPlayingSet().cend() ? *playersPlayingSet().cbegin() : *iter;
}

void PlayerSettings::startOver(std::vector<int> playerSideMap)
{
    //TODO: size_t to int
    _playerCount = playerSideMap.size();
    _playerSideMap = std::move(playerSideMap);
    _playersFinishedMap.assign(playerCount(), false);
    _playersFinishedList.clear();
    _playersPlayingSet.clear();
    for(int player = 0; player < playerCount(); ++player)
        _playersPlayingSet.emplace(player);
}

void PlayerSettings::setPlayerFinished(int player, bool value)
{
    _playersFinishedMap.at(player) = value;

    if(value)
    {
        if(_playersPlayingSet.find(player) != _playersPlayingSet.cend())
        {
            _playersFinishedList.emplace_back(player);
            _playersPlayingSet.erase(player);
        }
    }
    else
    {
        auto finishedListIt = std::find(_playersFinishedList.cbegin(),
                                        _playersFinishedList.cend(),
                                        player);

        if(finishedListIt != _playersFinishedList.cend())
        {
            _playersFinishedList.erase(finishedListIt);
            _playersPlayingSet.emplace(player);
        }
    }
}

}
