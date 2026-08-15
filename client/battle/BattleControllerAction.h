/*
 * BattleControllerAction.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#pragma once

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
