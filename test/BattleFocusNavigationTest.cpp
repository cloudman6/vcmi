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

#include <gtest/gtest.h>

namespace
{
struct AxisSample
{
	double x;
	double y;
	BattleHex::EDir direction;
};
}

TEST(BattleFocusNavigationTest, QuantizesAllSixHexDirectionsAfterSettleDelay)
{
	const std::array<AxisSample, 6> samples = {{{1.0, 0.0, BattleHex::RIGHT},
		{0.5, 0.9, BattleHex::BOTTOM_RIGHT}, {-0.5, 0.9, BattleHex::BOTTOM_LEFT},
		{-1.0, 0.0, BattleHex::LEFT}, {-0.5, -0.9, BattleHex::TOP_LEFT},
		{0.5, -0.9, BattleHex::TOP_RIGHT}}};

	for(const auto & sample : samples)
	{
		BattleFocusModel model;
		ASSERT_TRUE(model.setFocus(BattleHex(8, 5)));
		const auto expected = model.getFocusedHex().cloneInDirection(sample.direction, true);
		BattleFocusNavigation navigation(model);

		navigation.updateAxis(7, BattleFocusNavigation::Axis::HORIZONTAL, sample.x);
		navigation.updateAxis(7, BattleFocusNavigation::Axis::VERTICAL, sample.y);
		EXPECT_FALSE(navigation.update(15));
		EXPECT_TRUE(navigation.update(1));
		EXPECT_EQ(model.getFocusedHex(), expected);
	}
}

TEST(BattleFocusNavigationTest, DeadZoneAndNeutralReleaseDoNotScheduleMovement)
{
	BattleFocusModel model;
	ASSERT_TRUE(model.setFocus(BattleHex(8, 5)));
	BattleFocusNavigation navigation(model);
	const auto start = model.getFocusedHex();

	navigation.updateAxis(1, BattleFocusNavigation::Axis::HORIZONTAL, 0.44);
	EXPECT_FALSE(navigation.update(1000));
	navigation.updateAxis(1, BattleFocusNavigation::Axis::HORIZONTAL, 1.0);
	navigation.updateAxis(1, BattleFocusNavigation::Axis::HORIZONTAL, 0.0);
	EXPECT_FALSE(navigation.update(1000));
	EXPECT_EQ(model.getFocusedHex(), start);
}

TEST(BattleFocusNavigationTest, VerticalUpFallsBackInwardAtRightBoardEdge)
{
	BattleFocusModel model;
	ASSERT_TRUE(model.setFocus(BattleHex(GameConstants::BFIELD_WIDTH - 1, GameConstants::BFIELD_HEIGHT - 1)));
	BattleFocusNavigation navigation(model);

	navigation.updateAxis(1, BattleFocusNavigation::Axis::VERTICAL, -1.0);

	ASSERT_TRUE(navigation.update(16));
	EXPECT_EQ(model.getFocusedHex(), BattleHex(GameConstants::BFIELD_WIDTH - 1, GameConstants::BFIELD_HEIGHT - 2));
}

TEST(BattleFocusNavigationTest, VerticalDownFallsBackInwardAtLeftBoardEdge)
{
	BattleFocusModel model;
	ASSERT_TRUE(model.setFocus(BattleHex(0, GameConstants::BFIELD_HEIGHT - 2)));
	BattleFocusNavigation navigation(model);

	navigation.updateAxis(1, BattleFocusNavigation::Axis::VERTICAL, 1.0);

	ASSERT_TRUE(navigation.update(16));
	EXPECT_EQ(model.getFocusedHex(), BattleHex(0, GameConstants::BFIELD_HEIGHT - 1));
}

TEST(BattleFocusNavigationTest, ExplicitOutwardDiagonalDoesNotSlideAlongBoardEdge)
{
	BattleFocusModel model;
	ASSERT_TRUE(model.setFocus(BattleHex(GameConstants::BFIELD_WIDTH - 1, GameConstants::BFIELD_HEIGHT - 1)));
	BattleFocusNavigation navigation(model);

	navigation.updateAxis(1, BattleFocusNavigation::Axis::HORIZONTAL, 0.5);
	navigation.updateAxis(1, BattleFocusNavigation::Axis::VERTICAL, -0.9);

	EXPECT_FALSE(navigation.update(16));
	EXPECT_EQ(model.getFocusedHex(), BattleHex(GameConstants::BFIELD_WIDTH - 1, GameConstants::BFIELD_HEIGHT - 1));
}

