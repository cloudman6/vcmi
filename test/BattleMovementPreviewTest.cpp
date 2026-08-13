/*
 * BattleMovementPreviewTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../client/StdInc.h"

#include "../client/battle/BattleControllerStateMachine.h"
#include "../client/battle/BattleMovementPreview.h"

#include "lib/battle/Unit.h"

#include <gtest/gtest.h>

using State = BattleControllerStateMachine::State;
using Outcome = BattleMovementPreview::Outcome;

TEST(BattleMovementPreviewTest, acceptFromBrowseCommitsVisiblePreviewWhenFocusIsReachable)
{
	EXPECT_EQ(BattleMovementPreview::decideAccept(State::BROWSE, true), Outcome::COMMIT);
	EXPECT_EQ(BattleMovementPreview::decideAccept(State::BROWSE, false), Outcome::NONE);
}

TEST(BattleMovementPreviewTest, acceptFromPreviewCommitsWhileFocusStaysReachable)
{
	EXPECT_EQ(BattleMovementPreview::decideAccept(State::PREVIEW, true), Outcome::COMMIT);
}

TEST(BattleMovementPreviewTest, acceptFromPreviewCancelsWhenFocusBecomesUnreachable)
{
	// invalidation refresh: the destination stopped being legal (server
	// refresh, obstacle, range change) - A backs out instead of committing
	EXPECT_EQ(BattleMovementPreview::decideAccept(State::PREVIEW, false), Outcome::CANCEL_PREVIEW);
}

TEST(BattleMovementPreviewTest, acceptHasNoPreviewMeaningInOtherLayers)
{
	for(bool reachable : {true, false})
	{
		EXPECT_EQ(BattleMovementPreview::decideAccept(State::ACTION, reachable), Outcome::NONE);
		EXPECT_EQ(BattleMovementPreview::decideAccept(State::TARGET, reachable), Outcome::NONE);
		EXPECT_EQ(BattleMovementPreview::decideAccept(State::ATTACK_DIRECTION, reachable), Outcome::NONE);
		EXPECT_EQ(BattleMovementPreview::decideAccept(State::COMMIT, reachable), Outcome::NONE);
	}
}

TEST(BattleMovementPreviewTest, wideUnitLandingHexIsOnTheSideFacingOwnArmy)
{
	// BT-02: the landing position of a double-wide stack occupies the head
	// hex and one adjacent tail hex; VCMI anchors positions on the head hex
	const BattleHex head(6, 4);

	EXPECT_EQ(battle::Unit::occupiedHex(head, true, BattleSide::ATTACKER), BattleHex(head.toInt() - 1));
	EXPECT_EQ(battle::Unit::occupiedHex(head, true, BattleSide::DEFENDER), BattleHex(head.toInt() + 1));
	EXPECT_EQ(battle::Unit::occupiedHex(head, false, BattleSide::ATTACKER), BattleHex::INVALID);
}
