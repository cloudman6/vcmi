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

#include "../../client/battle/BattleActionsController.h"
#include "../../lib/battle/Unit.h"

#include <gtest/gtest.h>

class BattleActionsControllerFocusTestAccess
{
public:
	static battle::controller::MeleeActionDecision actionIsLegal(const CBattleInfoCallback & battle, const BattleHexArray & availableHexes, const battle::Unit * attacker, const battle::Unit * target, const BattleControllerFocusHarness::Selection & selection)
	{
		return BattleActionsController::actionIsLegal(battle, availableHexes, attacker, target, selection, false);
	}

	static bool actionRealize(const CBattleInfoCallback & battle, const BattleHexArray & availableHexes, const battle::Unit * attacker, const battle::Unit * target, const BattleControllerFocusHarness::Selection & selection, const std::function<void(const BattleHex &)> & submit)
	{
		return BattleActionsController::actionRealize(battle, availableHexes, attacker, target, selection, false, submit);
	}
};

namespace
{
/// Focus transport probe only. The melee legality and realization oracles below
/// invoke BattleActionsController's real private action seam.
class SelectionCaptureSink final : public BattleControllerFocusHarness::Sink
{
public:
	void preview(const BattleControllerFocusHarness::Selection & selection) override
	{
		previewedSelections.push_back(selection);
	}

	void submit(const BattleControllerFocusHarness::Selection & selection) override
	{
		submittedSelections.push_back(selection);
	}

	std::vector<BattleControllerFocusHarness::Selection> previewedSelections;
	std::vector<BattleControllerFocusHarness::Selection> submittedSelections;
};

class BattleControllerFocusSpikeTest : public testing::Test
{
protected:
	SelectionCaptureSink actions;
	BattleActionLifecycle lifecycle;
	BattleControllerFocusHarness focus{actions, lifecycle};
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

	EXPECT_EQ(actions.previewedSelections.size(), 13);
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
		const auto & previewSelection = actions.previewedSelections.back();
		EXPECT_EQ(previewSelection.attackDirection, legalDirections[index]);
		const auto previewDecision = BattleActionsControllerFocusTestAccess::actionIsLegal(battleFixture.battle(), availableApproaches, &attacker, &target, previewSelection);
		EXPECT_TRUE(previewDecision.isLegal());
		EXPECT_EQ(previewDecision.approachHex, expectedApproaches[index]);

		ASSERT_TRUE(focus.confirm());
		const auto & confirmSelection = actions.submittedSelections.back();
		EXPECT_EQ(confirmSelection.attackDirection, legalDirections[index]);
		BattleHex confirmedApproach = BattleHex::INVALID;
		EXPECT_TRUE(BattleActionsControllerFocusTestAccess::actionRealize(battleFixture.battle(), availableApproaches, &attacker, &target, confirmSelection, [&confirmedApproach](const BattleHex & approachHex)
		{
			confirmedApproach = approachHex;
		}));
		EXPECT_EQ(confirmedApproach, expectedApproaches[index]);
	}

	ASSERT_TRUE(focus.selectAttackDirection(BattleHex::EDir::TOP_RIGHT));
	const auto invalidSelection = actions.previewedSelections.back();
	EXPECT_EQ(invalidSelection.attackDirection, BattleHex::EDir::TOP_RIGHT);
	EXPECT_EQ(BattleActionsControllerFocusTestAccess::actionIsLegal(battleFixture.battle(), availableApproaches, &attacker, &target, invalidSelection).approachHex, expectedApproaches[0]);
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
	std::vector<BattleActionLifecycle::Stage> observedStages;
	lifecycle.subscribe([&observedStages](BattleActionLifecycle::Stage stage, const BattleAction &)
	{
		observedStages.push_back(stage);
	});

	for(const auto & reply : replies)
	{
		SCOPED_TRACE(static_cast<int>(reply.actionType));
		ASSERT_TRUE(focus.setFocus(BattleControllerFocusSpikeFixture::initialFocus));
		ASSERT_TRUE(focus.selectAttackDirection(BattleHex::EDir::RIGHT));

		const auto previewsBeforeReply = actions.previewedSelections.size();
		const auto stagesBeforeReply = observedStages.size();
		lifecycle.giveCommand(reply);
		lifecycle.sendCommand(reply);
		lifecycle.startAction(reply);
		lifecycle.endAction(reply);
		ASSERT_EQ(observedStages.size(), stagesBeforeReply + 4);
		EXPECT_EQ(observedStages[stagesBeforeReply], BattleActionLifecycle::Stage::COMMAND_GIVEN);
		EXPECT_EQ(observedStages[stagesBeforeReply + 1], BattleActionLifecycle::Stage::COMMAND_SENT);
		EXPECT_EQ(observedStages[stagesBeforeReply + 2], BattleActionLifecycle::Stage::ACTION_STARTED);
		EXPECT_EQ(observedStages[stagesBeforeReply + 3], BattleActionLifecycle::Stage::ACTION_ENDED);

		EXPECT_FALSE(focus.attackDirection().has_value());
		EXPECT_EQ(actions.previewedSelections.size(), previewsBeforeReply + 1);
		EXPECT_EQ(actions.previewedSelections.back().targetHex, BattleControllerFocusSpikeFixture::initialFocus);

		const auto submissionsBeforeConfirm = actions.submittedSelections.size();
		EXPECT_TRUE(focus.confirm());
		EXPECT_EQ(actions.submittedSelections.back().targetHex, BattleControllerFocusSpikeFixture::initialFocus);
		EXPECT_EQ(actions.submittedSelections.size(), submissionsBeforeConfirm + 1);

		EXPECT_FALSE(focus.setFocus(BattleHex::INVALID));
		EXPECT_FALSE(focus.confirm());
		EXPECT_EQ(actions.submittedSelections.size(), submissionsBeforeConfirm + 1);
	}
}

int main(int argc, char ** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
