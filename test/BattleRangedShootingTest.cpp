/*
 * BattleRangedShootingTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../client/StdInc.h"

#include "../client/battle/BattleControllerStateMachine.h"
#include "../client/battle/BattleRangedShooting.h"

#include <gtest/gtest.h>

using State = BattleControllerStateMachine::State;
using Outcome = BattleRangedShooting::Outcome;
using Reason = BattleRangedShooting::DisabledReason;

TEST(BattleRangedShootingTest, classifyShootableTargetYieldsNoReason)
{
	EXPECT_EQ(BattleRangedShooting::classify(true, true, false, true), Reason::NONE);
}

TEST(BattleRangedShootingTest, classifyNonEnemyTargetIsNotAShootingConcern)
{
	// an empty or own-side focus never produces a disabled shoot reason
	EXPECT_EQ(BattleRangedShooting::classify(false, false, true, false), Reason::NONE);
}

TEST(BattleRangedShootingTest, classifyNoAmmoBeatsOtherReasons)
{
	// out of shots: the other failing rules add no information
	EXPECT_EQ(BattleRangedShooting::classify(true, false, true, false), Reason::NO_AMMO);
	EXPECT_EQ(BattleRangedShooting::classify(true, false, false, true), Reason::NO_AMMO);
}

TEST(BattleRangedShootingTest, classifyBlockedLineBeatsOutOfRange)
{
	// an adjacent enemy blocks the shot regardless of distance
	EXPECT_EQ(BattleRangedShooting::classify(true, true, true, false), Reason::BLOCKED_LINE);
	EXPECT_EQ(BattleRangedShooting::classify(true, true, true, true), Reason::BLOCKED_LINE);
}

TEST(BattleRangedShootingTest, classifyOutOfRange)
{
	EXPECT_EQ(BattleRangedShooting::classify(true, true, false, false), Reason::OUT_OF_RANGE);
}

TEST(BattleRangedShootingTest, acceptOpensTheShootingLayerFromBrowse)
{
	EXPECT_EQ(BattleRangedShooting::decideAccept(State::BROWSE, true), Outcome::START_ACTION);
	// illegal targets stay focusable but never open the shooting layer
	EXPECT_EQ(BattleRangedShooting::decideAccept(State::BROWSE, false), Outcome::NONE);
}

TEST(BattleRangedShootingTest, acceptCommitsFromAction)
{
	// shooting has no direction layer: the second A commits directly
	EXPECT_EQ(BattleRangedShooting::decideAccept(State::ACTION, true), Outcome::COMMIT);
}

TEST(BattleRangedShootingTest, acceptCancelsTheLayerWhenTargetBecomesDisabled)
{
	// BT-07: the target died, moved out of range or spent its last shot
	EXPECT_EQ(BattleRangedShooting::decideAccept(State::ACTION, false), Outcome::CANCEL_LAYER);
}

TEST(BattleRangedShootingTest, acceptHasNoShootingMeaningInOtherLayers)
{
	for(bool shootable : {true, false})
	{
		EXPECT_EQ(BattleRangedShooting::decideAccept(State::TARGET, shootable), Outcome::NONE);
		EXPECT_EQ(BattleRangedShooting::decideAccept(State::ATTACK_DIRECTION, shootable), Outcome::NONE);
		EXPECT_EQ(BattleRangedShooting::decideAccept(State::PREVIEW, shootable), Outcome::NONE);
		EXPECT_EQ(BattleRangedShooting::decideAccept(State::COMMIT, shootable), Outcome::NONE);
	}
}
