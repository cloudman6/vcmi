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
#include "../client/eventsSDL/InputSourceGameController.h"

#include "../lib/GameConstants.h"

#include <gtest/gtest.h>

TEST(BattleFocusNavigationTest, DpadShortcutsAreNotBattleNavigation)
{
	for(const auto shortcut : {EShortcut::MOVE_UP, EShortcut::MOVE_DOWN, EShortcut::MOVE_LEFT, EShortcut::MOVE_RIGHT})
		EXPECT_FALSE(BattleFocusNavigation::isNavigationShortcut(shortcut));
}

TEST(BattleFocusNavigationTest, ControllerModeDpadLeavesFocusUntouched)
{
	BattleFocusModel model;
	model.setFocus(BattleHex(8, 5));
	BattleFocusNavigation navigation(model);
	const BattleHex start = model.getFocusedHex();

	for(const auto shortcut : {EShortcut::MOVE_UP, EShortcut::MOVE_DOWN, EShortcut::MOVE_LEFT, EShortcut::MOVE_RIGHT})
		EXPECT_FALSE(navigation.handleShortcut(shortcut, InputMode::CONTROLLER));
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

TEST(BattleFocusNavigationTest, DpadIsNeverConsumedWithoutFocus)
{
	BattleFocusModel model;
	BattleFocusNavigation navigation(model);

	EXPECT_FALSE(navigation.handleShortcut(EShortcut::MOVE_RIGHT, InputMode::CONTROLLER));
	EXPECT_FALSE(model.hasFocus());

	model.setFocus(BattleHex(0, 0));
	EXPECT_FALSE(navigation.handleShortcut(EShortcut::MOVE_LEFT, InputMode::CONTROLLER));
	EXPECT_EQ(model.getFocusedHex(), BattleHex(0, 0));
}

TEST(BattleFocusNavigationTest, LeftStickQuantizesAllSixHexDirections)
{
	struct Sample { double x; double y; BattleHex::EDir direction; };
	const std::array<Sample, 6> samples = {{{1, 0, BattleHex::RIGHT}, {0.5, 0.9, BattleHex::BOTTOM_RIGHT},
		{-0.5, 0.9, BattleHex::BOTTOM_LEFT}, {-1, 0, BattleHex::LEFT},
		{-0.5, -0.9, BattleHex::TOP_LEFT}, {0.5, -0.9, BattleHex::TOP_RIGHT}}};

	for(const auto & sample : samples)
	{
		BattleFocusModel model;
		model.setFocus(BattleHex(8, 5));
		BattleFocusNavigation navigation(model);
		const BattleHex expected = model.getFocusedHex().cloneInDirection(sample.direction, true);
		navigation.updateAxis(7, true, sample.x);
		navigation.updateAxis(7, false, sample.y);
		EXPECT_FALSE(navigation.update(15));
		EXPECT_TRUE(navigation.update(1));
		EXPECT_EQ(model.getFocusedHex(), expected);
	}
}

TEST(BattleFocusNavigationTest, DeadZoneReleaseAndDeviceChangeResetSampling)
{
	BattleFocusModel model;
	model.setFocus(BattleHex(8, 5));
	BattleFocusNavigation navigation(model);
	const BattleHex start = model.getFocusedHex();

	navigation.updateAxis(1, true, 0.3);
	EXPECT_FALSE(navigation.update(500));
	EXPECT_EQ(model.getFocusedHex(), start);

	navigation.updateAxis(1, true, 1.0);
	navigation.updateAxis(2, false, 1.0);
	EXPECT_TRUE(navigation.update(500));
	EXPECT_EQ(model.getFocusedHex(), start.cloneInDirection(BattleHex::BOTTOM_LEFT, true));
}

TEST(BattleFocusNavigationTest, AxisNormalizationIsSymmetric)
{
	const double positive = InputSourceGameController::normalizeAxisValue(16384, 0.2, 1.0);
	const double negative = InputSourceGameController::normalizeAxisValue(-16384, 0.2, 1.0);
	EXPECT_NEAR(positive, -negative, 0.0001);
	EXPECT_GT(positive, 0.0);
	EXPECT_EQ(InputSourceGameController::normalizeAxisValue(3000, 0.2, 1.0), 0.0);
}
