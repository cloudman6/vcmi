/*
 * BattleFocusNavigationTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../client/StdInc.h"

#include "../client/battle/BattleFocusNavigation.h"

#include "../lib/GameConstants.h"

#include <gtest/gtest.h>

TEST(BattleFocusNavigationTest, NavigationShortcutsAreRecognized)
{
	EXPECT_TRUE(BattleFocusNavigation::isNavigationShortcut(EShortcut::MOVE_UP));
	EXPECT_TRUE(BattleFocusNavigation::isNavigationShortcut(EShortcut::MOVE_DOWN));
	EXPECT_TRUE(BattleFocusNavigation::isNavigationShortcut(EShortcut::MOVE_LEFT));
	EXPECT_TRUE(BattleFocusNavigation::isNavigationShortcut(EShortcut::MOVE_RIGHT));

	EXPECT_FALSE(BattleFocusNavigation::isNavigationShortcut(EShortcut::GLOBAL_ACCEPT));
	EXPECT_FALSE(BattleFocusNavigation::isNavigationShortcut(EShortcut::GLOBAL_CANCEL));
	EXPECT_FALSE(BattleFocusNavigation::isNavigationShortcut(EShortcut::BATTLE_CONSOLE_UP));
	EXPECT_FALSE(BattleFocusNavigation::isNavigationShortcut(EShortcut::MOVE_PAGE_DOWN));
}

TEST(BattleFocusNavigationTest, ControllerModeNavigationMovesFocus)
{
	BattleFocusModel model;
	model.setFocus(BattleHex(8, 5));
	BattleFocusNavigation navigation(model);
	const BattleHex start = model.getFocusedHex();

	ASSERT_TRUE(navigation.handleShortcut(EShortcut::MOVE_RIGHT, InputMode::CONTROLLER));
	EXPECT_EQ(model.getFocusedHex(), BattleHex(start.getX() + 1, start.getY()));

	ASSERT_TRUE(navigation.handleShortcut(EShortcut::MOVE_LEFT, InputMode::CONTROLLER));
	EXPECT_EQ(model.getFocusedHex(), start);

	// odd row: MOVE_UP maps onto the column-preserving TOP_RIGHT diagonal
	ASSERT_TRUE(navigation.handleShortcut(EShortcut::MOVE_UP, InputMode::CONTROLLER));
	EXPECT_EQ(model.getFocusedHex().getY(), start.getY() - 1);
	EXPECT_EQ(model.getFocusedHex().getX(), start.getX());

	ASSERT_TRUE(navigation.handleShortcut(EShortcut::MOVE_DOWN, InputMode::CONTROLLER));
	EXPECT_EQ(model.getFocusedHex(), start);
}

TEST(BattleFocusNavigationTest, MouseModeLeavesFocusUntouched)
{
	BattleFocusModel model;
	model.setFocus(BattleHex(8, 5));
	BattleFocusNavigation navigation(model);
	const BattleHex start = model.getFocusedHex();

	EXPECT_FALSE(navigation.handleShortcut(EShortcut::MOVE_UP, InputMode::KEYBOARD_AND_MOUSE));
	EXPECT_FALSE(navigation.handleShortcut(EShortcut::MOVE_DOWN, InputMode::KEYBOARD_AND_MOUSE));
	EXPECT_FALSE(navigation.handleShortcut(EShortcut::MOVE_LEFT, InputMode::KEYBOARD_AND_MOUSE));
	EXPECT_FALSE(navigation.handleShortcut(EShortcut::MOVE_RIGHT, InputMode::KEYBOARD_AND_MOUSE));
	EXPECT_EQ(model.getFocusedHex(), start);

	EXPECT_FALSE(navigation.handleShortcut(EShortcut::MOVE_UP, InputMode::TOUCH));
	EXPECT_EQ(model.getFocusedHex(), start);
}

TEST(BattleFocusNavigationTest, UnrelatedShortcutsAreNotConsumed)
{
	BattleFocusModel model;
	model.setFocus(BattleHex(8, 5));
	BattleFocusNavigation navigation(model);
	const BattleHex start = model.getFocusedHex();

	EXPECT_FALSE(navigation.handleShortcut(EShortcut::GLOBAL_ACCEPT, InputMode::CONTROLLER));
	EXPECT_FALSE(navigation.handleShortcut(EShortcut::BATTLE_CONSOLE_UP, InputMode::CONTROLLER));
	EXPECT_FALSE(navigation.handleShortcut(EShortcut::MOVE_PAGE_DOWN, InputMode::CONTROLLER));
	EXPECT_EQ(model.getFocusedHex(), start);
}

TEST(BattleFocusNavigationTest, BorderAndFocuslessNavigationAreConsumedWithoutMoving)
{
	BattleFocusModel model;
	BattleFocusNavigation navigation(model);

	// no focus yet: navigation is consumed but cannot move anywhere
	EXPECT_TRUE(navigation.handleShortcut(EShortcut::MOVE_RIGHT, InputMode::CONTROLLER));
	EXPECT_FALSE(model.hasFocus());

	model.setFocus(BattleHex(0, 0));
	EXPECT_TRUE(navigation.handleShortcut(EShortcut::MOVE_LEFT, InputMode::CONTROLLER));
	EXPECT_EQ(model.getFocusedHex(), BattleHex(0, 0));
}
