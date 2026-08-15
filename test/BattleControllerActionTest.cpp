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
