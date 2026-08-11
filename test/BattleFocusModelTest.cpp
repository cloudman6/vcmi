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

TEST(BattleFocusModelTest, startsWithoutFocus)
{
	BattleFocusModel model;
	EXPECT_FALSE(model.hasFocus());
	EXPECT_EQ(model.getFocusedHex(), BattleHex::INVALID);
}

TEST(BattleFocusModelTest, setFocusAcceptsBoardHexesOnly)
{
	BattleFocusModel model;

	EXPECT_TRUE(model.setFocus(BattleHex(0, 0)));
	EXPECT_TRUE(model.hasFocus());
	EXPECT_EQ(model.getFocusedHex(), BattleHex(0, 0));

	EXPECT_TRUE(model.setFocus(BattleHex(GameConstants::BFIELD_WIDTH - 1, GameConstants::BFIELD_HEIGHT - 1)));

	EXPECT_FALSE(model.setFocus(BattleHex(-1)));
	EXPECT_FALSE(model.setFocus(BattleHex(GameConstants::BFIELD_SIZE)));
	EXPECT_TRUE(model.hasFocus()); // failed set keeps previous focus
}

TEST(BattleFocusModelTest, clearFocusDropsState)
{
	BattleFocusModel model;
	model.setFocus(BattleHex(8, 5));
	model.clearFocus();
	EXPECT_FALSE(model.hasFocus());
	EXPECT_EQ(model.getFocusedHex(), BattleHex::INVALID);
}

TEST(BattleFocusModelTest, moveWithoutFocusIsRejected)
{
	BattleFocusModel model;
	EXPECT_FALSE(model.moveFocus(BattleHex::RIGHT));
	EXPECT_FALSE(model.hasFocus());
}

TEST(BattleFocusModelTest, horizontalMoveChangesOnlyX)
{
	BattleFocusModel model;
	model.setFocus(BattleHex(8, 5));

	const BattleHex start = model.getFocusedHex();
	ASSERT_TRUE(model.moveFocus(BattleHex::RIGHT));
	EXPECT_EQ(model.getFocusedHex().getY(), start.getY());
	EXPECT_EQ(model.getFocusedHex().getX(), start.getX() + 1);

	ASSERT_TRUE(model.moveFocus(BattleHex::LEFT));
	EXPECT_EQ(model.getFocusedHex(), start);
}

TEST(BattleFocusModelTest, verticalMoveChangesYByOneRowAndAtMostOneColumn)
{
	BattleFocusModel model;
	model.setFocus(BattleHex(8, 5));

	const BattleHex start = model.getFocusedHex();
	ASSERT_TRUE(model.moveFocus(BattleHex::TOP_LEFT));
	EXPECT_EQ(model.getFocusedHex().getY(), start.getY() - 1);
	EXPECT_LE(std::abs(model.getFocusedHex().getX() - start.getX()), 1);

	// odd row: TOP_LEFT decrements x; the inverse from the even row above is BOTTOM_RIGHT
	ASSERT_TRUE(model.moveFocus(BattleHex::BOTTOM_RIGHT));
	EXPECT_EQ(model.getFocusedHex(), start);
}

TEST(BattleFocusModelTest, moveAtBorderKeepsFocusAndFails)
{
	BattleFocusModel model;

	model.setFocus(BattleHex(0, 0));
	EXPECT_FALSE(model.moveFocus(BattleHex::LEFT));
	EXPECT_FALSE(model.moveFocus(BattleHex::TOP_LEFT));
	EXPECT_FALSE(model.moveFocus(BattleHex::TOP_RIGHT));
	EXPECT_EQ(model.getFocusedHex(), BattleHex(0, 0));

	model.setFocus(BattleHex(GameConstants::BFIELD_WIDTH - 1, 0));
	EXPECT_FALSE(model.moveFocus(BattleHex::RIGHT));
	EXPECT_FALSE(model.moveFocus(BattleHex::TOP_RIGHT));

	model.setFocus(BattleHex(0, GameConstants::BFIELD_HEIGHT - 1));
	EXPECT_FALSE(model.moveFocus(BattleHex::BOTTOM_LEFT));

	model.setFocus(BattleHex(GameConstants::BFIELD_WIDTH - 1, GameConstants::BFIELD_HEIGHT - 1));
	EXPECT_FALSE(model.moveFocus(BattleHex::BOTTOM_RIGHT));
	EXPECT_FALSE(model.moveFocus(BattleHex::RIGHT));
}

TEST(BattleFocusModelTest, padDirectionsResolveToHexDirections)
{
	BattleFocusModel model;
	model.setFocus(BattleHex(8, 5));

	EXPECT_EQ(model.padDirectionToHex(BattleFocusModel::PAD_LEFT), BattleHex::LEFT);
	EXPECT_EQ(model.padDirectionToHex(BattleFocusModel::PAD_RIGHT), BattleHex::RIGHT);

	const auto up = model.padDirectionToHex(BattleFocusModel::PAD_UP);
	EXPECT_TRUE(up == BattleHex::TOP_LEFT || up == BattleHex::TOP_RIGHT);
	const auto down = model.padDirectionToHex(BattleFocusModel::PAD_DOWN);
	EXPECT_TRUE(down == BattleHex::BOTTOM_LEFT || down == BattleHex::BOTTOM_RIGHT);

	EXPECT_EQ(model.padDirectionToHex(BattleFocusModel::PAD_NONE), BattleHex::NONE);
}

TEST(BattleFocusModelTest, padVerticalMappingKeepsColumnAndDependsOnRowParity)
{
	BattleFocusModel model;

	// odd row: the vertical direction that preserves x is the right-hand one
	model.setFocus(BattleHex(8, 5));
	EXPECT_EQ(model.padDirectionToHex(BattleFocusModel::PAD_UP), BattleHex::TOP_RIGHT);
	EXPECT_EQ(model.padDirectionToHex(BattleFocusModel::PAD_DOWN), BattleHex::BOTTOM_RIGHT);

	// even row: the vertical direction that preserves x is the left-hand one
	model.setFocus(BattleHex(8, 4));
	EXPECT_EQ(model.padDirectionToHex(BattleFocusModel::PAD_UP), BattleHex::TOP_LEFT);
	EXPECT_EQ(model.padDirectionToHex(BattleFocusModel::PAD_DOWN), BattleHex::BOTTOM_LEFT);
}

TEST(BattleFocusModelTest, padMoveAppliesMappedDirection)
{
	BattleFocusModel model;
	model.setFocus(BattleHex(8, 5));
	const BattleHex start = model.getFocusedHex();

	ASSERT_TRUE(model.moveFocusPad(BattleFocusModel::PAD_UP));
	EXPECT_EQ(model.getFocusedHex().getY(), start.getY() - 1);
	EXPECT_EQ(model.getFocusedHex().getX(), start.getX()); // column-preserving policy

	ASSERT_TRUE(model.moveFocusPad(BattleFocusModel::PAD_DOWN));
	EXPECT_EQ(model.getFocusedHex(), start);

	EXPECT_FALSE(model.moveFocusPad(BattleFocusModel::PAD_NONE));
}
