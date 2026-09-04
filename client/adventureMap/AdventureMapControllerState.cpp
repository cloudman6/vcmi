/*
 * AdventureMapControllerState.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "StdInc.h"

#include "AdventureMapControllerState.h"

namespace
{
constexpr double SCORE_EPSILON = 1e-9;

struct CandidateScore
{
	double alignment;
	double distanceSquared;
	si32 id;
};

bool isBetter(const CandidateScore & candidate, const CandidateScore & selected)
{
	if(candidate.alignment > selected.alignment + SCORE_EPSILON)
		return true;
	if(std::abs(candidate.alignment - selected.alignment) > SCORE_EPSILON)
		return false;

	if(candidate.distanceSquared + SCORE_EPSILON < selected.distanceSquared)
		return true;
	if(std::abs(candidate.distanceSquared - selected.distanceSquared) > SCORE_EPSILON)
		return false;

	return candidate.id < selected.id;
}
}

std::optional<AdventureMapControllerObjectCandidate> AdventureMapControllerObjectSelector::select(
	const std::vector<AdventureMapControllerObjectCandidate> & candidates,
	const Point & origin,
	std::optional<ObjectInstanceID> currentObject,
	double directionX,
	double directionY)
{
	if(!std::isfinite(directionX) || !std::isfinite(directionY))
		return std::nullopt;

	const double directionLength = std::hypot(directionX, directionY);
	if(directionLength <= SCORE_EPSILON)
		return std::nullopt;

	std::optional<AdventureMapControllerObjectCandidate> selected;
	CandidateScore selectedScore{-std::numeric_limits<double>::infinity(), 0.0, 0};
	for(const auto & candidate : candidates)
	{
		if(currentObject == candidate.id)
			continue;

		const double deltaX = candidate.center.x - origin.x;
		const double deltaY = candidate.center.y - origin.y;
		const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
		if(distanceSquared <= SCORE_EPSILON)
			continue;

		const double dot = directionX * deltaX + directionY * deltaY;
		if(dot <= 0.0)
			continue;

		const CandidateScore score{
			dot / (directionLength * std::sqrt(distanceSquared)),
			distanceSquared,
			candidate.id.getNum()
		};
		if(!selected || isBetter(score, selectedScore))
		{
			selected = candidate;
			selectedScore = score;
		}
	}
	return selected;
}

void AdventureMapControllerState::setTarget(const AdventureMapControllerTarget & target)
{
	currentTarget = target;
}

void AdventureMapControllerState::clearTarget()
{
	currentTarget.reset();
	pendingPrimary.reset();
}

const std::optional<AdventureMapControllerTarget> & AdventureMapControllerState::target() const
{
	return currentTarget;
}

bool AdventureMapControllerState::pressPrimary()
{
	pendingPrimary = currentTarget;
	return pendingPrimary.has_value();
}

std::optional<AdventureMapControllerTarget> AdventureMapControllerState::releasePrimary(
	const std::optional<AdventureMapControllerTarget> & revalidatedTarget)
{
	if(!pendingPrimary || pendingPrimary != revalidatedTarget)
	{
		pendingPrimary.reset();
		return std::nullopt;
	}

	const auto result = pendingPrimary;
	pendingPrimary.reset();
	return result;
}

void AdventureMapControllerState::resetInput()
{
	pendingPrimary.reset();
}
