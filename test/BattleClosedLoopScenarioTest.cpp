/*
 * BattleClosedLoopScenarioTest.cpp, part of VCMI engine
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
#include "../client/battle/BattleMovementPreview.h"
#include "../client/battle/BattleRangedShooting.h"

#include "../lib/battle/BattleHex.h"

#include <gtest/gtest.h>

using State = BattleControllerStateMachine::State;

/// Drives one accept press through the three decision layers in the same
/// priority order as BattleInterface::handleControllerAccept: melee beats
/// shooting, shooting beats movement. Returns the layer that consumed A.
enum class AcceptConsumer
{
	NONE,
	MELEE,
	SHOOTING,
	MOVEMENT
};

struct AcceptContext
{
	bool attackable = false;
	bool shootable = false;
	bool focusedReachable = false;
};

static AcceptConsumer dispatchAccept(BattleControllerStateMachine & states, const AcceptContext & ctx)
{
	switch(BattleAttackDirection::decideAccept(states.top(), ctx.attackable))
	{
		case BattleAttackDirection::MeleeOutcome::START_ACTION:
			EXPECT_TRUE(states.enter(State::ACTION));
			return AcceptConsumer::MELEE;
		case BattleAttackDirection::MeleeOutcome::OPEN_DIRECTION:
			EXPECT_TRUE(states.enter(State::ATTACK_DIRECTION));
			return AcceptConsumer::MELEE;
		case BattleAttackDirection::MeleeOutcome::COMMIT:
			states.reset();
			return AcceptConsumer::MELEE;
		case BattleAttackDirection::MeleeOutcome::CANCEL_LAYER:
			EXPECT_TRUE(states.cancel());
			return AcceptConsumer::MELEE;
		case BattleAttackDirection::MeleeOutcome::NONE:
			break;
	}

	switch(BattleRangedShooting::decideAccept(states.top(), ctx.shootable))
	{
		case BattleRangedShooting::Outcome::START_ACTION:
			EXPECT_TRUE(states.enter(State::ACTION));
			return AcceptConsumer::SHOOTING;
		case BattleRangedShooting::Outcome::COMMIT:
			states.reset();
			return AcceptConsumer::SHOOTING;
		case BattleRangedShooting::Outcome::CANCEL_LAYER:
			EXPECT_TRUE(states.cancel());
			return AcceptConsumer::SHOOTING;
		case BattleRangedShooting::Outcome::NONE:
			break;
	}

	switch(BattleMovementPreview::decideAccept(states.top(), ctx.focusedReachable))
	{
		case BattleMovementPreview::Outcome::START_PREVIEW:
			EXPECT_TRUE(states.enter(State::PREVIEW));
			return AcceptConsumer::MOVEMENT;
		case BattleMovementPreview::Outcome::COMMIT:
			states.reset();
			return AcceptConsumer::MOVEMENT;
		case BattleMovementPreview::Outcome::CANCEL_PREVIEW:
			EXPECT_TRUE(states.cancel());
			return AcceptConsumer::MOVEMENT;
		case BattleMovementPreview::Outcome::NONE:
			break;
	}

	return AcceptConsumer::NONE;
}

TEST(BattleClosedLoopScenarioTest, MoveMeleeShootLoopCompletesWithoutPointer)
{
	BattleControllerStateMachine states;

	// Phase 1 - movement: the focus sits on a reachable empty hex
	EXPECT_EQ(states.top(), State::BROWSE);
	AcceptContext moveCtx;
	moveCtx.focusedReachable = true;
	EXPECT_EQ(dispatchAccept(states, moveCtx), AcceptConsumer::MOVEMENT);
	EXPECT_EQ(states.top(), State::BROWSE); // committed move resets the stack

	// Phase 2 - melee: a new turn, the focus sits on an adjacent enemy
	AcceptContext meleeCtx;
	meleeCtx.attackable = true;
	EXPECT_EQ(dispatchAccept(states, meleeCtx), AcceptConsumer::MELEE);
	EXPECT_EQ(states.top(), State::BROWSE); // committed attack resets the stack

	// Shoulder fine-tuning cycles the visible approach hex without moving focus
	const std::vector<BattleHex> origins = {BattleHex(7, 5), BattleHex(8, 4)};
	const BattleHex recommended = BattleAttackDirection::recommend(origins);
	EXPECT_EQ(recommended, origins.front());
	const BattleHex cycled = BattleAttackDirection::cycle(origins, recommended, true);
	EXPECT_EQ(cycled, origins.back());

	// Phase 3 - shooting: the focus sits on an in-range enemy with ammo
	AcceptContext shootCtx;
	shootCtx.shootable = true;
	EXPECT_EQ(dispatchAccept(states, shootCtx), AcceptConsumer::SHOOTING);
	EXPECT_EQ(states.top(), State::BROWSE); // committed shot resets the stack
}

TEST(BattleClosedLoopScenarioTest, priorityStaysMeleeOverShootingOverMovement)
{
	BattleControllerStateMachine states;

	// an attackable enemy is never a movement destination, but the dispatch
	// must still pick melee when both flags happen to be set at once
	AcceptContext contested;
	contested.attackable = true;
	contested.shootable = true;
	contested.focusedReachable = true;
	EXPECT_EQ(dispatchAccept(states, contested), AcceptConsumer::MELEE);
	EXPECT_EQ(states.top(), State::BROWSE);

	EXPECT_EQ(states.top(), State::BROWSE);
	contested.attackable = false;
	EXPECT_EQ(dispatchAccept(states, contested), AcceptConsumer::SHOOTING);
	EXPECT_EQ(states.top(), State::BROWSE);

	EXPECT_EQ(states.top(), State::BROWSE);
	contested.shootable = false;
	EXPECT_EQ(dispatchAccept(states, contested), AcceptConsumer::MOVEMENT);
	EXPECT_EQ(states.top(), State::BROWSE);
}

TEST(BattleClosedLoopScenarioTest, disabledTargetHasNoCommitAndIdleBCanOpenParent)
{
	BattleControllerStateMachine states;

	// A legal shot commits immediately and never leaves an open layer.
	AcceptContext shootCtx;
	shootCtx.shootable = true;
	ASSERT_EQ(dispatchAccept(states, shootCtx), AcceptConsumer::SHOOTING);
	EXPECT_EQ(states.top(), State::BROWSE);

	// Once disabled, the target remains focusable but A has no action.
	EXPECT_EQ(BattleRangedShooting::classify(true, false, false, true),
		BattleRangedShooting::DisabledReason::NO_AMMO);
	shootCtx.shootable = false;
	EXPECT_EQ(dispatchAccept(states, shootCtx), AcceptConsumer::NONE);
	EXPECT_EQ(states.top(), State::BROWSE);

	// idle B stays in browse; the caller then returns to the parent layer
	EXPECT_FALSE(states.cancel());
	EXPECT_EQ(BattleControllerStateMachine::decideCancel(true, false, false),
		BattleControllerStateMachine::CancelDecision::OPEN_PARENT_LAYER);
}