TEST(BattleFocusNavigationTest, NearVerticalHeldInputContinuesAlongRightBoardEdge)
{
	BattleFocusModel model;
	ASSERT_TRUE(model.setFocus(BattleHex(GameConstants::BFIELD_WIDTH - 1, GameConstants::BFIELD_HEIGHT - 1)));
	BattleFocusNavigation navigation(model);

	navigation.updateAxis(1, BattleFocusNavigation::Axis::HORIZONTAL, 0.1);
	navigation.updateAxis(1, BattleFocusNavigation::Axis::VERTICAL, -1.0);

	ASSERT_TRUE(navigation.update(16));
	EXPECT_EQ(model.getFocusedHex(), BattleHex(GameConstants::BFIELD_WIDTH - 1, GameConstants::BFIELD_HEIGHT - 2));
	ASSERT_TRUE(navigation.update(320));
	EXPECT_EQ(model.getFocusedHex(), BattleHex(GameConstants::BFIELD_WIDTH - 1, GameConstants::BFIELD_HEIGHT - 3));
}

TEST(BattleFocusNavigationTest, HeldDirectionMovesAtMostOncePerTickAndRepeatsAfterDelay)
{
	BattleFocusModel model;
	ASSERT_TRUE(model.setFocus(BattleHex(8, 5)));
	BattleFocusNavigation navigation(model);

	navigation.updateAxis(1, BattleFocusNavigation::Axis::HORIZONTAL, 1.0);
	EXPECT_TRUE(navigation.update(16));
	const auto afterFirst = model.getFocusedHex();
	EXPECT_FALSE(navigation.update(319));
	EXPECT_EQ(model.getFocusedHex(), afterFirst);
	EXPECT_TRUE(navigation.update(1));
	const auto afterRepeat = model.getFocusedHex();
	EXPECT_NE(afterRepeat, afterFirst);
	EXPECT_TRUE(navigation.update(500));
	EXPECT_EQ(model.getFocusedHex(), afterRepeat.cloneInDirection(BattleHex::RIGHT, true));
}

TEST(BattleFocusNavigationTest, HysteresisPreventsDirectionFlappingAtSectorBoundary)
{
	BattleFocusModel model;
	ASSERT_TRUE(model.setFocus(BattleHex(8, 5)));
	BattleFocusNavigation navigation(model);

	navigation.updateAxis(1, BattleFocusNavigation::Axis::HORIZONTAL, 1.0);
	ASSERT_TRUE(navigation.update(16));
	const auto afterRight = model.getFocusedHex();

	// Roughly 35 degrees: quantization sees the neighbouring sector, but the
	// active direction remains RIGHT until it clears the 10-degree hysteresis.
	navigation.updateAxis(1, BattleFocusNavigation::Axis::VERTICAL, 0.7);
	ASSERT_TRUE(navigation.update(320));
	EXPECT_EQ(model.getFocusedHex(), afterRight.cloneInDirection(BattleHex::RIGHT, true));

	// 45 degrees clears the hysteresis and schedules a fresh settled move.
	navigation.updateAxis(1, BattleFocusNavigation::Axis::VERTICAL, 1.0);
	EXPECT_FALSE(navigation.update(15));
	EXPECT_TRUE(navigation.update(1));
	EXPECT_EQ(model.getFocusedHex(), afterRight.cloneInDirection(BattleHex::RIGHT, true)
		.cloneInDirection(BattleHex::BOTTOM_RIGHT, true));
}

TEST(BattleFocusNavigationTest, ResetAndDeviceChangeDiscardStaleAxisState)
{
	BattleFocusModel model;
	ASSERT_TRUE(model.setFocus(BattleHex(8, 5)));
	BattleFocusNavigation navigation(model);
	const auto start = model.getFocusedHex();

	navigation.updateAxis(1, BattleFocusNavigation::Axis::HORIZONTAL, 1.0);
	navigation.reset();
	EXPECT_FALSE(navigation.update(1000));

	navigation.updateAxis(1, BattleFocusNavigation::Axis::HORIZONTAL, 1.0);
	navigation.updateAxis(2, BattleFocusNavigation::Axis::VERTICAL, 1.0);
	EXPECT_TRUE(navigation.update(16));
	EXPECT_EQ(model.getFocusedHex(), start.cloneInDirection(BattleHex::BOTTOM_LEFT, true));
}
