/*
 * BattleFocusModel.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../StdInc.h"

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

void BattleFocusModel::clearFocus()
{
	focusedHex = BattleHex::INVALID;
}

bool BattleFocusModel::moveFocus(BattleHex::EDir direction)
{
	if(!hasFocus())
		return false;

	try
	{
		focusedHex = focusedHex.cloneInDirection(direction, true);
		return true;
	}
	catch(const std::out_of_range &)
	{
		// board border: focus stays where it was
		return false;
	}
}

bool BattleFocusModel::moveFocusPad(PadDirection pad)
{
	if(pad == PAD_NONE)
		return false;

	return moveFocus(padDirectionToHex(pad));
}

BattleHex::EDir BattleFocusModel::padDirectionToHex(PadDirection pad) const
{
	if(!hasFocus())
		return BattleHex::NONE;

	switch(pad)
	{
		case PAD_LEFT:
			return BattleHex::LEFT;
		case PAD_RIGHT:
			return BattleHex::RIGHT;
		case PAD_UP:
			// odd rows: the upward diagonal that keeps the column is the right-hand one
			return focusedHex.getY() % 2 == 1 ? BattleHex::TOP_RIGHT : BattleHex::TOP_LEFT;
		case PAD_DOWN:
			return focusedHex.getY() % 2 == 1 ? BattleHex::BOTTOM_RIGHT : BattleHex::BOTTOM_LEFT;
		default:
			return BattleHex::NONE;
	}
}
