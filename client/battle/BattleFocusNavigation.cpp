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

#include <cmath>
#include <numbers>

namespace
{
constexpr double DEAD_ZONE = 0.45;
constexpr double HYSTERESIS_DEGREES = 10.0;
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

void BattleFocusNavigation::updateAxis(int instanceId, bool horizontal, double value)
{
	if(activeInstance != -1 && activeInstance != instanceId)
		reset();
	activeInstance = instanceId;
	(horizontal ? axisX : axisY) = value;

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

	const double angle = std::atan2(axisY, axisX) * 180.0 / std::numbers::pi;
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
		return model.moveFocus(direction);
	}

	if(elapsed < INITIAL_REPEAT_MS)
		return false;
	elapsed = INITIAL_REPEAT_MS - REPEAT_MS;
	return model.moveFocus(direction);
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

	double angle = std::atan2(axisY, axisX) * 180.0 / std::numbers::pi;
	if(angle < -150.0 || angle >= 150.0) return BattleHex::LEFT;
	if(angle < -90.0) return BattleHex::TOP_LEFT;
	if(angle < -30.0) return BattleHex::TOP_RIGHT;
	if(angle < 30.0) return BattleHex::RIGHT;
	if(angle < 90.0) return BattleHex::BOTTOM_RIGHT;
	return BattleHex::BOTTOM_LEFT;
}

bool BattleFocusNavigation::isNavigationShortcut(EShortcut)
{
	return false;
}

BattleFocusModel::PadDirection BattleFocusNavigation::padDirection(EShortcut shortcut)
{
	switch(shortcut)
	{
		case EShortcut::MOVE_UP:
			return BattleFocusModel::PAD_UP;
		case EShortcut::MOVE_DOWN:
			return BattleFocusModel::PAD_DOWN;
		case EShortcut::MOVE_LEFT:
			return BattleFocusModel::PAD_LEFT;
		case EShortcut::MOVE_RIGHT:
			return BattleFocusModel::PAD_RIGHT;
		default:
			return BattleFocusModel::PAD_NONE;
	}
}

bool BattleFocusNavigation::handleShortcut(EShortcut, InputMode)
{
	return false;
}
