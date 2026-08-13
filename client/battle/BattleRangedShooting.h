/*
 * BattleRangedShooting.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "BattleControllerStateMachine.h"

/// D4 controller shooting contract: the BT-04 disabled reason tokens and
/// the A-button walk for the shooting layer. Pure - battle legality is
/// supplied by the caller from CBattleInfoCallback seams.
class BattleRangedShooting
{
public:
	/// BT-04 reason tokens for a focused enemy target that cannot be shot
	enum class DisabledReason
	{
		NONE,         ///< target is shootable or not a shooting concern
		NO_AMMO,      ///< shooter has no shots left
		BLOCKED_LINE, ///< enemy unit adjacent to the shooter blocks the shot
		OUT_OF_RANGE  ///< target beyond the limited shooting range
	};

	enum class Outcome
	{
		NONE,
		START_ACTION,
		COMMIT,
		CANCEL_LAYER
	};

	/// First failing rule wins: no ammo, then blocked line, then range.
	/// Non-enemy focus is never a shooting concern.
	static DisabledReason classify(bool enemyTarget, bool hasAmmo, bool blockedByNeighbor, bool withinRange);

	/// BROWSE + shootable commits the already-visible preview directly.
	/// A disabled target never exposes a confirm action.
	static Outcome decideAccept(BattleControllerStateMachine::State top, bool shootable);
};
