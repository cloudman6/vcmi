/*
 * BattleFocusModelTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "../client/StdInc.h"

#include "../client/battle/BattleFocusModel.h"

#include "../lib/GameConstants.h"

#include <gtest/gtest.h>

TEST(BattleFocusModelTest, StartsWithoutFocus)
{
	BattleFocusModel model;
	EXPECT_FALSE(model.hasFocus());
	EXPECT_EQ(model.getFocusedHex(), BattleHex::INVALID);
}

TEST(BattleFocusModelTest, SetFocusKeepsOnlyValidBoardHexes)
{
	BattleFocusModel model;
	ASSERT_TRUE(model.setFocus(BattleHex(8, 5)));
	EXPECT_FALSE(model.setFocus(BattleHex(-1)));
	EXPECT_EQ(model.getFocusedHex(), BattleHex(8, 5));
}

TEST(BattleFocusModelTest, MovesOneAdjacentHexInAllSixDirections)
{
	for(const auto direction : {BattleHex::TOP_LEFT, BattleHex::TOP_RIGHT, BattleHex::RIGHT,
		BattleHex::BOTTOM_RIGHT, BattleHex::BOTTOM_LEFT, BattleHex::LEFT})
	{
		BattleFocusModel model;
		ASSERT_TRUE(model.setFocus(BattleHex(8, 5)));
		const auto expected = model.getFocusedHex().cloneInDirection(direction, true);
		EXPECT_TRUE(model.moveFocus(direction));
		EXPECT_EQ(model.getFocusedHex(), expected);
	}
}

TEST(BattleFocusModelTest, RejectsMovementWithoutFocusAndAcrossBoardBorder)
{
	BattleFocusModel model;
	EXPECT_FALSE(model.moveFocus(BattleHex::RIGHT));

	ASSERT_TRUE(model.setFocus(BattleHex(0, 0)));
	EXPECT_FALSE(model.moveFocus(BattleHex::LEFT));
	EXPECT_FALSE(model.moveFocus(BattleHex::TOP_LEFT));
	EXPECT_EQ(model.getFocusedHex(), BattleHex(0, 0));
}
