/*
 * BattleUnitSelector.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "../StdInc.h"

#include "BattleUnitSelector.h"

namespace
{
constexpr double SCORE_EPSILON = 1e-9;

struct Position
{
	double x;
	double y;
};

struct Score
{
	double alignment;
	double distanceSquared;
	uint32_t unitId;
};

Position hexCenter(const BattleHex & hex)
{
	return {
		44.0 * hex.getX() + (hex.getY() % 2 == 0 ? 22.0 : 0.0),
		42.0 * hex.getY()
	};
}

Position occupiedCenter(const BattleUnitNavigationCandidate & candidate)
{
	const auto head = hexCenter(candidate.headHex);
	if(!candidate.tailHex.isValid())
		return head;

	const auto tail = hexCenter(candidate.tailHex);
	return {(head.x + tail.x) / 2.0, (head.y + tail.y) / 2.0};
}

bool contains(const BattleUnitNavigationCandidate & candidate, const BattleHex & hex)
{
	return candidate.headHex == hex || candidate.tailHex == hex;
}

bool better(const Score & candidate, const Score & current)
{
	if(candidate.alignment > current.alignment + SCORE_EPSILON)
		return true;
	if(std::abs(candidate.alignment - current.alignment) > SCORE_EPSILON)
		return false;

	if(candidate.distanceSquared + SCORE_EPSILON < current.distanceSquared)
		return true;
	if(std::abs(candidate.distanceSquared - current.distanceSquared) > SCORE_EPSILON)
		return false;

	return candidate.unitId < current.unitId;
}
}

std::optional<BattleUnitNavigationCandidate> BattleUnitSelector::select(
	const std::vector<BattleUnitNavigationCandidate> & candidates,
	const BattleHex & focusedHex,
	double directionX,
	double directionY)
{
	if(!focusedHex.isValid() || !std::isfinite(directionX) || !std::isfinite(directionY))
		return std::nullopt;

	const double directionLength = std::hypot(directionX, directionY);
	if(directionLength <= SCORE_EPSILON)
		return std::nullopt;

	Position origin = hexCenter(focusedHex);
	std::optional<uint32_t> currentUnitId;
	for(const auto & candidate : candidates)
	{
		if(candidate.headHex.isValid() && contains(candidate, focusedHex))
		{
			origin = occupiedCenter(candidate);
			currentUnitId = candidate.unitId;
			break;
		}
	}

	std::optional<BattleUnitNavigationCandidate> selected;
	Score selectedScore{-std::numeric_limits<double>::infinity(), 0.0, 0};
	for(const auto & candidate : candidates)
	{
		if(!candidate.headHex.isValid() || (currentUnitId && candidate.unitId == *currentUnitId))
			continue;

		const auto target = occupiedCenter(candidate);
		const double deltaX = target.x - origin.x;
		const double deltaY = target.y - origin.y;
		const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
		if(distanceSquared <= SCORE_EPSILON)
			continue;

		const double dot = directionX * deltaX + directionY * deltaY;
		if(dot <= 0.0)
			continue;

		const Score score{
			dot / (directionLength * std::sqrt(distanceSquared)),
			distanceSquared,
			candidate.unitId
		};
		if(!selected || better(score, selectedScore))
		{
			selected = candidate;
			selectedScore = score;
		}
	}

	return selected;
}
