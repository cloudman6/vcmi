/*
 * AdventureMapNavigationSpike.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../../client/adventureMap/AdventureMapShortcuts.h"
#include "../../client/gui/Shortcut.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace adventureMapNavigationSpike
{
struct Tile
{
	int x;
	int y;
	int z;

	bool operator==(const Tile & other) const = default;
};

struct MapBounds
{
	Tile minimum;
	Tile maximum;
};

struct Viewport
{
	Tile minimum;
	Tile maximum;
};

enum class NavigationLayer
{
	Objects,
	Tiles
};

enum class Direction
{
	North,
	South,
	West,
	East
};

enum class NavigationOutcome
{
	ObjectFocused,
	CameraCenterRequested,
	TileFocused,
	CameraPanRequested,
	MapBoundary,
	NoDirectionalCandidate,
	CursorModeActive
};

enum class RefreshOutcome
{
	SelectionPreserved,
	FellBackToTile
};

struct SceneCandidate
{
	std::string stableId;
	Tile tile;
	bool focusable;
	bool onScreen;
	EShortcut confirmShortcut;
};

struct NavigationTarget
{
	std::optional<std::string> objectId;
	Tile tile;
	EShortcut confirmShortcut;
	bool onScreen;
};

struct NavigationMove
{
	NavigationOutcome outcome;
	NavigationTarget target;
};

using PathPreviewQuery = std::function<bool(const Tile &)>;

/// Test-only comparison spike. Scene queries and action legality are supplied by callers.
class AdventureMapNavigationSpike
{
	MapBounds bounds;
	Viewport viewport;
	EShortcut tileCursorShortcut;
	PathPreviewQuery pathPreviewQuery;
	NavigationLayer layer;
	NavigationTarget target;
	bool previewVisible;
	bool cursorMode;

public:
	AdventureMapNavigationSpike(
		MapBounds bounds,
		Viewport viewport,
		Tile startTile,
		EShortcut tileCursorShortcut,
		PathPreviewQuery pathPreviewQuery);

	void setLayer(NavigationLayer newLayer);
	NavigationMove move(Direction direction, const std::vector<SceneCandidate> & candidates);
	RefreshOutcome refresh(const std::vector<SceneCandidate> & candidates);
	bool requestPathPreview();
	bool commit(const std::vector<AdventureMapShortcutState> & shortcuts);
	bool enterCursorMode(bool nativeNavigationUnavailable);
	RefreshOutcome exitCursorMode(const std::vector<SceneCandidate> & candidates);

	const NavigationTarget & currentTarget() const;
	bool hasPathPreview() const;
	bool isCursorMode() const;
};
}
