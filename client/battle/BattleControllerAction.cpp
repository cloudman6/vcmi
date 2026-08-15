/*
 * BattleControllerAction.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "../StdInc.h"

#include "BattleControllerAction.h"

BattleControllerPrimaryAction classifyBattleControllerPrimaryAction(
	PossiblePlayerBattleAction::Actions action, bool legal)
{
	if(!legal)
		return BattleControllerPrimaryAction::NONE;

	switch(action)
	{
		case PossiblePlayerBattleAction::MOVE_TACTICS:
		case PossiblePlayerBattleAction::MOVE_STACK:
			return BattleControllerPrimaryAction::MOVE;
		case PossiblePlayerBattleAction::ATTACK:
		case PossiblePlayerBattleAction::LONG_WEAPON_ATTACK:
		case PossiblePlayerBattleAction::WALK_AND_ATTACK:
		case PossiblePlayerBattleAction::ATTACK_AND_RETURN:
			return BattleControllerPrimaryAction::ATTACK;
		case PossiblePlayerBattleAction::SHOOT:
			return BattleControllerPrimaryAction::SHOOT;
		case PossiblePlayerBattleAction::CREATURE_INFO:
			return BattleControllerPrimaryAction::INSPECT;
		default:
			return BattleControllerPrimaryAction::NONE;
	}
}
