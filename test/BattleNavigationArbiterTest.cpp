/*
 * BattleNavigationArbiterTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "../client/StdInc.h"

#include "../client/battle/BattleNavigationArbiter.h"

#include <gtest/gtest.h>

TEST(BattleNavigationArbiterTest, FirstActiveNavigationOwnsFocusUntilReleased)
{
	BattleNavigationArbiter arbiter;

	arbiter.update(BattleNavigationArbiter::Source::HEX, true, false);
	EXPECT_EQ(arbiter.owner(), BattleNavigationArbiter::Source::HEX);

	arbiter.update(BattleNavigationArbiter::Source::UNIT, true, true);
	EXPECT_EQ(arbiter.owner(), BattleNavigationArbiter::Source::HEX);
}

TEST(BattleNavigationArbiterTest, ReleasingOwnerHandsOffToHeldNavigation)
{
	BattleNavigationArbiter arbiter;
	arbiter.update(BattleNavigationArbiter::Source::HEX, true, false);
	arbiter.update(BattleNavigationArbiter::Source::UNIT, true, true);

	arbiter.update(BattleNavigationArbiter::Source::HEX, false, true);
	EXPECT_EQ(arbiter.owner(), BattleNavigationArbiter::Source::UNIT);
}

TEST(BattleNavigationArbiterTest, ReleasingOnlyActiveNavigationClearsOwnership)
{
	BattleNavigationArbiter arbiter;
	arbiter.update(BattleNavigationArbiter::Source::UNIT, true, false);

	arbiter.update(BattleNavigationArbiter::Source::UNIT, false, false);
	EXPECT_EQ(arbiter.owner(), BattleNavigationArbiter::Source::NONE);
}

TEST(BattleNavigationArbiterTest, ResetClearsOwnership)
{
	BattleNavigationArbiter arbiter;
	arbiter.update(BattleNavigationArbiter::Source::HEX, true, false);

	arbiter.reset();
	EXPECT_EQ(arbiter.owner(), BattleNavigationArbiter::Source::NONE);
}
