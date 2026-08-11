/*
 * BattleAttackDirection.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../StdInc.h"

#include "BattleAttackDirection.h"

BattleHex BattleAttackDirection::recommend(const std::vector<BattleHex> & candidates)
{
	return candidates.empty() ? BattleHex::INVALID : candidates.front();
}

BattleHex BattleAttackDirection::cycle(const std::vector<BattleHex> & candidates, const BattleHex & current, bool forward)
{
	if(candidates.empty())
		return BattleHex::INVALID;

	size_t currentIndex = 0;
	bool found = false;
	for(size_t index = 0; index < candidates.size(); ++index)
	{
		if(candidates[index] == current)
		{
			currentIndex = index;
			found = true;
			break;
		}
	}

	if(!found)
		return forward ? candidates.front() : candidates.back();

	return candidates[(currentIndex + (forward ? 1 : candidates.size() - 1)) % candidates.size()];
}

BattleAttackDirection::MeleeOutcome BattleAttackDirection::decideAccept(BattleControllerStateMachine::State top, bool attackable)
{
	using State = BattleControllerStateMachine::State;

	switch(top)
	{
		case State::BROWSE:
			return attackable ? MeleeOutcome::START_ACTION : MeleeOutcome::NONE;
		case State::ACTION:
			if(!attackable)
				return MeleeOutcome::CANCEL_LAYER; // BT-07 target lost
			return MeleeOutcome::OPEN_DIRECTION;
		case State::ATTACK_DIRECTION:
			if(!attackable)
				return MeleeOutcome::CANCEL_LAYER; // BT-07 target lost
			return MeleeOutcome::COMMIT;
		default:
			return MeleeOutcome::NONE;
	}
}
