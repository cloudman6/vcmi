/*
 * BattleHintBar.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "BattleControllerStateMachine.h"
#include "BattleRangedShooting.h"

#include "../eventsSDL/InputHandler.h"
#include "../gui/Shortcut.h"

#include <string>
#include <vector>

/// One glyph+text prompt of the battle controller hint bar (D6).
struct BattleHintEntry
{
	EShortcut glyph;      ///< shortcut whose controller glyph is rendered
	std::string textKey;  ///< translation key of the prompt text
};

/// Pure content contract of the contextual battle hint bar. The caller
/// supplies the interaction layer and the focus context; the rule returns
/// the prompt list without touching rendering, input or battle state.
class BattleHintBar
{
public:
	struct Context
	{
		bool attackable = false;
		bool shootable = false;
		bool focusedReachable = false;
		BattleRangedShooting::DisabledReason shootingDisabled = BattleRangedShooting::DisabledReason::NONE;
	};

	/// Prompt list for the given layer; empty when the bar must be hidden
	/// (pointer modes, spell targeting, commit animation).
	static std::vector<BattleHintEntry> entries(InputMode inputMode, BattleControllerStateMachine::State top, const Context & context);
};
