/*
 * BattleRangedShooting.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../StdInc.h"

#include "BattleRangedShooting.h"

BattleRangedShooting::DisabledReason BattleRangedShooting::classify(bool enemyTarget, bool hasAmmo, bool blockedByNeighbor, bool withinRange)
{
	if(!enemyTarget)
		return DisabledReason::NONE;

	if(!hasAmmo)
		return DisabledReason::NO_AMMO;

	if(blockedByNeighbor)
		return DisabledReason::BLOCKED_LINE;

	if(!withinRange)
		return DisabledReason::OUT_OF_RANGE;

	return DisabledReason::NONE;
}

BattleRangedShooting::Outcome BattleRangedShooting::decideAccept(BattleControllerStateMachine::State top, bool shootable)
{
	using State = BattleControllerStateMachine::State;

	switch(top)
	{
		case State::BROWSE:
			return shootable ? Outcome::COMMIT : Outcome::NONE;
		case State::ACTION:
			if(!shootable)
				return Outcome::CANCEL_LAYER; // BT-07 target lost or disabled
			return Outcome::COMMIT;
		default:
			return Outcome::NONE;
	}
}
