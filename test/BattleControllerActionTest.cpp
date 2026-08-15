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
	EXPECT_EQ(classifyBattleControllerPrimaryAction(Possible::ATTACK, true), Action::ATTACK);
	EXPECT_EQ(classifyBattleControllerPrimaryAction(Possible::WALK_AND_ATTACK, true), Action::ATTACK);
	EXPECT_EQ(classifyBattleControllerPrimaryAction(Possible::SHOOT, true), Action::SHOOT);
	EXPECT_EQ(classifyBattleControllerPrimaryAction(Possible::CREATURE_INFO, true), Action::INSPECT);
	EXPECT_EQ(classifyBattleControllerPrimaryAction(Possible::CREATURE_INFO, false), Action::NONE);
	EXPECT_EQ(classifyBattleControllerPrimaryAction(Possible::AIMED_SPELL_CREATURE, true), Action::NONE);
}
