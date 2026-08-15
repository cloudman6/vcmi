/*
 * BattleUnitNavigation.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "../StdInc.h"

#include "BattleUnitNavigation.h"

namespace
{
constexpr double DEAD_ZONE = 0.45;
constexpr double DIRECTION_CHANGE_DEGREES = 30.0;
constexpr uint32_t SETTLE_DELAY_MS = 16;
constexpr uint32_t INITIAL_REPEAT_MS = 320;
constexpr uint32_t REPEAT_MS = 110;

double angularDistance(double x1, double y1, double x2, double y2)
{
	const double first = std::atan2(y1, x1) * 180.0 / M_PI;
	const double second = std::atan2(y2, x2) * 180.0 / M_PI;
	double result = std::fmod(std::abs(first - second), 360.0);
	return result > 180.0 ? 360.0 - result : result;
}
}
BattleUnitNavigation::BattleUnitNavigation(BattleFocusModel & model_, CandidateProvider candidateProvider_)
	: model(model_)
	, candidateProvider(std::move(candidateProvider_))
{
}

void BattleUnitNavigation::updateAxis(int instanceId, Axis axis, double value)
{
	if(activeInstance != -1 && activeInstance != instanceId)
		reset();
	activeInstance = instanceId;
	(axis == Axis::HORIZONTAL ? axisX : axisY) = value;

	if(std::hypot(axisX, axisY) < DEAD_ZONE)
	{
		active = false;
		initialPending = false;
		elapsed = 0;
		return;
	}

	if(!active)
	{
		active = true;
		initialPending = true;
		elapsed = 0;
	}
	else if(angularDistance(directionX, directionY, axisX, axisY) > DIRECTION_CHANGE_DEGREES)
	{
		initialPending = true;
		elapsed = 0;
	}

	directionX = axisX;
	directionY = axisY;
}

bool BattleUnitNavigation::update(uint32_t msPassed)
{
	if(!active)
		return false;

	elapsed += msPassed;
	if(initialPending)
	{
		if(elapsed < SETTLE_DELAY_MS)
			return false;
		initialPending = false;
		elapsed = 0;
		return selectUnit();
	}

	if(elapsed < INITIAL_REPEAT_MS)
		return false;
	elapsed = INITIAL_REPEAT_MS - REPEAT_MS;
	return selectUnit();
}

void BattleUnitNavigation::reset()
{
	activeInstance = -1;
	axisX = 0.0;
	axisY = 0.0;
	directionX = 0.0;
	directionY = 0.0;
	active = false;
	initialPending = false;
	elapsed = 0;
}

bool BattleUnitNavigation::isActive() const
{
	return active;
}

bool BattleUnitNavigation::selectUnit()
{
	if(!model.hasFocus())
		return false;

	const auto selected = BattleUnitSelector::select(
		candidateProvider(), model.getFocusedHex(), directionX, directionY);
	return selected && model.setFocus(selected->headHex);
}
