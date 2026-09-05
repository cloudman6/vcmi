/*
 * AdventureMapControllerState.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */
#pragma once

#include "../../lib/GameConstants.h"
#include "../../lib/Point.h"
#include "../../lib/int3.h"

struct AdventureMapInteractionTarget
{
	int3 tile;
	std::optional<ObjectInstanceID> objectId;

	bool operator==(const AdventureMapInteractionTarget &) const = default;
};

struct AdventureMapControllerTarget
{
	/// Tile used to place the scene-focus presentation.
	int3 visualTile;
	/// Origin-free target passed unchanged to canonical hover, primary and secondary owners.
	AdventureMapInteractionTarget interaction;

	bool operator==(const AdventureMapControllerTarget &) const = default;
};

struct AdventureMapControllerObjectCandidate
{
	ObjectInstanceID id;
	Point center;
	int3 interactionTile;
};

/// Stateless directional selection over candidates already accepted by the Adventure owner.
class AdventureMapControllerObjectSelector
{
public:
	static std::optional<AdventureMapControllerObjectCandidate> select(
		const std::vector<AdventureMapControllerObjectCandidate> & candidates,
		const Point & origin,
		std::optional<ObjectInstanceID> currentObject,
		double directionX,
		double directionY);
};

/// Scene-focus identity and press/release state. Gameplay policy remains in AdventureMapInterface.
class AdventureMapControllerState
{
	std::optional<AdventureMapControllerTarget> currentTarget;
	std::optional<AdventureMapControllerTarget> pendingPrimary;

public:
	void setTarget(const AdventureMapControllerTarget & target);
	void clearTarget();
	const std::optional<AdventureMapControllerTarget> & target() const;

	bool pressPrimary();
	std::optional<AdventureMapControllerTarget> releasePrimary(
		const std::optional<AdventureMapControllerTarget> & revalidatedTarget);
	void resetInput();
};

/// Adventure-scoped controller modes. Cursor choice survives transient input
/// resets; held camera state never does.
class AdventureMapControllerModeState
{
	bool cursor = false;
	bool camera = false;
	double cameraX = 0.0;
	double cameraY = 0.0;

public:
	void toggleCursorMode();
	bool cursorMode() const;

	void setCameraHeld(bool held);
	bool cameraHeld() const;
	void updateCameraAxis(bool horizontal, double value);
	std::pair<double, double> cameraDirection() const;

	void resetInput();
};
