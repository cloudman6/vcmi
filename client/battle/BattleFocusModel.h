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

/// Controller focus position on the battlefield. This class deliberately has
/// no input, rendering or battle-legality responsibilities.
class BattleFocusModel
{
public:
	bool hasFocus() const;
	BattleHex getFocusedHex() const;

	bool setFocus(const BattleHex & hex);
	bool moveFocus(BattleHex::EDir direction);

private:
	BattleHex focusedHex = BattleHex::INVALID;
};
