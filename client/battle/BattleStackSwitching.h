/*
 * BattleStackSwitching.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/battle/BattleHex.h"

#include <vector>

/// One own stack that LB/RB may move the controller focus to, described by
/// its head hex (D8: wide units anchor on the head hex) and optional tail hex.
struct BattleStackSwitchEntry
{
	int32_t unitId = -1;
	BattleHex headHex = BattleHex::INVALID;
	BattleHex tailHex = BattleHex::INVALID;
};

/// Pure selection rule for LB/RB stack switching (D7). No battle access,
/// no input mode checks - the caller gates when switching is allowed.
class BattleStackSwitching
{
public:
	/// Cycles through candidates in turn order (forward) or reverse order.
	/// Focus resting on either hex of a wide unit identifies that entry;
	/// focus outside the candidate set starts at the first entry when
	/// switching forward and at the last one when switching backward.
	/// Returns an invalid entry for an empty candidate list.
	static BattleStackSwitchEntry select(const std::vector<BattleStackSwitchEntry> & candidates, const BattleHex & focusedHex, bool forward);
};
