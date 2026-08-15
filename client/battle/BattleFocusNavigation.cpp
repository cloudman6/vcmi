/*
 * BattleFocusNavigation.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "../StdInc.h"

#include "BattleFocusNavigation.h"

namespace
{
constexpr double DEAD_ZONE = 0.45;
constexpr double HYSTERESIS_DEGREES = 10.0;
constexpr double VERTICAL_EDGE_FALLBACK_DEGREES = 10.0;
constexpr uint32_t SETTLE_DELAY_MS = 16;
constexpr uint32_t INITIAL_REPEAT_MS = 320;
constexpr uint32_t REPEAT_MS = 110;

double directionAngle(BattleHex::EDir direction)
{
	switch(direction)
	{
		case BattleHex::RIGHT: return 0.0;
		case BattleHex::BOTTOM_RIGHT: return 60.0;
		case BattleHex::BOTTOM_LEFT: return 120.0;
		case BattleHex::LEFT: return 180.0;
		case BattleHex::TOP_LEFT: return -120.0;
		case BattleHex::TOP_RIGHT: return -60.0;
		default: return 0.0;
	}
}

double angularDistance(double left, double right)
{
	double result = std::fmod(std::abs(left - right), 360.0);
	return result > 180.0 ? 360.0 - result : result;
}
}

BattleFocusNavigation::BattleFocusNavigation(BattleFocusModel & model)
	: model(model)
{
}

void BattleFocusNavigation::updateAxis(int instanceId, Axis axis, double value)
{
	if(activeInstance != -1 && activeInstance != instanceId)
		reset();
	activeInstance = instanceId;
	(axis == Axis::HORIZONTAL ? axisX : axisY) = value;

	const auto next = quantizedDirection();
	if(next == BattleHex::NONE)
	{
		direction = BattleHex::NONE;
		initialPending = false;
		elapsed = 0;
		return;
	}

	if(direction == BattleHex::NONE)
	{
		direction = next;
		initialPending = true;
		elapsed = 0;
		return;
	}

	const double angle = std::atan2(axisY, axisX) * 180.0 / M_PI;
	if(next != direction && angularDistance(angle, directionAngle(direction)) > 30.0 + HYSTERESIS_DEGREES)
	{
		direction = next;
		initialPending = true;
		elapsed = 0;
	}
}

bool BattleFocusNavigation::update(uint32_t msPassed)
{
	if(direction == BattleHex::NONE)
		return false;

	elapsed += msPassed;
	if(initialPending)
	{
		if(elapsed < SETTLE_DELAY_MS)
			return false;
		initialPending = false;
		elapsed = 0;
		return moveFocusWithVerticalEdgeFallback();
	}

	if(elapsed < INITIAL_REPEAT_MS)
		return false;
	elapsed = INITIAL_REPEAT_MS - REPEAT_MS;
	return moveFocusWithVerticalEdgeFallback();
}

void BattleFocusNavigation::reset()
{
	activeInstance = -1;
	axisX = 0.0;
	axisY = 0.0;
	direction = BattleHex::NONE;
	initialPending = false;
	elapsed = 0;
}

BattleHex::EDir BattleFocusNavigation::quantizedDirection() const
{
	if(std::hypot(axisX, axisY) < DEAD_ZONE)
		return BattleHex::NONE;

	const double angle = std::atan2(axisY, axisX) * 180.0 / M_PI;
	if(angle < -150.0 || angle >= 150.0) return BattleHex::LEFT;
	if(angle < -90.0) return BattleHex::TOP_LEFT;
	if(angle < -30.0) return BattleHex::TOP_RIGHT;
	if(angle < 30.0) return BattleHex::RIGHT;
	if(angle < 90.0) return BattleHex::BOTTOM_RIGHT;
	return BattleHex::BOTTOM_LEFT;
}

bool BattleFocusNavigation::moveFocusWithVerticalEdgeFallback()
{
	if(model.moveFocus(direction))
		return true;

	const double angle = std::atan2(axisY, axisX) * 180.0 / M_PI;
	const double verticalAngle = axisY < 0.0 ? -90.0 : 90.0;
	if(angularDistance(angle, verticalAngle) > VERTICAL_EDGE_FALLBACK_DEGREES)
		return false;

	BattleHex::EDir fallback = BattleHex::NONE;
	switch(direction)
	{
		case BattleHex::TOP_LEFT: fallback = BattleHex::TOP_RIGHT; break;
		case BattleHex::TOP_RIGHT: fallback = BattleHex::TOP_LEFT; break;
		case BattleHex::BOTTOM_LEFT: fallback = BattleHex::BOTTOM_RIGHT; break;
		case BattleHex::BOTTOM_RIGHT: fallback = BattleHex::BOTTOM_LEFT; break;
		default: return false;
	}
	return model.moveFocus(fallback);
}
