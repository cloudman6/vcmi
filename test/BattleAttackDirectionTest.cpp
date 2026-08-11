/*
 * BattleAttackDirectionTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../client/StdInc.h"

#include "../client/battle/BattleAttackDirection.h"
#include "../client/battle/BattleControllerStateMachine.h"

#include <gtest/gtest.h>

using State = BattleControllerStateMachine::State;
using Outcome = BattleAttackDirection::MeleeOutcome;

namespace
{
std::vector<BattleHex> sampleOrigins()
{
	// direction scan order as fromWhichHexAttack would produce it
	return {BattleHex(6, 4), BattleHex(7, 4), BattleHex(8, 5)};
}
}

TEST(BattleAttackDirectionTest, recommendReturnsFirstScanOrderCandidate)
{
	EXPECT_EQ(BattleAttackDirection::recommend(sampleOrigins()), BattleHex(6, 4));
}

TEST(BattleAttackDirectionTest, recommendWithoutCandidatesYieldsInvalid)
{
	EXPECT_EQ(BattleAttackDirection::recommend({}), BattleHex::INVALID);
}

TEST(BattleAttackDirectionTest, cycleForwardWrapsThroughCandidates)
{
	const auto origins = sampleOrigins();

	EXPECT_EQ(BattleAttackDirection::cycle(origins, BattleHex(6, 4), true), BattleHex(7, 4));
	EXPECT_EQ(BattleAttackDirection::cycle(origins, BattleHex(8, 5), true), BattleHex(6, 4));
}

TEST(BattleAttackDirectionTest, cycleBackwardWrapsThroughCandidates)
{
	const auto origins = sampleOrigins();

	EXPECT_EQ(BattleAttackDirection::cycle(origins, BattleHex(7, 4), false), BattleHex(6, 4));
	EXPECT_EQ(BattleAttackDirection::cycle(origins, BattleHex(6, 4), false), BattleHex(8, 5));
}

TEST(BattleAttackDirectionTest, cycleWithoutCurrentSelectionStartsAtEitherEnd)
{
	const auto origins = sampleOrigins();

	EXPECT_EQ(BattleAttackDirection::cycle(origins, BattleHex(1, 1), true), BattleHex(6, 4));
	EXPECT_EQ(BattleAttackDirection::cycle(origins, BattleHex(1, 1), false), BattleHex(8, 5));
}

TEST(BattleAttackDirectionTest, cycleWithoutCandidatesYieldsInvalid)
{
	EXPECT_EQ(BattleAttackDirection::cycle({}, BattleHex(6, 4), true), BattleHex::INVALID);
}

TEST(BattleAttackDirectionTest, acceptWalksTheMeleeLayersInOrder)
{
	EXPECT_EQ(BattleAttackDirection::decideAccept(State::BROWSE, true), Outcome::START_ACTION);
	EXPECT_EQ(BattleAttackDirection::decideAccept(State::ACTION, true), Outcome::OPEN_DIRECTION);
	EXPECT_EQ(BattleAttackDirection::decideAccept(State::ATTACK_DIRECTION, true), Outcome::COMMIT);
}

TEST(BattleAttackDirectionTest, acceptWithoutTargetCancelsDeeperMeleeLayers)
{
	// BT-07 target-lost: the enemy stack died or became unreachable while the
	// player was choosing an approach - back out one layer instead of commit
	EXPECT_EQ(BattleAttackDirection::decideAccept(State::BROWSE, false), Outcome::NONE);
	EXPECT_EQ(BattleAttackDirection::decideAccept(State::ACTION, false), Outcome::CANCEL_LAYER);
	EXPECT_EQ(BattleAttackDirection::decideAccept(State::ATTACK_DIRECTION, false), Outcome::CANCEL_LAYER);
}

TEST(BattleAttackDirectionTest, acceptHasNoMeleeMeaningInOtherLayers)
{
	for(bool attackable : {true, false})
	{
		EXPECT_EQ(BattleAttackDirection::decideAccept(State::TARGET, attackable), Outcome::NONE);
		EXPECT_EQ(BattleAttackDirection::decideAccept(State::PREVIEW, attackable), Outcome::NONE);
		EXPECT_EQ(BattleAttackDirection::decideAccept(State::COMMIT, attackable), Outcome::NONE);
	}
}
