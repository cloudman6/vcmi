/*
 * BattleFocusModel.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "BattleFocusModel.h"

bool BattleFocusModel::hasFocus() const
{
	return focusedHex.isValid();
}

BattleHex BattleFocusModel::getFocusedHex() const
{
	return focusedHex;
}

bool BattleFocusModel::setFocus(const BattleHex & hex)
{
	if(!hex.isValid())
		return false;

	focusedHex = hex;
	return true;
}

bool BattleFocusModel::moveFocus(BattleHex::EDir direction)
{
	if(!hasFocus() || direction == BattleHex::NONE)
		return false;

	try
	{
		focusedHex = focusedHex.cloneInDirection(direction, true);
		return true;
	}
	catch(const std::out_of_range &)
	{
		return false;
	}
}
