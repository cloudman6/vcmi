/*
 * BattleMovementPreview.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../StdInc.h"

#include "BattleMovementPreview.h"

BattleMovementPreview::Outcome BattleMovementPreview::decideAccept(BattleControllerStateMachine::State top, bool focusedReachable)
{
	using State = BattleControllerStateMachine::State;

	switch(top)
	{
		case State::BROWSE:
			return focusedReachable ? Outcome::COMMIT : Outcome::NONE;
		case State::PREVIEW:
			// invalidation refresh: a destination that stopped being legal
			// backs out of the preview instead of committing
			return focusedReachable ? Outcome::COMMIT : Outcome::CANCEL_PREVIEW;
		default:
			return Outcome::NONE;
	}
}
