/*
 * BattleUnitNavigationTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "../client/StdInc.h"

#include "../client/battle/BattleUnitNavigation.h"

#include <gtest/gtest.h>

namespace
{
BattleUnitNavigationCandidate candidate(uint32_t unitId, BattleHex headHex)
{
	return {unitId, headHex, BattleHex::INVALID};
}
}

TEST(BattleUnitNavigationTest, InitialPushSelectsOnceAfterSettleDelay)
{
	BattleFocusModel focus;
	ASSERT_TRUE(focus.setFocus(BattleHex(5, 5)));
	BattleUnitNavigation navigation(focus, []
	{
		return std::vector{candidate(10, BattleHex(7, 5))};
	});

	navigation.updateAxis(7, BattleUnitNavigation::Axis::HORIZONTAL, 1.0);
	EXPECT_FALSE(navigation.update(15));
	EXPECT_TRUE(navigation.update(1));
	EXPECT_EQ(focus.getFocusedHex(), BattleHex(7, 5));
	EXPECT_FALSE(navigation.update(1));
}

TEST(BattleUnitNavigationTest, HeldRepeatContinuesFromNewSharedFocus)
{
	BattleFocusModel focus;
	ASSERT_TRUE(focus.setFocus(BattleHex(5, 5)));
	BattleUnitNavigation navigation(focus, []
	{
		return std::vector{candidate(10, BattleHex(7, 5)), candidate(20, BattleHex(9, 5))};
	});

	navigation.updateAxis(7, BattleUnitNavigation::Axis::HORIZONTAL, 1.0);
	ASSERT_TRUE(navigation.update(16));
	EXPECT_EQ(focus.getFocusedHex(), BattleHex(7, 5));
	EXPECT_FALSE(navigation.update(319));
	EXPECT_TRUE(navigation.update(1));
	EXPECT_EQ(focus.getFocusedHex(), BattleHex(9, 5));
}

TEST(BattleUnitNavigationTest, LargeTickPerformsAtMostOneSelection)
{
	BattleFocusModel focus;
	ASSERT_TRUE(focus.setFocus(BattleHex(3, 5)));
	BattleUnitNavigation navigation(focus, []
	{
		return std::vector{
			candidate(10, BattleHex(5, 5)),
			candidate(20, BattleHex(7, 5)),
			candidate(30, BattleHex(9, 5))};
	});

	navigation.updateAxis(7, BattleUnitNavigation::Axis::HORIZONTAL, 1.0);
	ASSERT_TRUE(navigation.update(500));
	EXPECT_EQ(focus.getFocusedHex(), BattleHex(5, 5));
	ASSERT_TRUE(navigation.update(500));
	EXPECT_EQ(focus.getFocusedHex(), BattleHex(7, 5));
}

TEST(BattleUnitNavigationTest, NeutralReleaseAndResetDiscardPendingRepeat)
{
	BattleFocusModel focus;
	ASSERT_TRUE(focus.setFocus(BattleHex(5, 5)));
	BattleUnitNavigation navigation(focus, []
	{
		return std::vector{candidate(10, BattleHex(7, 5))};
	});

	navigation.updateAxis(7, BattleUnitNavigation::Axis::HORIZONTAL, 1.0);
	navigation.updateAxis(7, BattleUnitNavigation::Axis::HORIZONTAL, 0.0);
	EXPECT_FALSE(navigation.update(1000));

	navigation.updateAxis(7, BattleUnitNavigation::Axis::HORIZONTAL, 1.0);
	navigation.reset();
	EXPECT_FALSE(navigation.update(1000));
	EXPECT_EQ(focus.getFocusedHex(), BattleHex(5, 5));
}

TEST(BattleUnitNavigationTest, DeviceChangeCannotReuseOtherControllersAxisComponent)
{
	BattleFocusModel focus;
	ASSERT_TRUE(focus.setFocus(BattleHex(5, 5)));
	BattleUnitNavigation navigation(focus, []
	{
		return std::vector{
			candidate(10, BattleHex(8, 5)),
			candidate(20, BattleHex(5, 7))};
	});

	navigation.updateAxis(1, BattleUnitNavigation::Axis::HORIZONTAL, 1.0);
	navigation.updateAxis(2, BattleUnitNavigation::Axis::VERTICAL, 1.0);
	ASSERT_TRUE(navigation.update(16));
	EXPECT_EQ(focus.getFocusedHex(), BattleHex(5, 7));
}

TEST(BattleUnitNavigationTest, RepeatUsesFreshCandidatesAfterDisappearance)
{
	BattleFocusModel focus;
	ASSERT_TRUE(focus.setFocus(BattleHex(5, 5)));
	std::vector candidates{candidate(10, BattleHex(7, 5)), candidate(20, BattleHex(9, 5))};
	BattleUnitNavigation navigation(focus, [&candidates]
	{
		return candidates;
	});

	navigation.updateAxis(7, BattleUnitNavigation::Axis::HORIZONTAL, 1.0);
	ASSERT_TRUE(navigation.update(16));
	EXPECT_EQ(focus.getFocusedHex(), BattleHex(7, 5));
	candidates.erase(candidates.begin());
	ASSERT_TRUE(navigation.update(320));
	EXPECT_EQ(focus.getFocusedHex(), BattleHex(9, 5));
}

TEST(BattleUnitNavigationTest, NoForwardCandidateHasNoFocusOrPointerFallback)
{
	BattleFocusModel focus;
	ASSERT_TRUE(focus.setFocus(BattleHex(5, 5)));
	BattleUnitNavigation navigation(focus, []
	{
		return std::vector{candidate(10, BattleHex(3, 5))};
	});

	navigation.updateAxis(7, BattleUnitNavigation::Axis::HORIZONTAL, 1.0);
	EXPECT_FALSE(navigation.update(16));
	EXPECT_EQ(focus.getFocusedHex(), BattleHex(5, 5));
}
