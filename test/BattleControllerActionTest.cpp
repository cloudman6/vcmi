/*
 * BattleControllerActionTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "../client/StdInc.h"

#include "../client/battle/BattleControllerAction.h"

#include <gtest/gtest.h>

TEST(BattleControllerActionTest, MapsOnlyLegalBattleActionsToControllerPrimaryActions)
{
	using Action = BattleControllerPrimaryAction;
	using Possible = PossiblePlayerBattleAction;

	EXPECT_EQ(classifyBattleControllerPrimaryAction(Possible::MOVE_STACK, true), Action::MOVE);
	EXPECT_EQ(classifyBattleControllerPrimaryAction(Possible::MOVE_TACTICS, true), Action::NONE);
	EXPECT_EQ(classifyBattleControllerPrimaryAction(Possible::ATTACK, true), Action::ATTACK);
	EXPECT_EQ(classifyBattleControllerPrimaryAction(Possible::WALK_AND_ATTACK, true), Action::ATTACK);
	EXPECT_EQ(classifyBattleControllerPrimaryAction(Possible::SHOOT, true), Action::SHOOT);
	EXPECT_EQ(classifyBattleControllerPrimaryAction(Possible::CREATURE_INFO, true), Action::INSPECT);
	EXPECT_EQ(classifyBattleControllerPrimaryAction(Possible::CREATURE_INFO, false), Action::NONE);
	EXPECT_EQ(classifyBattleControllerPrimaryAction(Possible::AIMED_SPELL_CREATURE, true), Action::NONE);
}

TEST(BattleControllerShootDisabledReasonTest, UsesCanonicalLegalityAndStableReasonPriority)
{
	using Reason = BattleControllerShootDisabledReason;

	EXPECT_EQ(classifyBattleControllerShootDisabledReason(false, false, false, true, true), Reason::NONE);
	EXPECT_EQ(classifyBattleControllerShootDisabledReason(true, true, false, true, true), Reason::NONE);
	EXPECT_EQ(classifyBattleControllerShootDisabledReason(true, false, false, true, true), Reason::NO_AMMO);
	EXPECT_EQ(classifyBattleControllerShootDisabledReason(true, false, true, true, true),
		Reason::BLOCKED_BY_ADJACENT_ENEMY);
	EXPECT_EQ(classifyBattleControllerShootDisabledReason(true, false, true, false, true), Reason::OUT_OF_RANGE);
	EXPECT_EQ(classifyBattleControllerShootDisabledReason(true, false, true, false, false), Reason::RULE_PROHIBITED);
}

TEST(BattleControllerActionPressStateTest, MatchingReleaseCommitsOnce)
{
	using Action = BattleControllerPrimaryAction;
	BattleControllerActionPressState state;
	const BattleHex focus(8, 5);

	EXPECT_TRUE(state.press(Action::MOVE, focus));
	EXPECT_TRUE(state.hasPendingAction());
	EXPECT_TRUE(state.isPressed(Action::MOVE, focus));
	EXPECT_EQ(state.release(Action::MOVE, focus), Action::MOVE);
	EXPECT_FALSE(state.hasPendingAction());
	EXPECT_EQ(state.release(Action::MOVE, focus), Action::NONE);
}

TEST(BattleControllerActionPressStateTest, HeldRepeatCannotRetargetCommit)
{
	using Action = BattleControllerPrimaryAction;
	BattleControllerActionPressState state;
	const BattleHex originalFocus(8, 5);
	const BattleHex changedFocus(9, 5);

	EXPECT_TRUE(state.press(Action::INSPECT, originalFocus));
	EXPECT_TRUE(state.press(Action::INSPECT, changedFocus));
	EXPECT_FALSE(state.isPressed(Action::INSPECT, changedFocus));
	EXPECT_EQ(state.release(Action::INSPECT, changedFocus), Action::NONE);
}

TEST(BattleControllerActionPressStateTest, ResetAndChangedActionCancelPendingCommit)
{
	using Action = BattleControllerPrimaryAction;
	BattleControllerActionPressState state;
	const BattleHex focus(8, 5);

	EXPECT_TRUE(state.press(Action::INSPECT, focus));
	EXPECT_EQ(state.release(Action::NONE, focus), Action::NONE);

	EXPECT_TRUE(state.press(Action::INSPECT, focus));
	state.reset();
	EXPECT_FALSE(state.isPressed(Action::INSPECT, focus));
	EXPECT_EQ(state.release(Action::INSPECT, focus), Action::NONE);
}

TEST(BattleControllerActionPressStateTest, ChangedMeleeOriginCancelsPendingCommit)
{
	using Action = BattleControllerPrimaryAction;
	BattleControllerActionPressState state;
	const BattleHex target(9, 5);
	const BattleHex leftOrigin(8, 5);
	const BattleHex upperOrigin(8, 4);

	EXPECT_TRUE(state.press(Action::ATTACK, target, leftOrigin, BattleHex::LEFT,
		PossiblePlayerBattleAction::ATTACK, 42));
	EXPECT_TRUE(state.isPressed(Action::ATTACK, target, leftOrigin, BattleHex::LEFT,
		PossiblePlayerBattleAction::ATTACK, 42));
	EXPECT_EQ(state.release(Action::ATTACK, target, upperOrigin, BattleHex::TOP_LEFT,
		PossiblePlayerBattleAction::ATTACK, 42), Action::NONE);
}

TEST(BattleControllerActionPressStateTest, ChangedMeleeSubtypeCancelsPendingCommit)
{
	using Action = BattleControllerPrimaryAction;
	BattleControllerActionPressState state;
	const BattleHex target(9, 5);
	const BattleHex origin(8, 5);

	EXPECT_TRUE(state.press(Action::ATTACK, target, origin, BattleHex::LEFT,
		PossiblePlayerBattleAction::ATTACK_AND_RETURN, 42));
	EXPECT_EQ(state.release(Action::ATTACK, target, origin, BattleHex::LEFT,
		PossiblePlayerBattleAction::ATTACK, 42), Action::NONE);
}

TEST(BattleControllerActionPressStateTest, ChangedMeleeTargetIdentityCancelsPendingCommit)
{
	using Action = BattleControllerPrimaryAction;
	BattleControllerActionPressState state;
	const BattleHex target(9, 5);
	const BattleHex origin(8, 5);

	EXPECT_TRUE(state.press(Action::ATTACK, target, origin, BattleHex::LEFT,
		PossiblePlayerBattleAction::ATTACK, 42));
	EXPECT_EQ(state.release(Action::ATTACK, target, origin, BattleHex::LEFT,
		PossiblePlayerBattleAction::ATTACK, 43), Action::NONE);
}

TEST(BattleControllerActionPressStateTest, ChangedShootTargetIdentityCancelsPendingCommit)
{
	using Action = BattleControllerPrimaryAction;
	BattleControllerActionPressState state;
	const BattleHex target(9, 5);

	EXPECT_TRUE(state.press(Action::SHOOT, target, BattleHex::INVALID, BattleHex::NONE,
		PossiblePlayerBattleAction::INVALID, 42));
	EXPECT_EQ(state.release(Action::SHOOT, target, BattleHex::INVALID, BattleHex::NONE,
		PossiblePlayerBattleAction::INVALID, 43), Action::NONE);
}

TEST(BattleControllerActionPressStateTest, IncompleteOrNonMeleeAttackIntentIsRejected)
{
	using Action = BattleControllerPrimaryAction;
	BattleControllerActionPressState state;
	const BattleHex target(9, 5);
	const BattleHex origin(8, 5);

	EXPECT_FALSE(state.press(Action::ATTACK, target, origin, BattleHex::LEFT,
		PossiblePlayerBattleAction::SHOOT, 42));
	EXPECT_FALSE(state.press(Action::ATTACK, target, origin, BattleHex::LEFT,
		PossiblePlayerBattleAction::ATTACK, std::nullopt));
}

TEST(BattleControllerMeleeOriginRepeatStateTest, RepeatsAfterInitialDelayAtFixedRate)
{
	BattleControllerMeleeOriginRepeatState state;
	const BattleControllerMeleeOriginRepeatContext context{
		PossiblePlayerBattleAction::ATTACK, BattleHex(9, 5), 42,
		BattleHex(8, 5), BattleHex::LEFT};
	state.press(false, context);

	EXPECT_EQ(state.update(319), std::nullopt);
	EXPECT_EQ(state.update(1), std::optional<bool>(false));
	EXPECT_EQ(state.update(109), std::nullopt);
	EXPECT_EQ(state.update(1), std::optional<bool>(false));
}

TEST(BattleControllerMeleeOriginRepeatStateTest, ReleaseResetAndDirectionChangeClearStaleRepeat)
{
	BattleControllerMeleeOriginRepeatState state;
	const BattleControllerMeleeOriginRepeatContext context{
		PossiblePlayerBattleAction::ATTACK, BattleHex(9, 5), 42,
		BattleHex(8, 5), BattleHex::LEFT};
	state.press(false, context);
	EXPECT_FALSE(state.release(true));
	EXPECT_EQ(state.update(320), std::optional<bool>(false));

	state.press(true, context);
	EXPECT_EQ(state.update(319), std::nullopt);
	EXPECT_TRUE(state.release(true));
	EXPECT_EQ(state.update(1000), std::nullopt);

	state.press(false, context);
	state.reset();
	EXPECT_EQ(state.update(1000), std::nullopt);
}

TEST(BattleControllerMeleeOriginRepeatStateTest, RejectsReplacementFocusAndSelectedOrigin)
{
	BattleControllerMeleeOriginRepeatState state;
	const BattleControllerMeleeOriginRepeatContext original{
		PossiblePlayerBattleAction::ATTACK, BattleHex(9, 5), 42,
		BattleHex(8, 5), BattleHex::LEFT};
	state.press(false, original);
	EXPECT_TRUE(state.retainContext(original));

	auto replacementTarget = original;
	replacementTarget.target = BattleHex(10, 5);
	replacementTarget.targetUnitId = 43;
	EXPECT_FALSE(state.retainContext(replacementTarget));
	EXPECT_FALSE(state.hasPendingRepeat());

	auto replacementOrigin = original;
	replacementOrigin.attackFrom = BattleHex(9, 4);
	replacementOrigin.direction = BattleHex::BOTTOM_LEFT;
	state.press(false, original);
	EXPECT_FALSE(state.retainContext(replacementOrigin));
	EXPECT_FALSE(state.hasPendingRepeat());
}

TEST(BattleControllerMeleeOriginRepeatStateTest, SuccessfulCycleAdvancesExpectedContext)
{
	BattleControllerMeleeOriginRepeatState state;
	const BattleControllerMeleeOriginRepeatContext original{
		PossiblePlayerBattleAction::ATTACK_AND_RETURN, BattleHex(9, 5), 42,
		BattleHex(8, 5), BattleHex::LEFT};
	auto advanced = original;
	advanced.attackFrom = BattleHex(9, 4);
	advanced.direction = BattleHex::BOTTOM_LEFT;

	state.press(true, original);
	state.selectionAdvanced(advanced);
	EXPECT_TRUE(state.retainContext(advanced));
	EXPECT_EQ(state.update(320), std::optional<bool>(true));
}
