/*
 * BattleFocusRestore.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../StdInc.h"

#include "BattleFocusRestore.h"

BattleHex BattleFocusRestore::decide(InputMode inputMode, const BattleHex & activeStackHead)
{
	// mouse zero-regression: pointer modes never move the controller focus
	if(inputMode != InputMode::CONTROLLER)
		return BattleHex::INVALID;

	// no active stack: the last valid focus and its status host hint stay put
	if(!activeStackHead.isValid())
		return BattleHex::INVALID;

	// entry default and restore both land on the active stack head hex
	return activeStackHead;
}
