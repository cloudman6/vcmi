/*
 * BattleAttackDirection.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "BattleControllerStateMachine.h"

#include "../../lib/battle/BattleHex.h"

#include <vector>

/// Pure melee approach rules: origin recommendation, shoulder-button cycling
/// through legal attack-from hexes and the direct A-button contract.
/// Battle legality (which origins exist) comes from the caller.
class BattleAttackDirection
{
public:
	enum class MeleeOutcome
	{
		NONE,          ///< A has no melee meaning in this layer
		START_ACTION,  ///< open the melee action layer with a recommended origin
		OPEN_DIRECTION,///< open the approach fine-tuning layer
		COMMIT,        ///< submit the melee attack with the chosen origin
		CANCEL_LAYER   ///< target lost (BT-07); back out one layer
	};

	/// Auto-recommended origin: first candidate in direction scan order,
	/// matching the mouse path fallback; INVALID without candidates.
	static BattleHex recommend(const std::vector<BattleHex> & candidates);

	/// Cycles through candidates with wrap-around; an unknown current
	/// selection moves relative to the first (forward) or last (backward)
	/// candidate; INVALID without candidates.
	static BattleHex cycle(const std::vector<BattleHex> & candidates, const BattleHex & current, bool forward);

	static MeleeOutcome decideAccept(BattleControllerStateMachine::State top, bool attackable);
};
