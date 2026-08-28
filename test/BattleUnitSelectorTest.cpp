/*
 * BattleUnitSelectorTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "../client/StdInc.h"

#include "../client/battle/BattleUnitSelector.h"

#include <gtest/gtest.h>

namespace
{
BattleUnitNavigationCandidate candidate(uint32_t unitId, BattleHex headHex,
	BattleHex tailHex = BattleHex::INVALID)
{
	return {unitId, headHex, tailHex};
}
}

TEST(BattleUnitSelectorTest, ChoosesNearerCandidateWhenForwardAlignmentMatches)
{
	const std::vector candidates = {
		candidate(20, BattleHex(9, 5)),
		candidate(10, BattleHex(7, 5))
	};

	const auto selected = BattleUnitSelector::select(candidates, BattleHex(5, 5), 1.0, 0.0);
	ASSERT_TRUE(selected.has_value());
	EXPECT_EQ(selected->unitId, 10);
}

TEST(BattleUnitSelectorTest, ForwardAlignmentOutranksDistance)
{
	const std::vector candidates = {
		candidate(10, BattleHex(6, 4)),
		candidate(20, BattleHex(9, 5))
	};

	const auto selected = BattleUnitSelector::select(candidates, BattleHex(5, 5), 1.0, 0.0);
	ASSERT_TRUE(selected.has_value());
	EXPECT_EQ(selected->unitId, 20);
}

TEST(BattleUnitSelectorTest, UsesOccupiedCenterButReturnsHeadForDoubleWideUnit)
{
	const std::vector candidates = {
		candidate(10, BattleHex(8, 4)),
		candidate(20, BattleHex(8, 5), BattleHex(9, 5))
	};

	const auto selected = BattleUnitSelector::select(candidates, BattleHex(5, 5), 1.0, 0.0);
	ASSERT_TRUE(selected.has_value());
	EXPECT_EQ(selected->unitId, 20);
	EXPECT_EQ(selected->headHex, BattleHex(8, 5));
}

TEST(BattleUnitSelectorTest, StableUnitIdBreaksGeometricTieIndependentlyOfInputOrder)
{
	const auto highId = candidate(20, BattleHex(8, 5));
	const auto lowId = candidate(10, BattleHex(8, 5));

	for(const auto & candidates : {std::vector{highId, lowId}, std::vector{lowId, highId}})
	{
		const auto selected = BattleUnitSelector::select(candidates, BattleHex(5, 5), 1.0, 0.0);
		ASSERT_TRUE(selected.has_value());
		EXPECT_EQ(selected->unitId, 10);
	}
}

TEST(BattleUnitSelectorTest, ExcludesCurrentUnitFromEitherOccupiedHex)
{
	const auto current = candidate(10, BattleHex(5, 5), BattleHex(4, 5));
	const auto next = candidate(20, BattleHex(8, 5));

	for(const auto focus : {current.headHex, current.tailHex})
	{
		const auto selected = BattleUnitSelector::select({current, next}, focus, 1.0, 0.0);
		ASSERT_TRUE(selected.has_value());
		EXPECT_EQ(selected->unitId, 20);
	}
}

TEST(BattleUnitSelectorTest, ExcludesMalformedCandidates)
{
	const std::vector candidates = {
		candidate(20, BattleHex::INVALID),
		candidate(30, BattleHex(8, 5))
	};

	const auto selected = BattleUnitSelector::select(candidates, BattleHex(5, 5), 1.0, 0.0);
	ASSERT_TRUE(selected.has_value());
	EXPECT_EQ(selected->unitId, 30);
}

TEST(BattleUnitSelectorTest, NoForwardCandidateLeavesFocusUnchanged)
{
	const std::vector candidates = {
		candidate(10, BattleHex(4, 5)),
		candidate(20, BattleHex(5, 3))
	};

	EXPECT_FALSE(BattleUnitSelector::select(candidates, BattleHex(5, 5), 1.0, 0.0));
	EXPECT_FALSE(BattleUnitSelector::select(candidates, BattleHex(5, 5), 0.0, 0.0));
}

TEST(BattleUnitSelectorTest, RecomputesFromCurrentCandidatesAfterTargetDisappears)
{
	const auto first = candidate(10, BattleHex(7, 5));
	const auto second = candidate(20, BattleHex(9, 5));
	ASSERT_EQ(BattleUnitSelector::select({first, second}, BattleHex(5, 5), 1.0, 0.0)->unitId, 10);

	const auto selected = BattleUnitSelector::select({second}, BattleHex(5, 5), 1.0, 0.0);
	ASSERT_TRUE(selected.has_value());
	EXPECT_EQ(selected->unitId, 20);
}
