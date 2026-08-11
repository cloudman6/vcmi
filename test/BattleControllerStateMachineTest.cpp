/*
 * BattleControllerStateMachineTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../client/StdInc.h"

#include "../client/battle/BattleControllerStateMachine.h"

#include <gtest/gtest.h>

using State = BattleControllerStateMachine::State;

TEST(BattleControllerStateMachineTest, startsInBrowse)
{
	BattleControllerStateMachine machine;
	EXPECT_EQ(machine.top(), State::BROWSE);
	EXPECT_EQ(machine.depth(), 1);
}

TEST(BattleControllerStateMachineTest, moveFlowPushesPreviewFromBrowse)
{
	BattleControllerStateMachine machine;
	ASSERT_TRUE(machine.enter(State::PREVIEW));
	EXPECT_EQ(machine.top(), State::PREVIEW);
	EXPECT_EQ(machine.depth(), 2);

	ASSERT_TRUE(machine.enter(State::COMMIT));
	EXPECT_EQ(machine.top(), State::COMMIT);
	EXPECT_EQ(machine.depth(), 3);
}

TEST(BattleControllerStateMachineTest, meleeFlowStacksActionAndAttackDirection)
{
	BattleControllerStateMachine machine;
	ASSERT_TRUE(machine.enter(State::ACTION));
	ASSERT_TRUE(machine.enter(State::ATTACK_DIRECTION));
	ASSERT_TRUE(machine.enter(State::COMMIT));
	EXPECT_EQ(machine.depth(), 4);
}

TEST(BattleControllerStateMachineTest, targetFlowReachesCommitThroughPreview)
{
	BattleControllerStateMachine machine;
	ASSERT_TRUE(machine.enter(State::ACTION));
	ASSERT_TRUE(machine.enter(State::TARGET));
	ASSERT_TRUE(machine.enter(State::PREVIEW));
	ASSERT_TRUE(machine.enter(State::COMMIT));
	EXPECT_EQ(machine.depth(), 5);
}

TEST(BattleControllerStateMachineTest, rejectsTransitionsOutsideTheContract)
{
	// Browse cannot jump straight to target selection, direction choice or commit;
	// melee direction stays behind Action so B can pop back onto Action.
	BattleControllerStateMachine machine;
	EXPECT_FALSE(machine.enter(State::TARGET));
	EXPECT_FALSE(machine.enter(State::ATTACK_DIRECTION));
	EXPECT_FALSE(machine.enter(State::COMMIT));
	EXPECT_FALSE(machine.enter(State::BROWSE));
	EXPECT_EQ(machine.top(), State::BROWSE);
	EXPECT_EQ(machine.depth(), 1);

	ASSERT_TRUE(machine.enter(State::ACTION));
	EXPECT_FALSE(machine.enter(State::COMMIT));
	EXPECT_FALSE(machine.enter(State::PREVIEW));
	EXPECT_FALSE(machine.enter(State::BROWSE));
	EXPECT_EQ(machine.top(), State::ACTION);

	ASSERT_TRUE(machine.enter(State::TARGET));
	EXPECT_FALSE(machine.enter(State::COMMIT));
	EXPECT_FALSE(machine.enter(State::ATTACK_DIRECTION));
	EXPECT_EQ(machine.top(), State::TARGET);

	ASSERT_TRUE(machine.enter(State::PREVIEW));
	EXPECT_FALSE(machine.enter(State::ATTACK_DIRECTION));
	EXPECT_EQ(machine.top(), State::PREVIEW);
}

TEST(BattleControllerStateMachineTest, cancelPopsOneLayerAtATime)
{
	BattleControllerStateMachine machine;
	ASSERT_TRUE(machine.enter(State::ACTION));
	ASSERT_TRUE(machine.enter(State::ATTACK_DIRECTION));

	// B exits the direction choice back onto the action layer
	EXPECT_TRUE(machine.cancel());
	EXPECT_EQ(machine.top(), State::ACTION);

	// B exits the action layer back to Browse
	EXPECT_TRUE(machine.cancel());
	EXPECT_EQ(machine.top(), State::BROWSE);
	EXPECT_EQ(machine.depth(), 1);

	// idle: nothing left to pop, the caller returns to the parent layer
	EXPECT_FALSE(machine.cancel());
	EXPECT_EQ(machine.top(), State::BROWSE);
	EXPECT_EQ(machine.depth(), 1);
}

TEST(BattleControllerStateMachineTest, cancelFromMovePreviewReturnsToBrowse)
{
	BattleControllerStateMachine machine;
	ASSERT_TRUE(machine.enter(State::PREVIEW));

	EXPECT_TRUE(machine.cancel());
	EXPECT_EQ(machine.top(), State::BROWSE);

	EXPECT_FALSE(machine.cancel());
}

TEST(BattleControllerStateMachineTest, resetReturnsToBrowse)
{
	BattleControllerStateMachine machine;
	ASSERT_TRUE(machine.enter(State::ACTION));
	ASSERT_TRUE(machine.enter(State::TARGET));
	ASSERT_TRUE(machine.enter(State::PREVIEW));

	machine.reset();

	EXPECT_EQ(machine.top(), State::BROWSE);
	EXPECT_EQ(machine.depth(), 1);
	ASSERT_TRUE(machine.enter(State::PREVIEW));
}

using Decision = BattleControllerStateMachine::CancelDecision;

TEST(BattleControllerStateMachineTest, cancelDecisionPopsLayerFirstInControllerMode)
{
	EXPECT_EQ(BattleControllerStateMachine::decideCancel(true, true, true), Decision::POP_LAYER);
	EXPECT_EQ(BattleControllerStateMachine::decideCancel(true, true, false), Decision::POP_LAYER);
}

TEST(BattleControllerStateMachineTest, cancelDecisionPreservesSpellCancelInEveryMode)
{
	// pre-existing GLOBAL_CANCEL contract: cancel always reaches the spell
	// cancel path, in keyboard/mouse mode unconditionally
	EXPECT_EQ(BattleControllerStateMachine::decideCancel(false, false, true), Decision::CANCEL_SPELL);
	EXPECT_EQ(BattleControllerStateMachine::decideCancel(false, false, false), Decision::CANCEL_SPELL);
	EXPECT_EQ(BattleControllerStateMachine::decideCancel(true, false, true), Decision::CANCEL_SPELL);
}

TEST(BattleControllerStateMachineTest, cancelDecisionOpensParentLayerOnlyWhenIdleInControllerMode)
{
	EXPECT_EQ(BattleControllerStateMachine::decideCancel(true, false, false), Decision::OPEN_PARENT_LAYER);
	EXPECT_EQ(BattleControllerStateMachine::decideCancel(false, true, false), Decision::CANCEL_SPELL);
}
