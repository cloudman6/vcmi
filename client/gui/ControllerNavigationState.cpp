/*
 * ControllerNavigationState.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "StdInc.h"

#include "ControllerNavigationState.h"

namespace
{
constexpr uint32_t NAVIGATION_SETTLE_DELAY_MS = 16;
constexpr uint32_t NAVIGATION_INITIAL_REPEAT_MS = 320;
constexpr uint32_t NAVIGATION_REPEAT_MS = 110;
constexpr double NAVIGATION_DIRECTION_CHANGE_DEGREES = 30.0;

double angularDistance(double x1, double y1, double x2, double y2)
{
	const double first = std::atan2(y1, x1) * 180.0 / M_PI;
	const double second = std::atan2(y2, x2) * 180.0 / M_PI;
	double result = std::fmod(std::abs(first - second), 360.0);
	return result > 180.0 ? 360.0 - result : result;
}
}

void ControllerNavigationRepeatState::start(bool settleFirst)
{
	elapsed = 0;
	initialPending = settleFirst;
	repeating = false;
}

bool ControllerNavigationRepeatState::ready(uint32_t msPassed)
{
	elapsed += msPassed;
	if(initialPending)
	{
		if(elapsed < NAVIGATION_SETTLE_DELAY_MS)
			return false;
		initialPending = false;
		elapsed = 0;
		return true;
	}

	const uint32_t threshold = repeating ? NAVIGATION_REPEAT_MS : NAVIGATION_INITIAL_REPEAT_MS;
	if(elapsed < threshold)
		return false;
	elapsed -= threshold;
	repeating = true;
	return true;
}

void ControllerNavigationRepeatState::reset()
{
	elapsed = 0;
	initialPending = false;
	repeating = false;
}

void ControllerNavigationState::update(bool horizontal, double value)
{
	(horizontal ? axisX : axisY) = value;
	const bool nextActive = !vstd::isAlmostZero(axisX) || !vstd::isAlmostZero(axisY);
	if(!nextActive)
	{
		active = false;
		directionX = directionY = 0.0;
		repeat.reset();
		return;
	}

	if(!active || angularDistance(directionX, directionY, axisX, axisY) > NAVIGATION_DIRECTION_CHANGE_DEGREES)
		repeat.start(true);
	active = true;
	directionX = axisX;
	directionY = axisY;
}

bool ControllerNavigationState::ready(uint32_t msPassed)
{
	return active && repeat.ready(msPassed);
}

void ControllerNavigationState::reset()
{
	axisX = axisY = directionX = directionY = 0.0;
	active = false;
	repeat.reset();
}

bool ControllerNavigationState::isActive() const
{
	return active;
}

double ControllerNavigationState::x() const
{
	return directionX;
}

double ControllerNavigationState::y() const
{
	return directionY;
}

std::pair<double, double> ControllerNavigationState::direction() const
{
	return {directionX, directionY};
}
