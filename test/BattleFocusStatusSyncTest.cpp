/*
 * BattleFocusStatusSyncTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../client/StdInc.h"

#include "../client/battle/BattleFocusStatusSync.h"

#include "../lib/GameConstants.h"

#include <gtest/gtest.h>

TEST(BattleFocusStatusSyncTest, ControllerFocusOwnsTheStatusHost)
{
	BattleFocusModel model;
	ASSERT_TRUE(model.setFocus(BattleHex(8, 5)));

	EXPECT_EQ(BattleFocusStatusSync::decide(InputMode::CONTROLLER, model), BattleHex(8, 5));

	// the host must follow the focus as it moves, so the damage and
	// retaliation preview stays on the focused target
	ASSERT_TRUE(model.moveFocus(BattleHex::RIGHT));
	EXPECT_EQ(BattleFocusStatusSync::decide(InputMode::CONTROLLER, model), BattleHex(9, 5));
}

TEST(BattleFocusStatusSyncTest, PointerModesLeaveTheStatusBarToThePointer)
{
	BattleFocusModel model;
	ASSERT_TRUE(model.setFocus(BattleHex(8, 5)));

	EXPECT_EQ(BattleFocusStatusSync::decide(InputMode::KEYBOARD_AND_MOUSE, model), BattleHex::INVALID);
	EXPECT_EQ(BattleFocusStatusSync::decide(InputMode::TOUCH, model), BattleHex::INVALID);
}

TEST(BattleFocusStatusSyncTest, ControllerWithoutFocusDoesNotTouchTheStatusBar)
{
	BattleFocusModel model;

	EXPECT_EQ(BattleFocusStatusSync::decide(InputMode::CONTROLLER, model), BattleHex::INVALID);
}
