/*
 * AdventureMapControllerStateTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "../client/StdInc.h"

#include "../client/adventureMap/AdventureMapControllerState.h"
#include "../client/gui/ControllerNavigationState.h"

#include <gtest/gtest.h>

namespace
{
using Candidate = AdventureMapControllerObjectCandidate;
using Target = AdventureMapControllerTarget;

Candidate candidate(si32 id, Point center, int3 interactionTile)
{
	return {ObjectInstanceID(id), center, interactionTile};
}
}

TEST(AdventureMapControllerObjectSelectorTest, AlignmentOutranksDistanceAndStableIdentityBreaksTies)
{
	const std::vector<Candidate> candidates{
		candidate(9, Point(2, 1), int3(2, 1, 0)),
		candidate(8, Point(4, 0), int3(4, 0, 0)),
		candidate(7, Point(4, 0), int3(4, 0, 0))};

	const auto selected = AdventureMapControllerObjectSelector::select(
		candidates, Point(0, 0), std::nullopt, 1.0, 0.0);
	ASSERT_TRUE(selected);
	EXPECT_EQ(selected->id, ObjectInstanceID(7));
}

TEST(AdventureMapControllerObjectSelectorTest, RejectsCurrentNonForwardAndInvalidDirections)
{
	const std::vector<Candidate> candidates{
		candidate(1, Point(0, 0), int3(0, 0, 0)),
		candidate(2, Point(-4, 0), int3(-4, 0, 0))};

	EXPECT_FALSE(AdventureMapControllerObjectSelector::select(
		candidates, Point(0, 0), ObjectInstanceID(1), 1.0, 0.0));
	EXPECT_FALSE(AdventureMapControllerObjectSelector::select(
		candidates, Point(0, 0), std::nullopt, 0.0, 0.0));
	EXPECT_FALSE(AdventureMapControllerObjectSelector::select(
		candidates, Point(0, 0), std::nullopt,
		std::numeric_limits<double>::quiet_NaN(), 1.0));
}

TEST(AdventureMapControllerStateTest, PrimaryReleaseRequiresTheExactPressedIdentity)
{
	AdventureMapControllerState state;
	const Target town{int3(8, 5, 0), {int3(9, 5, 0), ObjectInstanceID(12)}};
	const Target overlappingHero{int3(8, 5, 0), {int3(8, 5, 0), ObjectInstanceID(13)}};

	state.setTarget(town);
	ASSERT_TRUE(state.pressPrimary());
	state.setTarget(overlappingHero);
	EXPECT_FALSE(state.releasePrimary(overlappingHero));

	state.setTarget(town);
	ASSERT_TRUE(state.pressPrimary());
	EXPECT_EQ(state.releasePrimary(town), town);
}

TEST(AdventureMapControllerStateTest, ResetCancelsPendingPrimaryWithoutDiscardingSceneFocus)
{
	AdventureMapControllerState state;
	const Target tile{int3(4, 3, 0), {int3(4, 3, 0), std::nullopt}};

	state.setTarget(tile);
	ASSERT_TRUE(state.pressPrimary());
	state.resetInput();

	EXPECT_EQ(state.target(), tile);
	EXPECT_FALSE(state.releasePrimary(tile));
}

TEST(AdventureMapControllerStateTest, InvalidatedObjectCannotCommitAtItsFormerTile)
{
	AdventureMapControllerState state;
	const Target object{int3(5, 5, 0), {int3(6, 5, 0), ObjectInstanceID(21)}};
	const Target replacementTile{int3(5, 5, 0), {int3(5, 5, 0), std::nullopt}};

	state.setTarget(object);
	ASSERT_TRUE(state.pressPrimary());
	EXPECT_FALSE(state.releasePrimary(replacementTile));
}

TEST(AdventureMapControllerModeStateTest, CursorAndCameraModesAreMutuallyExclusive)
{
	AdventureMapControllerModeState state;
	state.setCameraHeld(true);
	state.updateCameraAxis(true, 0.75);
	state.updateCameraAxis(false, -0.5);
	ASSERT_TRUE(state.cameraHeld());
	EXPECT_EQ(state.cameraDirection(), std::pair(0.75, -0.5));

	state.toggleCursorMode();
	EXPECT_TRUE(state.cursorMode());
	EXPECT_FALSE(state.cameraHeld());
	EXPECT_EQ(state.cameraDirection(), std::pair(0.0, 0.0));

	state.setCameraHeld(true);
	EXPECT_FALSE(state.cameraHeld());
}

TEST(AdventureMapControllerModeStateTest, ResetReleasesCameraWithoutChangingCursorChoice)
{
	AdventureMapControllerModeState state;
	state.toggleCursorMode();
	state.resetInput();
	EXPECT_TRUE(state.cursorMode());

	state.toggleCursorMode();
	state.setCameraHeld(true);
	state.updateCameraAxis(true, 1.0);
	state.resetInput();
	EXPECT_FALSE(state.cursorMode());
	EXPECT_FALSE(state.cameraHeld());
	EXPECT_EQ(state.cameraDirection(), std::pair(0.0, 0.0));
}

TEST(ControllerNavigationStateTest, SettlesRepeatsAndStopsOnlyAfterBothComponentsAreNeutral)
{
	ControllerNavigationState state;
	state.update(true, 1.0);
	state.update(false, 0.5);
	EXPECT_FALSE(state.ready(15));
	EXPECT_TRUE(state.ready(1));
	EXPECT_FALSE(state.ready(319));
	EXPECT_TRUE(state.ready(1));
	EXPECT_FALSE(state.ready(109));
	EXPECT_TRUE(state.ready(1));

	state.update(true, 0.0);
	EXPECT_TRUE(state.isActive());
	EXPECT_EQ(state.direction(), std::pair(0.0, 0.5));
	state.update(false, 0.0);
	EXPECT_FALSE(state.isActive());
}

TEST(ControllerNavigationStateTest, MaterialDirectionChangeRequiresASettleBeforeMovingAgain)
{
	ControllerNavigationState state;
	state.update(true, 1.0);
	ASSERT_TRUE(state.ready(16));

	state.update(true, 0.0);
	state.update(false, 1.0);
	EXPECT_FALSE(state.ready(15));
	EXPECT_TRUE(state.ready(1));
	EXPECT_EQ(state.direction(), std::pair(0.0, 1.0));
}
