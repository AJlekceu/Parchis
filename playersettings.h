#ifndef PLAYERSETTINGS_H
#define PLAYERSETTINGS_H

#include <vector>
#include <set>

namespace parchis
{

class PlayerSettings
{
public:
    int playerCount() const { return _playerCount; }
    const std::vector<int> & playerSideMap() const { return _playerSideMap; }
    const std::vector<bool> & playersFinishedMap() const { return _playersFinishedMap; }
    const std::vector<int> & playersFinishedList() const { return _playersFinishedList; }
    const std::set<int> & playersPlayingSet() const { return _playersPlayingSet; }
    int firstPlayerPlaying() const { return nextPlayerPlaying(-1); }
    int nextPlayerPlaying(int player) const;

    void reset() { startOver(std::vector<int>{}); }
    void startOver(std::vector<int> playerSideMap);
    void setPlayerFinished(int player, bool value);

private:
    int _playerCount = 0;
    std::vector<int> _playerSideMap;
    std::vector<bool> _playersFinishedMap;
    std::vector<int> _playersFinishedList;
    std::set<int> _playersPlayingSet;
};

}
#endif // PLAYERSETTINGS_H
