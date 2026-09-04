/*
 * BattleControllerRadialStateTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "../client/StdInc.h"

#include "../client/battle/BattleControllerRadialState.h"
#include "../client/gui/Shortcut.h"

#include <gtest/gtest.h>

namespace
{
using Entry = BattleControllerRadialEntry;

const std::array<EShortcut, BattleControllerRadialState::SLOT_COUNT> SHORTCUTS = {
	EShortcut::BATTLE_WAIT,
	EShortcut::BATTLE_DEFEND,
	EShortcut::BATTLE_AUTOCOMBAT,
	EShortcut::BATTLE_CAST_SPELL,
	EShortcut::BATTLE_RETREAT,
	EShortcut::BATTLE_SURRENDER,
	EShortcut::BATTLE_TOGGLE_GRID,
	EShortcut::BATTLE_TOGGLE_MOUSE_SHADOW,
	EShortcut::BATTLE_TOGGLE_MOVEMENT_SHADOW,
	EShortcut::BATTLE_TOGGLE_STACK_INFO,
	EShortcut::BATTLE_TOGGLE_QUEUE,
	EShortcut::BATTLE_TOGGLE_HEROES_STATS
};

std::vector<Entry> entries(bool enabled = true)
{
	std::vector<Entry> result;
	for(size_t slot = 0; slot < SHORTCUTS.size(); ++slot)
		result.push_back({SHORTCUTS[slot], enabled, slot});
	return result;
}
}

TEST(BattleControllerRadialStateTest, SelectsTwelveCounterclockwiseSectorsWithoutASecondDeadZone)
{
	constexpr double PI = 3.14159265358979323846;
	for(size_t slot = 0; slot < SHORTCUTS.size(); ++slot)
	{
		BattleControllerRadialState state;
		state.open(entries());
		const double angle = -PI / 2.0 - static_cast<double>(slot) * 2.0 * PI / static_cast<double>(BattleControllerRadialState::SLOT_COUNT);
		EXPECT_TRUE(state.selectDirection(std::cos(angle) * 0.01, std::sin(angle) * 0.01));
		EXPECT_EQ(state.selectedItem(), BattleControllerRadialItemId(SHORTCUTS[slot]));
	}
}

TEST(BattleControllerRadialStateTest, NeutralOrEmptySectorClearsSelectionAndPendingConfirm)
{
	BattleControllerRadialState state;
	state.open({
		{EShortcut::BATTLE_WAIT, true, 9}
	});
	ASSERT_TRUE(state.selectDirection(1.0, 0.0));
	ASSERT_TRUE(state.pressConfirm({
		{EShortcut::BATTLE_WAIT, true, 9}
	}));

	EXPECT_TRUE(state.selectDirection(0.0, 0.0));
	EXPECT_EQ(state.selectedItem(), std::nullopt);
	EXPECT_EQ(
		state.releaseConfirm({
			{EShortcut::BATTLE_WAIT, true, 9}
		}),
		std::nullopt
	);

	ASSERT_TRUE(state.selectDirection(1.0, 0.0));
	ASSERT_TRUE(state.pressConfirm({
		{EShortcut::BATTLE_WAIT, true, 9}
	}));
	EXPECT_TRUE(state.selectDirection(-1.0, 0.0));
	EXPECT_EQ(state.selectedItem(), std::nullopt);
	EXPECT_EQ(
		state.releaseConfirm({
			{EShortcut::BATTLE_WAIT, true, 9}
		}),
		std::nullopt
	);
}

TEST(BattleControllerRadialStateTest, ConfirmReleaseRevalidatesSameEnabledShortcut)
{
	BattleControllerRadialState state;
	state.open({
		{EShortcut::BATTLE_WAIT, true, 9}
	});
	ASSERT_TRUE(state.selectDirection(1.0, 0.0));
	ASSERT_TRUE(state.pressConfirm({
		{EShortcut::BATTLE_WAIT, true, 9}
	}));

	EXPECT_EQ(
		state.releaseConfirm({
			{EShortcut::BATTLE_WAIT, false, 9}
		}),
		std::nullopt
	);
	ASSERT_TRUE(state.pressConfirm({
		{EShortcut::BATTLE_WAIT, true, 9}
	}));
	EXPECT_EQ(
		state.releaseConfirm({
			{EShortcut::BATTLE_DEFEND, true, 9}
		}),
		std::nullopt
	);
	EXPECT_EQ(state.selectedItem(), std::nullopt);
	ASSERT_TRUE(state.selectDirection(1.0, 0.0));
	ASSERT_TRUE(state.pressConfirm({
		{EShortcut::BATTLE_WAIT, true, 9}
	}));
	EXPECT_EQ(
		state.releaseConfirm({
			{EShortcut::BATTLE_WAIT, true, 9}
		}),
		BattleControllerRadialItemId(EShortcut::BATTLE_WAIT)
	);
	EXPECT_FALSE(state.selectDirection(1.0, 0.0));
}

TEST(BattleControllerRadialStateTest, SelectionChangeAndResetCannotRetargetAConfirm)
{
	BattleControllerRadialState state;
	state.open({
		{EShortcut::BATTLE_WAIT,   true, 9},
		{EShortcut::BATTLE_DEFEND, true, 3}
	});
	ASSERT_TRUE(state.selectDirection(1.0, 0.0));
	ASSERT_TRUE(state.pressConfirm({
		{EShortcut::BATTLE_WAIT,   true, 9},
		{EShortcut::BATTLE_DEFEND, true, 3}
	}));
	ASSERT_TRUE(state.selectDirection(-1.0, 0.0));
	EXPECT_EQ(
		state.releaseConfirm({
			{EShortcut::BATTLE_WAIT,   true, 9},
			{EShortcut::BATTLE_DEFEND, true, 3}
		}),
		std::nullopt
	);
	EXPECT_TRUE(state.pressConfirm({
		{EShortcut::BATTLE_WAIT,   true, 9},
		{EShortcut::BATTLE_DEFEND, true, 3}
	}));
	state.reset();
	EXPECT_EQ(
		state.releaseConfirm({
			{EShortcut::BATTLE_DEFEND, true, 3}
		}),
		std::nullopt
	);
	EXPECT_FALSE(state.selectDirection(-1.0, 0.0));
}

TEST(BattleControllerRadialStateTest, SelectionAndConfirmUseCurrentEntriesInsteadOfOpeningSnapshot)
{
	BattleControllerRadialState state;
	state.open({
		{EShortcut::BATTLE_WAIT, false, 9}
	});

	const std::vector<Entry> current = {
		{EShortcut::BATTLE_WAIT, true, 9}
	};
	ASSERT_TRUE(state.selectDirection(1.0, 0.0));
	EXPECT_EQ(state.selectedItem(), BattleControllerRadialItemId(EShortcut::BATTLE_WAIT));
	ASSERT_TRUE(state.pressConfirm(current));
	EXPECT_EQ(state.releaseConfirm(current), BattleControllerRadialItemId(EShortcut::BATTLE_WAIT));

	BattleControllerRadialState missing;
	missing.open({
		{EShortcut::BATTLE_WAIT, true, 9}
	});
	ASSERT_TRUE(missing.selectDirection(1.0, 0.0));
	EXPECT_FALSE(missing.pressConfirm({}));
	EXPECT_EQ(missing.selectedItem(), std::nullopt);
}

TEST(BattleControllerRadialStateTest, TracksOnlyRealPagesAndDoesNotWrap)
{
	BattleControllerRadialState state;
	state.open({
		{{EShortcut::BATTLE_CAST_SPELL, 10}, true, 0, 0},
		{{EShortcut::BATTLE_CAST_SPELL, 22}, true, 0, 1}
	});

	EXPECT_EQ(state.pageCount(), 2);
	EXPECT_EQ(state.currentPage(), 0);
	EXPECT_FALSE(state.changePage(-1));
	EXPECT_TRUE(state.changePage(1));
	EXPECT_EQ(state.currentPage(), 1);
	EXPECT_FALSE(state.changePage(1));
}

TEST(BattleControllerRadialStateTest, SpellIdentitySurvivesPagingAndAvailabilityRevalidation)
{
	const BattleControllerRadialItemId firstSpell{EShortcut::BATTLE_CAST_SPELL, 10};
	const BattleControllerRadialItemId secondSpell{EShortcut::BATTLE_CAST_SPELL, 22};
	const std::vector<Entry> spells = {
		{firstSpell,  true, 0, 0},
        {secondSpell, true, 0, 1}
	};

	BattleControllerRadialState state;
	state.open(spells);
	ASSERT_TRUE(state.changePage(1));
	ASSERT_TRUE(state.selectDirection(0.0, -1.0));
	EXPECT_EQ(state.selectedItem(), secondSpell);
	ASSERT_TRUE(state.pressConfirm(spells));

	const std::vector<Entry> nowDisabled = {
		{firstSpell,  true,  0, 0},
        {secondSpell, false, 0, 1}
	};
	EXPECT_EQ(state.releaseConfirm(nowDisabled), std::nullopt);
	EXPECT_TRUE(state.pressConfirm(spells));
	EXPECT_EQ(state.currentPage(), 1);
}

TEST(BattleControllerRadialStateTest, PageChangeClearsSelectionAndPendingConfirm)
{
	const std::vector<Entry> spells = {
		{{EShortcut::BATTLE_CAST_SPELL, 10}, true, 0, 0},
		{{EShortcut::BATTLE_CAST_SPELL, 22}, true, 0, 1}
	};

	BattleControllerRadialState state;
	state.open(spells);
	ASSERT_TRUE(state.selectDirection(0.0, -1.0));
	ASSERT_TRUE(state.pressConfirm(spells));
	ASSERT_TRUE(state.changePage(1));
	EXPECT_EQ(state.selectedItem(), std::nullopt);
	EXPECT_EQ(state.releaseConfirm(spells), std::nullopt);
}
