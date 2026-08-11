/*
 * BattleStackSwitching.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../StdInc.h"

#include "BattleStackSwitching.h"

BattleStackSwitchEntry BattleStackSwitching::select(const std::vector<BattleStackSwitchEntry> & candidates, const BattleHex & focusedHex, bool forward)
{
	if(candidates.empty())
		return {};

	size_t current = 0;
	bool found = false;
	for(size_t index = 0; index < candidates.size(); ++index)
	{
		if(candidates[index].headHex == focusedHex || candidates[index].tailHex == focusedHex)
		{
			current = index;
			found = true;
			break;
		}
	}

	if(!found)
		return forward ? candidates.front() : candidates.back();

	// move one step in the chosen direction, wrapping at both ends
	size_t next = forward
		? (current + 1) % candidates.size()
		: (current + candidates.size() - 1) % candidates.size();

	return candidates[next];
}
