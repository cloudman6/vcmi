/*
 * BattleFocusModel.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/battle/BattleHex.h"

/// Persistent controller focus position on the battle field. Pure state model:
/// no rendering, no input and no battle legality - consumers validate actions
/// through BattleActionsController. The model never duplicates battle rules.
class BattleFocusModel
{
public:
	/// Four-way physical input from D-pad or analog stick quantization.
	enum PadDirection
	{
		PAD_NONE = -1,
		PAD_UP,
		PAD_DOWN,
		PAD_LEFT,
		PAD_RIGHT
	};

	bool hasFocus() const;
	BattleHex getFocusedHex() const;

	/// Accepts only valid board hexes; failed calls keep the previous focus.
	bool setFocus(const BattleHex & hex);
	void clearFocus();

	/// Moves focus one hex in the given direction. Fails without moving at
	/// board borders or without focus.
	bool moveFocus(BattleHex::EDir direction);

	/// Maps a four-way pad input onto the six hex directions and moves.
	bool moveFocusPad(PadDirection pad);

	/// Deterministic four-way to six-way mapping. Vertical inputs pick the
	/// diagonal that preserves the current column, which depends on row parity.
	BattleHex::EDir padDirectionToHex(PadDirection pad) const;

private:
	BattleHex focusedHex = BattleHex::INVALID;
};
