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

#include "BattleControllerFocusHarness.h"
#include "BattleControllerFocusSpikeFixture.h"

#include <gtest/gtest.h>

namespace
{
class RecordingBattleActionsSink final : public BattleControllerFocusHarness::Sink
{
public:
	void preview(const BattleHex & hex) override
	{
		previewedHexes.push_back(hex);
	}

	void submit(const BattleHex & hex) override
	{
		submittedHexes.push_back(hex);
	}

	std::vector<BattleHex> previewedHexes;
	std::vector<BattleHex> submittedHexes;
};

class BattleControllerFocusSpikeTest : public testing::Test
{
protected:
	RecordingBattleActionsSink actions;
	BattleControllerFocusHarness focus{actions};
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

	EXPECT_EQ(actions.previewedHexes.size(), 13);
}

TEST_F(BattleControllerFocusSpikeTest, KeepsTargetFocusWhileSelectingEitherLegalMeleeApproachDirection)
{
	ASSERT_TRUE(focus.setFocus(BattleControllerFocusSpikeFixture::doubleWideHead));

	ASSERT_TRUE(focus.selectAttackDirection(BattleHex::EDir::TOP_LEFT));
	EXPECT_EQ(focus.attackDirection(), BattleHex::EDir::TOP_LEFT);
	EXPECT_EQ(focus.focusedHex(), BattleControllerFocusSpikeFixture::doubleWideHead);

	ASSERT_TRUE(focus.selectAttackDirection(BattleHex::EDir::BOTTOM_RIGHT));
	EXPECT_EQ(focus.attackDirection(), BattleHex::EDir::BOTTOM_RIGHT);
	EXPECT_EQ(focus.focusedHex(), BattleControllerFocusSpikeFixture::doubleWideHead);

	EXPECT_EQ(BattleControllerFocusSpikeFixture::primaryAttackFrom, BattleControllerFocusSpikeFixture::doubleWideHead.cloneInDirection(BattleHex::EDir::TOP_LEFT, false));
	EXPECT_EQ(BattleControllerFocusSpikeFixture::alternateAttackFrom, BattleControllerFocusSpikeFixture::doubleWideHead.cloneInDirection(BattleHex::EDir::BOTTOM_RIGHT, false));
	EXPECT_NE(BattleControllerFocusSpikeFixture::primaryAttackFrom, BattleControllerFocusSpikeFixture::alternateAttackFrom);
	EXPECT_EQ(BattleControllerFocusSpikeFixture::doubleWideTail, BattleControllerFocusSpikeFixture::doubleWideHead.cloneInDirection(BattleHex::EDir::RIGHT, false));
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
	const std::array<const char *, 3> commands = {"wait", "defend", "spell"};

	for(const auto * command : commands)
	{
		SCOPED_TRACE(command);
		ASSERT_TRUE(focus.setFocus(BattleControllerFocusSpikeFixture::initialFocus));
		ASSERT_TRUE(focus.selectAttackDirection(BattleHex::EDir::RIGHT));

		const auto previewsBeforeReply = actions.previewedHexes.size();
		focus.refreshAfterBattleReply();

		EXPECT_FALSE(focus.attackDirection().has_value());
		EXPECT_EQ(actions.previewedHexes.size(), previewsBeforeReply + 1);
		EXPECT_EQ(actions.previewedHexes.back(), BattleControllerFocusSpikeFixture::initialFocus);

		const auto submissionsBeforeConfirm = actions.submittedHexes.size();
		EXPECT_TRUE(focus.confirm());
		EXPECT_EQ(actions.submittedHexes.back(), BattleControllerFocusSpikeFixture::initialFocus);
		EXPECT_EQ(actions.submittedHexes.size(), submissionsBeforeConfirm + 1);

		EXPECT_FALSE(focus.setFocus(BattleHex::INVALID));
		EXPECT_FALSE(focus.confirm());
		EXPECT_EQ(actions.submittedHexes.size(), submissionsBeforeConfirm + 1);
	}
}

int main(int argc, char ** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
