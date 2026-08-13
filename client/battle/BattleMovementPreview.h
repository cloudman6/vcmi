/*
 * BattleMovementPreview.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "BattleControllerStateMachine.h"

/// Pure A-button contract for the controller movement preview. The
/// caller supplies the current interaction layer and whether the focused hex
/// is a legal movement destination for the active stack; the rule decides
/// the outcome without touching battle state.
class BattleMovementPreview
{
public:
	enum class Outcome
	{
		NONE,           ///< A has no preview/commit meaning in this layer
		START_PREVIEW,  ///< legacy preview-layer outcome; not used from browse
		COMMIT,         ///< submit the move to the focused hex
		CANCEL_PREVIEW  ///< focus stopped being reachable; back to browse
	};

	static Outcome decideAccept(BattleControllerStateMachine::State top, bool focusedReachable);
};
