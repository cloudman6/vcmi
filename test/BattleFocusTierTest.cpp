/*
 * BattleFocusTierTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../client/StdInc.h"

#include "../client/battle/BattleFocusTier.h"

#include <gtest/gtest.h>

using Tier = BattleFocusTier::Tier;

TEST(BattleFocusTierTest, noFocusProducesNoTier)
{
	EXPECT_FALSE(BattleFocusTier::classify(false, false, false, false).has_value());
}

TEST(BattleFocusTierTest, browseFocusDefaultsToNeutral)
{
	auto tier = BattleFocusTier::classify(true, false, false, false);
	ASSERT_TRUE(tier.has_value());
	EXPECT_EQ(*tier, Tier::NEUTRAL);
}

TEST(BattleFocusTierTest, movableHexOutranksNeutral)
{
	auto tier = BattleFocusTier::classify(true, true, false, false);
	ASSERT_TRUE(tier.has_value());
	EXPECT_EQ(*tier, Tier::MOVABLE);
}

TEST(BattleFocusTierTest, attackableTargetOutranksMovement)
{
	// attacking an enemy is the dominant affordance on an occupied hex
	auto tier = BattleFocusTier::classify(true, true, true, false);
	ASSERT_TRUE(tier.has_value());
	EXPECT_EQ(*tier, Tier::ATTACKABLE);
}

TEST(BattleFocusTierTest, illegalTargetDominatesEveryAffordance)
{
	// a focused target that cannot be committed must stay readable as
	// illegal even when the hex would otherwise be movable or attackable
	auto tier = BattleFocusTier::classify(true, true, true, true);
	ASSERT_TRUE(tier.has_value());
	EXPECT_EQ(*tier, Tier::ILLEGAL);

	tier = BattleFocusTier::classify(true, false, false, true);
	ASSERT_TRUE(tier.has_value());
	EXPECT_EQ(*tier, Tier::ILLEGAL);
}
