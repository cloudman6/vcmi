/*
 * BattleFocusRestoreTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../client/StdInc.h"

#include "../client/battle/BattleFocusRestore.h"

#include "../lib/GameConstants.h"

#include <gtest/gtest.h>

// D8: on battle entry the controller focus defaults to the active stack hex;
// for wide units the caller passes the head hex resolved from getPosition()
TEST(BattleFocusRestoreTest, EntryDefaultsToTheActiveStackHeadHex)
{
	EXPECT_EQ(BattleFocusRestore::decide(InputMode::CONTROLLER, BattleHex(10, 3)), BattleHex(10, 3));
}

// D8: after target death, authority rejection or a netpack refresh the focus
// restores back to the active stack hex, no matter where it moved to while
// choosing the lost target
TEST(BattleFocusRestoreTest, RestoresToTheActiveStackHeadHexFromAnyFocusedTarget)
{
	// the focus stood on an enemy hex when the target was lost
	EXPECT_EQ(BattleFocusRestore::decide(InputMode::CONTROLLER, BattleHex(10, 3)), BattleHex(10, 3));
}

// D8: without an active stack the last valid focus stays put; the existing
// status host keeps announcing it as the hint
TEST(BattleFocusRestoreTest, NoActiveStackKeepsTheLastValidFocus)
{
	EXPECT_EQ(BattleFocusRestore::decide(InputMode::CONTROLLER, BattleHex::INVALID), BattleHex::INVALID);
}

// mouse zero-regression: pointer modes never move the controller focus,
// neither on battle entry nor after an active stack change
TEST(BattleFocusRestoreTest, PointerModesNeverMoveTheFocus)
{
	EXPECT_EQ(BattleFocusRestore::decide(InputMode::KEYBOARD_AND_MOUSE, BattleHex(10, 3)), BattleHex::INVALID);
	EXPECT_EQ(BattleFocusRestore::decide(InputMode::TOUCH, BattleHex(10, 3)), BattleHex::INVALID);
}

TEST(BattleFocusRestoreTest, CursorModeSuspendsActiveStackDrivenFocusChanges)
{
	EXPECT_EQ(
		BattleFocusRestore::decide(InputMode::CONTROLLER, BattleHex(10, 3), true),
		BattleHex::INVALID);
}

TEST(BattleFocusRestoreTest, CursorModeExitPrefersTheSavedFocusThenFallsBackToActiveStack)
{
	EXPECT_EQ(
		BattleFocusRestore::afterCursorMode(BattleHex(7, 5), BattleHex(10, 3)),
		BattleHex(7, 5));
	EXPECT_EQ(
		BattleFocusRestore::afterCursorMode(BattleHex::INVALID, BattleHex(10, 3)),
		BattleHex(10, 3));
	EXPECT_EQ(
		BattleFocusRestore::afterCursorMode(BattleHex::INVALID, BattleHex::INVALID),
		BattleHex::INVALID);
}
