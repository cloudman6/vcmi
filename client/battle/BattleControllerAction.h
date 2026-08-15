/*
 * BattleControllerAction.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#pragma once

#include "../../lib/battle/BattleHex.h"
#include "../../lib/battle/PossiblePlayerBattleAction.h"

enum class BattleControllerPrimaryAction
{
	NONE,
	MOVE,
	ATTACK,
	SHOOT,
	INSPECT
};

BattleControllerPrimaryAction classifyBattleControllerPrimaryAction(
	PossiblePlayerBattleAction::Actions action, bool legal);

/// Tracks a controller primary action from button press to matching release.
class BattleControllerActionPressState
{
	BattleControllerPrimaryAction pressedAction = BattleControllerPrimaryAction::NONE;
	BattleHex pressedHex = BattleHex::INVALID;

public:
	bool press(BattleControllerPrimaryAction action, const BattleHex & focusedHex);
	BattleControllerPrimaryAction release(BattleControllerPrimaryAction action, const BattleHex & focusedHex);
	bool hasPendingAction() const;
	bool isPressed(BattleControllerPrimaryAction action, const BattleHex & focusedHex) const;
	void reset();
};
