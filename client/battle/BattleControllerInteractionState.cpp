/*
 * BattleControllerInteractionState.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "BattleControllerInteractionState.h"

#include "../eventsSDL/InputHandler.h"

bool BattleControllerInteractionState::isCursorMode() const
{
	return cursorMode;
}

void BattleControllerInteractionState::enterCursorMode(const BattleHex & focusedHex)
{
	if(cursorMode)
		return;
	cursorRestoreHex = focusedHex;
	cursorMode = true;
	pointerPresentationOwner = true;
}

BattleHex BattleControllerInteractionState::leaveCursorMode(const BattleHex & activeStackHex)
{
	if(!cursorMode)
		return BattleHex::INVALID;

	cursorMode = false;
	pointerPresentationOwner = false;
	const BattleHex restoreHex = cursorRestoreHex.isValid() ? cursorRestoreHex : activeStackHex;
	cursorRestoreHex = BattleHex::INVALID;
	return restoreHex;
}

void BattleControllerInteractionState::controllerInputActivated()
{
	if(!cursorMode)
		pointerPresentationOwner = false;
}

bool BattleControllerInteractionState::acceptsPointerPresentation(PointerEventSource source)
{
	if(source == PointerEventSource::REAL_MOUSE || source == PointerEventSource::TOUCH)
	{
		pointerPresentationOwner = true;
		return true;
	}
	if(source == PointerEventSource::CONTROLLER_CURSOR)
		return cursorMode;
	return pointerPresentationOwner;
}
