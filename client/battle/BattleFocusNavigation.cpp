/*
 * BattleFocusNavigation.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../StdInc.h"

#include "BattleFocusNavigation.h"

BattleFocusNavigation::BattleFocusNavigation(BattleFocusModel & model)
	: model(model)
{
}

bool BattleFocusNavigation::isNavigationShortcut(EShortcut shortcut)
{
	return shortcut == EShortcut::MOVE_UP
		|| shortcut == EShortcut::MOVE_DOWN
		|| shortcut == EShortcut::MOVE_LEFT
		|| shortcut == EShortcut::MOVE_RIGHT;
}

BattleFocusModel::PadDirection BattleFocusNavigation::padDirection(EShortcut shortcut)
{
	switch(shortcut)
	{
		case EShortcut::MOVE_UP:
			return BattleFocusModel::PAD_UP;
		case EShortcut::MOVE_DOWN:
			return BattleFocusModel::PAD_DOWN;
		case EShortcut::MOVE_LEFT:
			return BattleFocusModel::PAD_LEFT;
		case EShortcut::MOVE_RIGHT:
			return BattleFocusModel::PAD_RIGHT;
		default:
			return BattleFocusModel::PAD_NONE;
	}
}

bool BattleFocusNavigation::handleShortcut(EShortcut shortcut, InputMode inputMode)
{
	if(!isNavigationShortcut(shortcut))
		return false;

	if(inputMode != InputMode::CONTROLLER)
		return false;

	model.moveFocusPad(padDirection(shortcut));
	return true;
}
