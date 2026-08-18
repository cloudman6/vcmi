/*
 * BattleControllerInteractionStateTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "../client/StdInc.h"

#include "../client/battle/BattleControllerInteractionState.h"
#include "../client/eventsSDL/InputHandler.h"

#include <gtest/gtest.h>

TEST(BattleControllerInteractionStateTest, CursorModeRestoresSavedFocusBeforeActiveStack)
{
	BattleControllerInteractionState state;
	state.enterCursorMode(BattleHex(42));

	EXPECT_TRUE(state.isCursorMode());
	EXPECT_EQ(state.leaveCursorMode(BattleHex(19)), BattleHex(42));
	EXPECT_FALSE(state.isCursorMode());
}

TEST(BattleControllerInteractionStateTest, CursorModeFallsBackToActiveStackWithoutSavedFocus)
{
	BattleControllerInteractionState state;
	state.enterCursorMode(BattleHex::INVALID);

	EXPECT_EQ(state.leaveCursorMode(BattleHex(19)), BattleHex(19));
}

TEST(BattleControllerInteractionStateTest, SyntheticRefreshCannotClaimNativePresentation)
{
	BattleControllerInteractionState state;
	state.controllerInputActivated();

	EXPECT_FALSE(state.acceptsPointerPresentation(PointerEventSource::SYNTHETIC_REFRESH));
	EXPECT_FALSE(state.acceptsPointerPresentation(PointerEventSource::CONTROLLER_CURSOR));
	EXPECT_TRUE(state.acceptsPointerPresentation(PointerEventSource::REAL_MOUSE));
	EXPECT_TRUE(state.acceptsPointerPresentation(PointerEventSource::SYNTHETIC_REFRESH));
}

TEST(BattleControllerInteractionStateTest, ControllerCursorOnlyOwnsPresentationInCursorMode)
{
	BattleControllerInteractionState state;
	state.controllerInputActivated();
	state.enterCursorMode(BattleHex(42));

	EXPECT_TRUE(state.acceptsPointerPresentation(PointerEventSource::CONTROLLER_CURSOR));
	state.leaveCursorMode(BattleHex(19));
	EXPECT_FALSE(state.acceptsPointerPresentation(PointerEventSource::CONTROLLER_CURSOR));
}
