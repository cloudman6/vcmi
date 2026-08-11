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

TEST(BattleFocusTierTest, neutralVisualUsesPlainHighlight)
{
	auto visual = BattleFocusTier::visual(Tier::NEUTRAL);
	EXPECT_FALSE(visual.shadeOverlay);
	EXPECT_FALSE(visual.borderOverlay);
	EXPECT_FALSE(visual.dimmedHighlight);
}

TEST(BattleFocusTierTest, movableVisualAddsShadeOverlay)
{
	auto visual = BattleFocusTier::visual(Tier::MOVABLE);
	EXPECT_TRUE(visual.shadeOverlay);
	EXPECT_FALSE(visual.borderOverlay);
	EXPECT_FALSE(visual.dimmedHighlight);
}

TEST(BattleFocusTierTest, attackableVisualAddsBorderOverlay)
{
	auto visual = BattleFocusTier::visual(Tier::ATTACKABLE);
	EXPECT_FALSE(visual.shadeOverlay);
	EXPECT_TRUE(visual.borderOverlay);
	EXPECT_FALSE(visual.dimmedHighlight);
}

TEST(BattleFocusTierTest, illegalVisualDimsAndShadesHighlight)
{
	auto visual = BattleFocusTier::visual(Tier::ILLEGAL);
	EXPECT_TRUE(visual.shadeOverlay);
	EXPECT_FALSE(visual.borderOverlay);
	EXPECT_TRUE(visual.dimmedHighlight);
}

TEST(BattleFocusTierTest, everyTierPairDiffersWithoutColor)
{
	// freeze F-7: neutral/movable/attackable must stay distinguishable
	// without relying on hue, so each tier pair needs a differing cue
	auto cues = [](Tier tier)
	{
		auto v = BattleFocusTier::visual(tier);
		return std::make_tuple(v.shadeOverlay, v.borderOverlay, v.dimmedHighlight);
	};

	EXPECT_NE(cues(Tier::NEUTRAL), cues(Tier::MOVABLE));
	EXPECT_NE(cues(Tier::NEUTRAL), cues(Tier::ATTACKABLE));
	EXPECT_NE(cues(Tier::NEUTRAL), cues(Tier::ILLEGAL));
	EXPECT_NE(cues(Tier::MOVABLE), cues(Tier::ATTACKABLE));
	EXPECT_NE(cues(Tier::MOVABLE), cues(Tier::ILLEGAL));
	EXPECT_NE(cues(Tier::ATTACKABLE), cues(Tier::ILLEGAL));
}
