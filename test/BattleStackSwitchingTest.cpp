/*
 * BattleStackSwitchingTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../client/StdInc.h"

#include "../client/battle/BattleStackSwitching.h"

#include <gtest/gtest.h>

namespace
{
std::vector<BattleStackSwitchEntry> sampleCandidates()
{
	// turn order as the battle queue would report it
	return {
		{10, BattleHex(2, 2), BattleHex(3, 2)},
		{20, BattleHex(5, 4), BattleHex::INVALID},
		{30, BattleHex(1, 6), BattleHex(2, 6)},
	};
}
}

TEST(BattleStackSwitchingTest, forwardCyclesInTurnOrderWithWrap)
{
	const auto candidates = sampleCandidates();

	EXPECT_EQ(BattleStackSwitching::select(candidates, BattleHex(2, 2), true).unitId, 20);
	EXPECT_EQ(BattleStackSwitching::select(candidates, BattleHex(5, 4), true).unitId, 30);
	// wrap around to the first candidate
	EXPECT_EQ(BattleStackSwitching::select(candidates, BattleHex(1, 6), true).unitId, 10);
}

TEST(BattleStackSwitchingTest, backwardCyclesInReverseOrderWithWrap)
{
	const auto candidates = sampleCandidates();

	EXPECT_EQ(BattleStackSwitching::select(candidates, BattleHex(5, 4), false).unitId, 10);
	EXPECT_EQ(BattleStackSwitching::select(candidates, BattleHex(2, 2), false).unitId, 30);
}

TEST(BattleStackSwitchingTest, wideUnitTailHexMatchesItsEntry)
{
	const auto candidates = sampleCandidates();

	// focus resting on the tail hex of a double-wide stack still identifies it
	EXPECT_EQ(BattleStackSwitching::select(candidates, BattleHex(3, 2), true).unitId, 20);
	EXPECT_EQ(BattleStackSwitching::select(candidates, BattleHex(2, 6), false).unitId, 20);
}

TEST(BattleStackSwitchingTest, selectionLandsOnHeadHex)
{
	const auto candidates = sampleCandidates();

	// D8: wide unit focus uses the head hex, never the tail
	EXPECT_EQ(BattleStackSwitching::select(candidates, BattleHex(3, 2), true).headHex, BattleHex(5, 4));
	EXPECT_EQ(BattleStackSwitching::select(candidates, BattleHex(5, 4), true).headHex, BattleHex(1, 6));
}

TEST(BattleStackSwitchingTest, focusOutsideCandidatesStartsAtFirstOrLast)
{
	const auto candidates = sampleCandidates();

	EXPECT_EQ(BattleStackSwitching::select(candidates, BattleHex(9, 9), true).unitId, 10);
	EXPECT_EQ(BattleStackSwitching::select(candidates, BattleHex(9, 9), false).unitId, 30);
}

TEST(BattleStackSwitchingTest, emptyCandidatesYieldInvalidEntry)
{
	const auto entry = BattleStackSwitching::select({}, BattleHex(2, 2), true);
	EXPECT_EQ(entry.unitId, -1);
	EXPECT_EQ(entry.headHex, BattleHex::INVALID);
}

TEST(BattleStackSwitchingTest, singleCandidateReturnsItself)
{
	const std::vector<BattleStackSwitchEntry> candidates = {{7, BattleHex(4, 4), BattleHex::INVALID}};

	EXPECT_EQ(BattleStackSwitching::select(candidates, BattleHex(4, 4), true).unitId, 7);
	EXPECT_EQ(BattleStackSwitching::select(candidates, BattleHex(9, 9), false).unitId, 7);
}
