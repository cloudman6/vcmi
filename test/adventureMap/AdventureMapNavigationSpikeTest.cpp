/*
 * AdventureMapNavigationSpikeTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "AdventureMapNavigationSpike.h"

#include <gtest/gtest.h>

#include <string_view>

namespace
{
using namespace adventureMapNavigationSpike;

class AdventureMapNavigationSpikeTest : public ::testing::Test
{
protected:
	MapBounds bounds{{0, 0, 0}, {7, 7, 0}};
	Viewport viewport{{2, 2, 0}, {4, 4, 0}};
	int previewQueries = 0;
	AdventureMapNavigationSpike spike{
		bounds,
		viewport,
		{3, 3, 0},
		EShortcut::ADVENTURE_MOVE_HERO,
		[this](const Tile & tile)
		{
			++previewQueries;
			return tile != Tile{0, 0, 0};
		}};

	SceneCandidate candidate(
		const std::string_view stableId,
		const Tile tile,
		const bool onScreen = true,
		const EShortcut shortcut = EShortcut::ADVENTURE_VISIT_OBJECT) const
	{
		return {std::string(stableId), tile, true, onScreen, shortcut};
	}
};

TEST_F(AdventureMapNavigationSpikeTest, objectLayerUsesDirectionBeforeDistanceAndStableIdForTies)
{
	const std::vector<SceneCandidate> candidates = {
		candidate("diagonal-close", {4, 4, 0}),
		candidate("direct-far", {6, 3, 0}),
		candidate("direct-beta", {5, 3, 0}),
		candidate("direct-alpha", {5, 3, 0})
	};

	const NavigationMove result = spike.move(Direction::East, candidates);

	EXPECT_EQ(result.outcome, NavigationOutcome::ObjectFocused);
	ASSERT_TRUE(result.target.objectId.has_value());
	EXPECT_EQ(*result.target.objectId, "direct-alpha");
	EXPECT_EQ(result.target.tile, (Tile{5, 3, 0}));
}

TEST_F(AdventureMapNavigationSpikeTest, tileCursorMovesOneTileAndRequestsPanAtViewportEdge)
{
	spike.setLayer(NavigationLayer::Tiles);
	const NavigationMove moveBeyondViewport = spike.move(Direction::East, {});

	EXPECT_EQ(moveBeyondViewport.outcome, NavigationOutcome::CameraPanRequested);
	EXPECT_EQ(moveBeyondViewport.target.tile, (Tile{4, 3, 0}));

	AdventureMapNavigationSpike edgeSpike(
		bounds,
		viewport,
		{7, 3, 0},
		EShortcut::ADVENTURE_MOVE_HERO,
		[](const Tile &) { return true; });
	edgeSpike.setLayer(NavigationLayer::Tiles);
	const NavigationMove atBoundary = edgeSpike.move(Direction::East, {});

	EXPECT_EQ(atBoundary.outcome, NavigationOutcome::MapBoundary);
	EXPECT_EQ(atBoundary.target.tile, (Tile{7, 3, 0}));
}

TEST_F(AdventureMapNavigationSpikeTest, previewAndCommitDelegateToExistingShortcutState)
{
	ASSERT_EQ(spike.move(Direction::East, {candidate("resource", {4, 3, 0})}).outcome, NavigationOutcome::ObjectFocused);
	EXPECT_TRUE(spike.requestPathPreview());
	EXPECT_EQ(previewQueries, 1);

	int commits = 0;
	const std::vector<AdventureMapShortcutState> disabled = {
		{EShortcut::ADVENTURE_VISIT_OBJECT, false, [&commits]() { ++commits; }}
	};
	EXPECT_FALSE(spike.commit(disabled));
	EXPECT_EQ(commits, 0);

	const std::vector<AdventureMapShortcutState> enabled = {
		{EShortcut::ADVENTURE_VISIT_OBJECT, true, [&commits]() { ++commits; }}
	};
	EXPECT_TRUE(spike.commit(enabled));
	EXPECT_EQ(commits, 1);
}

TEST_F(AdventureMapNavigationSpikeTest, unavailablePreviewNeverInvokesAnOtherwiseEnabledShortcut)
{
	AdventureMapNavigationSpike unavailablePreviewSpike(
		bounds,
		viewport,
		{3, 3, 0},
		EShortcut::ADVENTURE_MOVE_HERO,
		[](const Tile &) { return false; });
	ASSERT_EQ(
		unavailablePreviewSpike.move(Direction::East, {candidate("blocked", {4, 3, 0})}).outcome,
		NavigationOutcome::ObjectFocused);
	EXPECT_FALSE(unavailablePreviewSpike.requestPathPreview());

	int commits = 0;
	const std::vector<AdventureMapShortcutState> enabled = {
		{EShortcut::ADVENTURE_VISIT_OBJECT, true, [&commits]() { ++commits; }}
	};
	EXPECT_FALSE(unavailablePreviewSpike.commit(enabled));
	EXPECT_EQ(commits, 0);
}

TEST_F(AdventureMapNavigationSpikeTest, disappearedObjectFallsBackToItsTileAndInvalidatesPreview)
{
	ASSERT_EQ(spike.move(Direction::East, {candidate("resource", {4, 3, 0})}).outcome, NavigationOutcome::ObjectFocused);
	ASSERT_TRUE(spike.requestPathPreview());

	EXPECT_EQ(spike.refresh({}), RefreshOutcome::FellBackToTile);
	EXPECT_FALSE(spike.currentTarget().objectId.has_value());
	EXPECT_EQ(spike.currentTarget().tile, (Tile{4, 3, 0}));
	EXPECT_FALSE(spike.hasPathPreview());
}

TEST_F(AdventureMapNavigationSpikeTest, objectLayerDoesNotSilentlyChangeTargetWhenNoCandidateFacesDirection)
{
	ASSERT_EQ(spike.move(Direction::East, {candidate("resource", {4, 3, 0})}).outcome, NavigationOutcome::ObjectFocused);
	ASSERT_TRUE(spike.requestPathPreview());

	const NavigationMove result = spike.move(Direction::North, {candidate("south", {3, 4, 0})});

	EXPECT_EQ(result.outcome, NavigationOutcome::NoDirectionalCandidate);
	ASSERT_TRUE(result.target.objectId.has_value());
	EXPECT_EQ(*result.target.objectId, "resource");
	EXPECT_EQ(result.target.tile, (Tile{4, 3, 0}));
	EXPECT_TRUE(spike.hasPathPreview());
}

TEST_F(AdventureMapNavigationSpikeTest, offscreenObjectRequestsCenterWithoutEnteringCursorMode)
{
	const NavigationMove result = spike.move(Direction::East, {candidate("offscreen", {6, 3, 0}, false)});

	EXPECT_EQ(result.outcome, NavigationOutcome::CameraCenterRequested);
	ASSERT_TRUE(result.target.objectId.has_value());
	EXPECT_EQ(*result.target.objectId, "offscreen");
	EXPECT_FALSE(spike.isCursorMode());
}

TEST_F(AdventureMapNavigationSpikeTest, cursorModeIsExplicitFallbackAndExitRevalidatesNativeTarget)
{
	const std::vector<SceneCandidate> candidates = {candidate("resource", {4, 3, 0})};
	ASSERT_EQ(spike.move(Direction::East, candidates).outcome, NavigationOutcome::ObjectFocused);
	const NavigationTarget beforeCursor = spike.currentTarget();

	EXPECT_FALSE(spike.enterCursorMode(false));
	EXPECT_TRUE(spike.enterCursorMode(true));
	EXPECT_TRUE(spike.isCursorMode());
	EXPECT_EQ(spike.move(Direction::East, candidates).outcome, NavigationOutcome::CursorModeActive);
	EXPECT_EQ(spike.currentTarget().objectId, beforeCursor.objectId);

	EXPECT_EQ(spike.exitCursorMode(candidates), RefreshOutcome::SelectionPreserved);
	EXPECT_FALSE(spike.isCursorMode());
	EXPECT_EQ(spike.currentTarget().objectId, beforeCursor.objectId);
}
}

int main(int argc, char * argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
