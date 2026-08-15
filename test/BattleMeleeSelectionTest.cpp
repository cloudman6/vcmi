/*
 * BattleMeleeSelectionTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "../client/StdInc.h"

#include "../client/battle/BattleControllerAction.h"
#include "../client/battle/BattleMeleeSelection.h"

#include <gtest/gtest.h>

namespace
{
using Action = PossiblePlayerBattleAction::Actions;
using Candidate = BattleMeleeSelection::Candidate;

const BattleHex target(9, 5);
constexpr uint32_t targetUnitId = 42;
const Candidate left{BattleHex(8, 5), BattleHex::LEFT};
const Candidate topLeft{BattleHex(8, 4), BattleHex::TOP_LEFT};
const Candidate bottomLeft{BattleHex(8, 6), BattleHex::BOTTOM_LEFT};

BattleControllerMeleeOriginRepeatContext repeatContext(const BattleMeleeSelection & selection)
{
	const auto candidate = selection.getCandidate();
	return {
		selection.getAction(), selection.getTarget(), selection.getTargetUnitId(),
		candidate.attackFrom, candidate.direction
	};
}
}

TEST(BattleMeleeSelectionTest, RefreshCreatesTypedSelectionForEveryMeleeAction)
{
	for(const auto action : {Action::ATTACK, Action::LONG_WEAPON_ATTACK,
		Action::WALK_AND_ATTACK, Action::ATTACK_AND_RETURN})
	{
		BattleMeleeSelection selection;
		ASSERT_TRUE(selection.refresh(action, target, targetUnitId, {left, topLeft}));
		EXPECT_EQ(selection.getAction(), action);
		EXPECT_EQ(selection.getTarget(), target);
		EXPECT_EQ(selection.getTargetUnitId(), targetUnitId);
		EXPECT_EQ(selection.getCandidate(), left);
		EXPECT_EQ(selection.returnsAfterAttack(), action == Action::ATTACK_AND_RETURN);
	}
}

TEST(BattleMeleeSelectionTest, RefreshRejectsNonMeleeTargetsAndEmptyCandidates)
{
	BattleMeleeSelection selection;

	EXPECT_FALSE(selection.refresh(Action::SHOOT, target, targetUnitId, {left}));
	EXPECT_FALSE(selection.isValid());
	EXPECT_FALSE(selection.refresh(Action::ATTACK, BattleHex::INVALID, targetUnitId, {left}));
	EXPECT_FALSE(selection.refresh(Action::ATTACK, target, targetUnitId, {}));
}

TEST(BattleMeleeSelectionTest, InvalidAndDuplicateCandidatesNeverEnterTheCycle)
{
	BattleMeleeSelection selection;
	const Candidate invalidHex{BattleHex::INVALID, BattleHex::LEFT};
	const Candidate invalidDirection{BattleHex(7, 5), BattleHex::NONE};
	const Candidate duplicateOrigin{left.attackFrom, BattleHex::TOP_LEFT};

	ASSERT_TRUE(selection.refresh(Action::ATTACK, target, targetUnitId,
		{invalidHex, left, invalidDirection, duplicateOrigin, topLeft}));
	EXPECT_EQ(selection.getCandidate(), left);
	ASSERT_TRUE(selection.cycle(true));
	EXPECT_EQ(selection.getCandidate(), topLeft);
	ASSERT_TRUE(selection.cycle(true));
	EXPECT_EQ(selection.getCandidate(), left);
}

TEST(BattleMeleeSelectionTest, RefreshPreservesOnlyAnExactStillLegalCandidate)
{
	BattleMeleeSelection selection;
	ASSERT_TRUE(selection.refresh(Action::WALK_AND_ATTACK, target, targetUnitId, {left, topLeft, bottomLeft}));
	ASSERT_TRUE(selection.cycle(true));
	ASSERT_EQ(selection.getCandidate(), topLeft);

	ASSERT_TRUE(selection.refresh(Action::WALK_AND_ATTACK, target, targetUnitId, {bottomLeft, topLeft}));
	EXPECT_EQ(selection.getCandidate(), topLeft);

	ASSERT_TRUE(selection.refresh(Action::WALK_AND_ATTACK, target, targetUnitId, {left, bottomLeft}));
	EXPECT_EQ(selection.getCandidate(), left);
}

TEST(BattleMeleeSelectionTest, CycleWrapsAndSingleCandidateDoesNotConsumeInput)
{
	BattleMeleeSelection selection;
	ASSERT_TRUE(selection.refresh(Action::ATTACK, target, targetUnitId, {left, topLeft, bottomLeft}));
	EXPECT_TRUE(selection.hasAlternativeCandidates());

	ASSERT_TRUE(selection.cycle(false));
	EXPECT_EQ(selection.getCandidate(), bottomLeft);
	ASSERT_TRUE(selection.cycle(true));
	EXPECT_EQ(selection.getCandidate(), left);

	ASSERT_TRUE(selection.refresh(Action::ATTACK, target, targetUnitId, {left}));
	EXPECT_FALSE(selection.hasAlternativeCandidates());
	EXPECT_FALSE(selection.cycle(true));
	EXPECT_EQ(selection.getCandidate(), left);
}

TEST(BattleMeleeSelectionTest, RepeatedCycleEventsAdvanceExactlyOneCandidateEach)
{
	BattleMeleeSelection selection;
	ASSERT_TRUE(selection.refresh(Action::ATTACK, target, targetUnitId, {left, topLeft, bottomLeft}));

	ASSERT_TRUE(selection.cycle(true));
	EXPECT_EQ(selection.getCandidate(), topLeft);
	ASSERT_TRUE(selection.cycle(true));
	EXPECT_EQ(selection.getCandidate(), bottomLeft);
	ASSERT_TRUE(selection.cycle(false));
	EXPECT_EQ(selection.getCandidate(), topLeft);
}

TEST(BattleMeleeSelectionTest, RefreshFallbackInvalidatesHeldRepeatContext)
{
	BattleMeleeSelection selection;
	BattleControllerMeleeOriginRepeatState repeat;
	ASSERT_TRUE(selection.refresh(Action::ATTACK, target, targetUnitId, {left, topLeft, bottomLeft}));
	ASSERT_TRUE(selection.cycle(true));
	ASSERT_EQ(selection.getCandidate(), topLeft);
	repeat.press(false, repeatContext(selection));

	ASSERT_TRUE(selection.refresh(Action::ATTACK, target, targetUnitId, {left, bottomLeft}));
	ASSERT_EQ(selection.getCandidate(), left);
	EXPECT_FALSE(repeat.retainContext(repeatContext(selection)));
	EXPECT_FALSE(repeat.hasPendingRepeat());
	EXPECT_EQ(repeat.update(1000), std::nullopt);
}

TEST(BattleMeleeSelectionTest, ReplacementTargetInvalidatesHeldRepeatContext)
{
	BattleMeleeSelection selection;
	BattleControllerMeleeOriginRepeatState repeat;
	ASSERT_TRUE(selection.refresh(Action::ATTACK, target, targetUnitId, {left, topLeft}));
	repeat.press(true, repeatContext(selection));

	ASSERT_TRUE(selection.refresh(Action::ATTACK, BattleHex(10, 5), targetUnitId + 1, {left, bottomLeft}));
	EXPECT_FALSE(repeat.retainContext(repeatContext(selection)));
	EXPECT_FALSE(repeat.hasPendingRepeat());
	EXPECT_EQ(repeat.update(1000), std::nullopt);
}
