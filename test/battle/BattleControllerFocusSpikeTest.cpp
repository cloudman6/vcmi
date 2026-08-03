/*
 * BattleControllerFocusSpikeTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "BattleControllerFocusBattleFixture.h"
#include "BattleControllerFocusHarness.h"
#include "BattleControllerFocusSpikeFixture.h"

#include "../../client/battle/BattleActionsControllerMeleeTargeting.h"

#include <gtest/gtest.h>

namespace
{
class RecordingBattleActionsSink final : public BattleControllerFocusHarness::Sink
{
public:
	void preview(const BattleHex & hex) override
	{
		previewedHexes.push_back(hex);
	}

	void submit(const BattleHex & hex) override
	{
		submittedHexes.push_back(hex);
	}

	std::vector<BattleHex> previewedHexes;
	std::vector<BattleHex> submittedHexes;
};

class BattleControllerFocusSpikeTest : public testing::Test
{
protected:
	RecordingBattleActionsSink actions;
	BattleControllerFocusHarness focus{actions};
	BattleControllerFocusBattleFixture battleFixture;
};
}

TEST_F(BattleControllerFocusSpikeTest, MovesFocusThroughAllSixHexDirections)
{
	ASSERT_TRUE(focus.setFocus(BattleControllerFocusSpikeFixture::initialFocus));

	for(const auto direction : BattleControllerFocusSpikeFixture::sixWayDirections)
	{
		const auto expected = BattleControllerFocusSpikeFixture::initialFocus.cloneInDirection(direction, false);

		ASSERT_TRUE(focus.setFocus(BattleControllerFocusSpikeFixture::initialFocus));
		EXPECT_TRUE(focus.move(direction));
		EXPECT_EQ(focus.focusedHex(), expected);
	}

	EXPECT_EQ(actions.previewedHexes.size(), 13);
}

TEST_F(BattleControllerFocusSpikeTest, KeepsTargetFocusWhileSelectingEitherLegalMeleeApproachDirection)
{
	auto & attacker = battleFixture.addUnit(BattleHex(60), BattleSide::ATTACKER, true);
	auto & target = battleFixture.addUnit(BattleHex(90), BattleSide::DEFENDER, true);
	const std::array<BattleHex::EDir, 2> legalDirections =
	{
		BattleHex::EDir::TOP_LEFT,
		BattleHex::EDir::BOTTOM_RIGHT,
	};

	BattleHexArray availableApproaches;
	std::array<BattleHex, legalDirections.size()> expectedApproaches;

	for(size_t index = 0; index < legalDirections.size(); ++index)
	{
		expectedApproaches[index] = battleFixture.battle().fromWhichHexAttack(&attacker, target.getPosition(), legalDirections[index], false);
		ASSERT_TRUE(expectedApproaches[index].isValid());
		availableApproaches.insert(expectedApproaches[index]);
	}

	EXPECT_NE(expectedApproaches[0], expectedApproaches[1]);

	for(int direction = 0; direction < 8; ++direction)
	{
		const auto candidateDirection = static_cast<BattleHex::EDir>(direction);
		const bool expectedLegal = candidateDirection == legalDirections[0] || candidateDirection == legalDirections[1];
		EXPECT_EQ(battleFixture.battle().battleCanAttackHex(availableApproaches, &attacker, target.getPosition(), candidateDirection), expectedLegal);
	}

	ASSERT_TRUE(focus.setFocus(target.getPosition()));
	for(size_t index = 0; index < legalDirections.size(); ++index)
	{
		ASSERT_TRUE(focus.selectAttackDirection(legalDirections[index]));
		EXPECT_EQ(focus.attackDirection(), legalDirections[index]);
		EXPECT_EQ(focus.focusedHex(), target.getPosition());
		EXPECT_EQ(battle::controller::MeleeTargeting::resolve(battleFixture.battle(), availableApproaches, &attacker, target.getPosition(), legalDirections[index], false), expectedApproaches[index]);
	}

	EXPECT_EQ(battle::controller::MeleeTargeting::resolve(battleFixture.battle(), availableApproaches, &attacker, target.getPosition(), BattleHex::EDir::TOP_RIGHT, false), expectedApproaches[0]);
}

TEST_F(BattleControllerFocusSpikeTest, RejectsNonHexAttackDirectionsAndInvalidFocusMoves)
{
	EXPECT_FALSE(focus.selectAttackDirection(BattleHex::EDir::TOP));
	EXPECT_FALSE(focus.selectAttackDirection(BattleHex::EDir::BOTTOM));
	EXPECT_FALSE(focus.move(BattleHex::EDir::LEFT));
	EXPECT_FALSE(focus.confirm());
}

TEST_F(BattleControllerFocusSpikeTest, RefreshesAndRechecksFocusAfterWaitDefendAndSpellReplies)
{
	auto & actor = battleFixture.addUnit(BattleControllerFocusSpikeFixture::initialFocus, BattleSide::ATTACKER, false);
	BattleAction heroSpell;
	heroSpell.actionType = EActionType::HERO_SPELL;
	heroSpell.side = BattleSide::ATTACKER;

	const std::array<BattleAction, 3> replies =
	{
		BattleAction::makeWait(&actor),
		BattleAction::makeDefend(&actor),
		heroSpell,
	};

	for(const auto & reply : replies)
	{
		SCOPED_TRACE(static_cast<int>(reply.actionType));
		ASSERT_TRUE(focus.setFocus(BattleControllerFocusSpikeFixture::initialFocus));
		ASSERT_TRUE(focus.selectAttackDirection(BattleHex::EDir::RIGHT));

		const auto previewsBeforeReply = actions.previewedHexes.size();
		focus.onActionReply(reply);

		EXPECT_FALSE(focus.attackDirection().has_value());
		EXPECT_EQ(actions.previewedHexes.size(), previewsBeforeReply + 1);
		EXPECT_EQ(actions.previewedHexes.back(), BattleControllerFocusSpikeFixture::initialFocus);

		const auto submissionsBeforeConfirm = actions.submittedHexes.size();
		EXPECT_TRUE(focus.confirm());
		EXPECT_EQ(actions.submittedHexes.back(), BattleControllerFocusSpikeFixture::initialFocus);
		EXPECT_EQ(actions.submittedHexes.size(), submissionsBeforeConfirm + 1);

		EXPECT_FALSE(focus.setFocus(BattleHex::INVALID));
		EXPECT_FALSE(focus.confirm());
		EXPECT_EQ(actions.submittedHexes.size(), submissionsBeforeConfirm + 1);
	}
}

int main(int argc, char ** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
