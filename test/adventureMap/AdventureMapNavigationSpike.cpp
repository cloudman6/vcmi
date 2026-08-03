/*
 * AdventureMapNavigationSpike.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "AdventureMapNavigationSpike.h"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <utility>

namespace adventureMapNavigationSpike
{
namespace
{
bool contains(const Viewport & viewport, const Tile & tile)
{
	return tile.x >= viewport.minimum.x && tile.x <= viewport.maximum.x
		&& tile.y >= viewport.minimum.y && tile.y <= viewport.maximum.y
		&& tile.z >= viewport.minimum.z && tile.z <= viewport.maximum.z;
}

Tile clampToBounds(const MapBounds & bounds, Tile tile)
{
	tile.x = std::clamp(tile.x, bounds.minimum.x, bounds.maximum.x);
	tile.y = std::clamp(tile.y, bounds.minimum.y, bounds.maximum.y);
	tile.z = std::clamp(tile.z, bounds.minimum.z, bounds.maximum.z);
	return tile;
}

bool isAtViewportEdge(const Viewport & viewport, const Tile & tile)
{
	return contains(viewport, tile)
		&& (tile.x == viewport.minimum.x || tile.x == viewport.maximum.x
			|| tile.y == viewport.minimum.y || tile.y == viewport.maximum.y);
}

Tile step(const Tile & tile, const Direction direction)
{
	Tile result = tile;
	switch(direction)
	{
	case Direction::North:
		--result.y;
		break;
	case Direction::South:
		++result.y;
		break;
	case Direction::West:
		--result.x;
		break;
	case Direction::East:
		++result.x;
		break;
	}
	return result;
}

std::pair<int, int> vectorFor(const Direction direction)
{
	switch(direction)
	{
	case Direction::North:
		return {0, -1};
	case Direction::South:
		return {0, 1};
	case Direction::West:
		return {-1, 0};
	case Direction::East:
		return {1, 0};
	}
	return {0, 0};
}

struct DirectionScore
{
	int64_t forward;
	int64_t lateral;
	int64_t distanceSquared;
	std::string stableId;
};

std::optional<DirectionScore> scoreCandidate(const Tile & origin, const Direction direction, const SceneCandidate & candidate)
{
	if(!candidate.focusable || candidate.tile.z != origin.z)
		return std::nullopt;

	const auto [directionX, directionY] = vectorFor(direction);
	const int64_t deltaX = static_cast<int64_t>(candidate.tile.x) - origin.x;
	const int64_t deltaY = static_cast<int64_t>(candidate.tile.y) - origin.y;
	const int64_t forward = deltaX * directionX + deltaY * directionY;
	if(forward <= 0)
		return std::nullopt;

	const int64_t lateral = std::abs(deltaX * directionY - deltaY * directionX);
	return DirectionScore{forward, lateral, deltaX * deltaX + deltaY * deltaY, candidate.stableId};
}

bool isBetterCandidate(const DirectionScore & left, const DirectionScore & right)
{
	const int64_t leftAngle = left.lateral * right.forward;
	const int64_t rightAngle = right.lateral * left.forward;
	if(leftAngle != rightAngle)
		return leftAngle < rightAngle;
	if(left.distanceSquared != right.distanceSquared)
		return left.distanceSquared < right.distanceSquared;
	return left.stableId < right.stableId;
}
}

AdventureMapNavigationSpike::AdventureMapNavigationSpike(
	MapBounds bounds,
	Viewport viewport,
	Tile startTile,
	EShortcut tileCursorShortcut,
	PathPreviewQuery pathPreviewQuery)
	: bounds(bounds)
	, viewport(viewport)
	, tileCursorShortcut(tileCursorShortcut)
	, pathPreviewQuery(std::move(pathPreviewQuery))
	, layer(NavigationLayer::Objects)
	, target({std::nullopt, startTile, tileCursorShortcut, true})
	, previewVisible(false)
	, cursorMode(false)
{
	target.tile = clampToBounds(this->bounds, target.tile);
	target.onScreen = contains(this->viewport, target.tile);
}

void AdventureMapNavigationSpike::setLayer(NavigationLayer newLayer)
{
	layer = newLayer;
}

NavigationMove AdventureMapNavigationSpike::move(const Direction direction, const std::vector<SceneCandidate> & candidates)
{
	if(cursorMode)
		return {NavigationOutcome::CursorModeActive, target};

	if(layer == NavigationLayer::Tiles)
	{
		const Tile nextTile = clampToBounds(bounds, step(target.tile, direction));
		if(nextTile == target.tile)
			return {NavigationOutcome::MapBoundary, target};

		previewVisible = false;
		target = {std::nullopt, nextTile, tileCursorShortcut, contains(viewport, nextTile)};
		if(!target.onScreen || isAtViewportEdge(viewport, nextTile))
			return {NavigationOutcome::CameraPanRequested, target};
		return {NavigationOutcome::TileFocused, target};
	}

	const SceneCandidate * selected = nullptr;
	std::optional<DirectionScore> selectedScore;
	for(const SceneCandidate & candidate : candidates)
	{
		const std::optional<DirectionScore> candidateScore = scoreCandidate(target.tile, direction, candidate);
		if(!candidateScore)
			continue;
		if(!selectedScore || isBetterCandidate(*candidateScore, *selectedScore))
		{
			selected = &candidate;
			selectedScore = candidateScore;
		}
	}

	if(selected == nullptr)
		return {NavigationOutcome::NoDirectionalCandidate, target};

	previewVisible = false;
	target = {selected->stableId, selected->tile, selected->confirmShortcut, selected->onScreen};
	if(!target.onScreen)
		return {NavigationOutcome::CameraCenterRequested, target};
	return {NavigationOutcome::ObjectFocused, target};
}

RefreshOutcome AdventureMapNavigationSpike::refresh(const std::vector<SceneCandidate> & candidates)
{
	if(!target.objectId)
		return RefreshOutcome::SelectionPreserved;

	const auto found = std::find_if(candidates.begin(), candidates.end(), [this](const SceneCandidate & candidate)
	{
		return candidate.focusable && candidate.stableId == *target.objectId;
	});
	if(found != candidates.end())
	{
		if(found->tile != target.tile)
			previewVisible = false;
		target = {found->stableId, found->tile, found->confirmShortcut, found->onScreen};
		return RefreshOutcome::SelectionPreserved;
	}

	target = {std::nullopt, target.tile, tileCursorShortcut, contains(viewport, target.tile)};
	previewVisible = false;
	return RefreshOutcome::FellBackToTile;
}

bool AdventureMapNavigationSpike::requestPathPreview()
{
	if(cursorMode || !pathPreviewQuery)
		return false;

	previewVisible = pathPreviewQuery(target.tile);
	return previewVisible;
}

bool AdventureMapNavigationSpike::commit(const std::vector<AdventureMapShortcutState> & shortcuts)
{
	if(cursorMode || !previewVisible || target.confirmShortcut == EShortcut::NONE)
		return false;

	const auto shortcut = std::find_if(shortcuts.begin(), shortcuts.end(), [this](const AdventureMapShortcutState & state)
	{
		return state.shortcut == target.confirmShortcut;
	});
	if(shortcut == shortcuts.end() || !shortcut->isEnabled || !shortcut->callback)
		return false;

	shortcut->callback();
	previewVisible = false;
	return true;
}

bool AdventureMapNavigationSpike::enterCursorMode(const bool nativeNavigationUnavailable)
{
	if(!nativeNavigationUnavailable)
		return false;

	cursorMode = true;
	previewVisible = false;
	return true;
}

RefreshOutcome AdventureMapNavigationSpike::exitCursorMode(const std::vector<SceneCandidate> & candidates)
{
	cursorMode = false;
	return refresh(candidates);
}

const NavigationTarget & AdventureMapNavigationSpike::currentTarget() const
{
	return target;
}

bool AdventureMapNavigationSpike::hasPathPreview() const
{
	return previewVisible;
}

bool AdventureMapNavigationSpike::isCursorMode() const
{
	return cursorMode;
}
}
