/*
 * BattleHintBarTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../client/StdInc.h"

#include "../client/battle/BattleHintBar.h"
#include "../client/battle/BattleHintBarPresenter.h"

#include <gtest/gtest.h>

using State = BattleControllerStateMachine::State;
using Reason = BattleRangedShooting::DisabledReason;

static std::vector<std::pair<EShortcut, std::string>> simplify(const std::vector<BattleHintEntry> & entries)
{
	std::vector<std::pair<EShortcut, std::string>> result;
	for(const auto & entry : entries)
		result.emplace_back(entry.glyph, entry.textKey);
	return result;
}

TEST(BattleHintBarTest, pointerModesHideTheHintBar)
{
	BattleHintBar::Context ctx;
	ctx.attackable = true;

	EXPECT_TRUE(BattleHintBar::entries(InputMode::KEYBOARD_AND_MOUSE, State::BROWSE, ctx).empty());
	EXPECT_TRUE(BattleHintBar::entries(InputMode::TOUCH, State::BROWSE, ctx).empty());
}

TEST(BattleHintBarTest, browseAdvertisesTheContextualAcceptAction)
{
	BattleHintBar::Context ctx;

	// empty-hex focus: move, plus back and the D7 LB/RB switch prompts
	ctx.focusedReachable = true;
	EXPECT_EQ(simplify(BattleHintBar::entries(InputMode::CONTROLLER, State::BROWSE, ctx)),
		(std::vector<std::pair<EShortcut, std::string>>{
			{EShortcut::GLOBAL_ACCEPT, "vcmi.battleWindow.hints.move"},
			{EShortcut::GLOBAL_CANCEL, "vcmi.battleWindow.hints.back"},
			{EShortcut::BATTLE_WAIT, "vcmi.battleWindow.hints.switchUnit"},
			{EShortcut::BATTLE_DEFEND, "vcmi.battleWindow.hints.switchUnit"}}));

	// melee target beats shooting and movement
	ctx = {};
	ctx.attackable = true;
	ctx.shootable = true;
	ctx.focusedReachable = true;
	EXPECT_EQ(simplify(BattleHintBar::entries(InputMode::CONTROLLER, State::BROWSE, ctx)),
		(std::vector<std::pair<EShortcut, std::string>>{
			{EShortcut::GLOBAL_ACCEPT, "vcmi.battleWindow.hints.attack"},
			{EShortcut::GLOBAL_CANCEL, "vcmi.battleWindow.hints.back"},
			{EShortcut::BATTLE_WAIT, "vcmi.battleWindow.hints.switchUnit"},
			{EShortcut::BATTLE_DEFEND, "vcmi.battleWindow.hints.switchUnit"}}));

	// shooting target without melee candidates
	ctx = {};
	ctx.shootable = true;
	EXPECT_EQ(simplify(BattleHintBar::entries(InputMode::CONTROLLER, State::BROWSE, ctx)).front(),
		(std::pair<EShortcut, std::string>{EShortcut::GLOBAL_ACCEPT, "vcmi.battleWindow.hints.shoot"}));
}

TEST(BattleHintBarTest, browseShowsTheBT04ReasonForDisabledTargets)
{
	BattleHintBar::Context ctx;
	ctx.shootingDisabled = Reason::NO_AMMO;

	// the target stays focusable but not commitable; the bar explains why
	EXPECT_EQ(simplify(BattleHintBar::entries(InputMode::CONTROLLER, State::BROWSE, ctx)).front(),
		(std::pair<EShortcut, std::string>{EShortcut::NONE, "vcmi.battleWindow.hints.reason.noAmmo"}));

	ctx.shootingDisabled = Reason::BLOCKED_LINE;
	EXPECT_EQ(simplify(BattleHintBar::entries(InputMode::CONTROLLER, State::BROWSE, ctx)).front(),
		(std::pair<EShortcut, std::string>{EShortcut::NONE, "vcmi.battleWindow.hints.reason.blockedLine"}));

	ctx.shootingDisabled = Reason::OUT_OF_RANGE;
	EXPECT_EQ(simplify(BattleHintBar::entries(InputMode::CONTROLLER, State::BROWSE, ctx)).front(),
		(std::pair<EShortcut, std::string>{EShortcut::NONE, "vcmi.battleWindow.hints.reason.outOfRange"}));
}

TEST(BattleHintBarTest, previewAndMeleeLayersAdvertiseTheirWalk)
{
	BattleHintBar::Context ctx;

	EXPECT_EQ(simplify(BattleHintBar::entries(InputMode::CONTROLLER, State::PREVIEW, ctx)),
		(std::vector<std::pair<EShortcut, std::string>>{
			{EShortcut::GLOBAL_ACCEPT, "vcmi.battleWindow.hints.move"},
			{EShortcut::GLOBAL_CANCEL, "vcmi.battleWindow.hints.back"}}));

	ctx.attackable = true;
	EXPECT_EQ(simplify(BattleHintBar::entries(InputMode::CONTROLLER, State::ACTION, ctx)).front(),
		(std::pair<EShortcut, std::string>{EShortcut::GLOBAL_ACCEPT, "vcmi.battleWindow.hints.aim"}));

	EXPECT_EQ(simplify(BattleHintBar::entries(InputMode::CONTROLLER, State::ATTACK_DIRECTION, ctx)),
		(std::vector<std::pair<EShortcut, std::string>>{
			{EShortcut::GLOBAL_ACCEPT, "vcmi.battleWindow.hints.attack"},
			{EShortcut::MOVE_LEFT, "vcmi.battleWindow.hints.adjust"},
			{EShortcut::MOVE_RIGHT, "vcmi.battleWindow.hints.adjust"},
			{EShortcut::GLOBAL_CANCEL, "vcmi.battleWindow.hints.back"}}));
}

TEST(BattleHintBarTest, shootingActionCommitsAndDeepLayersStayQuiet)
{
	BattleHintBar::Context ctx;
	ctx.shootable = true;

	EXPECT_EQ(simplify(BattleHintBar::entries(InputMode::CONTROLLER, State::ACTION, ctx)).front(),
		(std::pair<EShortcut, std::string>{EShortcut::GLOBAL_ACCEPT, "vcmi.battleWindow.hints.shoot"}));

	// spell targeting and commit animation get no bar; D6 keeps spell
	// entries out of M3-1
	ctx = {};
	EXPECT_TRUE(BattleHintBar::entries(InputMode::CONTROLLER, State::TARGET, ctx).empty());
	EXPECT_TRUE(BattleHintBar::entries(InputMode::CONTROLLER, State::COMMIT, ctx).empty());
}

TEST(BattleHintBarTest, switchPromptsOnlyAppearWhileBrowsing)
{
	BattleHintBar::Context ctx;
	ctx.focusedReachable = true;

	for(auto state : {State::ACTION, State::PREVIEW, State::ATTACK_DIRECTION})
	{
		for(const auto & entry : BattleHintBar::entries(InputMode::CONTROLLER, state, ctx))
		{
			EXPECT_NE(entry.glyph, EShortcut::BATTLE_WAIT);
			EXPECT_NE(entry.glyph, EShortcut::BATTLE_DEFEND);
		}
	}
}

TEST(BattleHintBarTest, actionPromptStaysInsideTheUnobscuredBattlefield)
{
	const Rect safeBattlefield(79, 86, 642, 469);
	const Rect rightEdgeHex(674, 296, 44, 42);
	const Rect topEdgeHex(80, 86, 44, 42);

	const Rect rightEdgePrompt = BattleHintBarPresenter::actionPromptRect(rightEdgeHex, safeBattlefield);
	EXPECT_EQ(rightEdgePrompt.x, 583);
	EXPECT_EQ(rightEdgePrompt.y, 266);
	EXPECT_EQ(rightEdgePrompt.w, 138);
	EXPECT_LE(rightEdgePrompt.x + rightEdgePrompt.w, safeBattlefield.x + safeBattlefield.w);

	const Rect topEdgePrompt = BattleHintBarPresenter::actionPromptRect(topEdgeHex, safeBattlefield);
	EXPECT_EQ(topEdgePrompt.y, 131);
	EXPECT_GE(topEdgePrompt.y, safeBattlefield.y);
}
