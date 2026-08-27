/*
 * BattleControllerActionRadialTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "../client/StdInc.h"

#include "../client/battle/BattleControllerActionRadialState.h"
#include "../client/battle/BattleControllerPromptGlyph.h"

#include <gtest/gtest.h>

namespace
{
using Action = BattleControllerActionRadialAction;
using Entry = BattleControllerActionRadialEntry;

const std::vector<Entry> ACTIONS = {
	{Action::WAIT, true, 6},
	{Action::DEFEND, false, 2},
	{Action::AUTOCOMBAT, true, 4}
};
}

TEST(BattleControllerActionRadialStateTest, UsesEightFixedDirectionSectors)
{
	const std::array<std::pair<double, double>, 8> directions = {{
		{0.0, -1.0},
		{1.0, -1.0},
		{1.0, 0.0},
		{1.0, 1.0},
		{0.0, 1.0},
		{-1.0, 1.0},
		{-1.0, 0.0},
		{-1.0, -1.0}
	}};

	for(size_t slot = 0; slot < directions.size(); ++slot)
	{
		BattleControllerActionRadialState state;
		state.open({{Action::WAIT, true, slot}});
		EXPECT_TRUE(state.selectDirection(directions[slot].first, directions[slot].second));
		EXPECT_EQ(state.selectedAction(), Action::WAIT);
	}

	BattleControllerActionRadialState state;
	state.open({{Action::WAIT, true, 0}});
	EXPECT_FALSE(state.selectDirection(0.2, 0.2));
	EXPECT_EQ(state.selectedAction(), std::nullopt);
}

TEST(BattleControllerActionRadialStateTest, EmptyDirectionClearsSelectionAndPendingConfirm)
{
	BattleControllerActionRadialState state;
	state.open(ACTIONS);
	ASSERT_TRUE(state.selectDirection(-1.0, 0.0));
	ASSERT_TRUE(state.pressConfirm());

	EXPECT_TRUE(state.selectDirection(1.0, -1.0));
	EXPECT_EQ(state.selectedAction(), std::nullopt);
	EXPECT_EQ(state.releaseConfirm(ACTIONS), std::nullopt);
	EXPECT_TRUE(state.isOpen());
}

TEST(BattleControllerActionRadialStateTest, DisabledActionCanBeSelectedButCannotCommit)
{
	BattleControllerActionRadialState state;
	state.open(ACTIONS);
	ASSERT_TRUE(state.selectDirection(1.0, 0.0));
	ASSERT_TRUE(state.pressConfirm());

	EXPECT_EQ(state.selectedAction(), Action::DEFEND);
	EXPECT_EQ(state.releaseConfirm(ACTIONS), std::nullopt);
	EXPECT_TRUE(state.isOpen());
}

TEST(BattleControllerActionRadialStateTest, ConfirmRevalidatesActionIdentityAndAvailability)
{
	BattleControllerActionRadialState state;
	state.open(ACTIONS);
	ASSERT_TRUE(state.selectDirection(-1.0, 0.0));
	ASSERT_TRUE(state.pressConfirm());

	const std::vector<Entry> reordered = {
		{Action::AUTOCOMBAT, true, 4},
		{Action::WAIT, true, 6}
	};
	EXPECT_EQ(state.releaseConfirm(reordered), Action::WAIT);
	EXPECT_FALSE(state.isOpen());

	state.open(ACTIONS);
	ASSERT_TRUE(state.selectDirection(-1.0, 0.0));
	ASSERT_TRUE(state.pressConfirm());
	EXPECT_EQ(state.releaseConfirm({{Action::AUTOCOMBAT, true, 4}}), std::nullopt);
	EXPECT_EQ(state.selectedAction(), std::nullopt);
	EXPECT_TRUE(state.isOpen());
}

TEST(BattleControllerActionRadialStateTest, SelectionChangeCannotRetargetPressedConfirm)
{
	BattleControllerActionRadialState state;
	state.open(ACTIONS);
	ASSERT_TRUE(state.selectDirection(-1.0, 0.0));
	ASSERT_TRUE(state.pressConfirm());

	ASSERT_TRUE(state.selectDirection(0.0, 1.0));
	EXPECT_EQ(state.releaseConfirm(ACTIONS), std::nullopt);
	EXPECT_EQ(state.selectedAction(), Action::AUTOCOMBAT);
	EXPECT_TRUE(state.isOpen());
}

TEST(BattleControllerActionRadialStateTest, BackAndResetNeverCommit)
{
	BattleControllerActionRadialState state;
	state.open(ACTIONS);
	ASSERT_TRUE(state.selectDirection(0.0, 1.0));
	EXPECT_TRUE(state.back());
	EXPECT_FALSE(state.isOpen());
	state.open(ACTIONS);
	ASSERT_TRUE(state.selectDirection(-1.0, 0.0));
	ASSERT_TRUE(state.pressConfirm());
	state.reset();
	EXPECT_FALSE(state.isOpen());
	EXPECT_EQ(state.releaseConfirm(ACTIONS), std::nullopt);
}

TEST(BattleControllerPromptGlyphTest, PreservesBattleNativeFamilyPresentation)
{
	const auto xbox = BattleControllerPromptGlyph::resolve(
		{"a"}, ControllerPrompt::Family::XBOX, false);
	EXPECT_EQ(xbox.spritePath, "controllerActionBar/generic-face-normal.png");
	EXPECT_EQ(xbox.runtimeLabel, "A");

	const auto playStation = BattleControllerPromptGlyph::resolve(
		{"a"}, ControllerPrompt::Family::PLAYSTATION, false);
	EXPECT_EQ(playStation.spritePath, "controllerActionBar/playstation-a-normal.png");
	EXPECT_TRUE(playStation.runtimeLabel.empty());

	const auto nintendo = BattleControllerPromptGlyph::resolve(
		{"a"}, ControllerPrompt::Family::NINTENDO, false);
	EXPECT_EQ(nintendo.spritePath, "controllerActionBar/generic-face-normal.png");
	EXPECT_EQ(nintendo.runtimeLabel, "B");
}

TEST(BattleControllerPromptGlyphTest, TriggerLabelsFollowControllerFamily)
{
	EXPECT_EQ(BattleControllerPromptGlyph::bindingLabel(
		"righttrigger", ControllerPrompt::Family::PLAYSTATION), "R2");
	EXPECT_EQ(BattleControllerPromptGlyph::bindingLabel(
		"righttrigger", ControllerPrompt::Family::XBOX), "RT");
	EXPECT_EQ(BattleControllerPromptGlyph::bindingLabel(
		"righttrigger", ControllerPrompt::Family::NINTENDO), "ZR");
}
