/*
 * BattleControllerActionRadialState.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "BattleControllerActionRadialState.h"

namespace
{
constexpr double DEAD_ZONE = 0.45;
constexpr double PI = 3.14159265358979323846;

std::optional<size_t> selectSlot(double axisX, double axisY)
{
	if(std::hypot(axisX, axisY) < DEAD_ZONE)
		return std::nullopt;

	double clockwiseFromNorth = std::atan2(axisX, -axisY);
	if(clockwiseFromNorth < 0.0)
		clockwiseFromNorth += 2.0 * PI;
	return static_cast<size_t>(std::floor((clockwiseFromNorth + PI / 8.0) / (PI / 4.0))) % 8;
}
}

const BattleControllerActionRadialEntry * BattleControllerActionRadialState::findAction(
	BattleControllerActionRadialAction action) const
{
	auto entry = std::ranges::find(entries, action, &BattleControllerActionRadialEntry::action);
	return entry == entries.end() ? nullptr : &*entry;
}

void BattleControllerActionRadialState::close()
{
	openState = false;
	entries.clear();
	selected.reset();
	pendingConfirm.reset();
}

void BattleControllerActionRadialState::open(
	const std::vector<BattleControllerActionRadialEntry> & currentEntries)
{
	openState = true;
	entries = currentEntries;
	selected.reset();
	pendingConfirm.reset();
}

bool BattleControllerActionRadialState::selectDirection(double axisX, double axisY)
{
	if(!openState)
		return false;

	const auto slot = selectSlot(axisX, axisY);
	const auto entry = slot
		? std::ranges::find(entries, *slot, &BattleControllerActionRadialEntry::slot)
		: entries.end();
	const auto nextSelection = entry == entries.end()
		? std::optional<BattleControllerActionRadialAction>{}
		: std::optional<BattleControllerActionRadialAction>{entry->action};
	if(selected == nextSelection)
		return false;
	pendingConfirm.reset();
	selected = nextSelection;
	return true;
}

bool BattleControllerActionRadialState::pressConfirm()
{
	if(!openState || !selected)
		return false;

	pendingConfirm = selected;
	return true;
}

std::optional<BattleControllerActionRadialAction> BattleControllerActionRadialState::releaseConfirm(
	const std::vector<BattleControllerActionRadialEntry> & currentEntries)
{
	if(!openState || !selected || !pendingConfirm)
		return std::nullopt;

	const auto action = *pendingConfirm;
	pendingConfirm.reset();
	if(action != *selected)
		return std::nullopt;

	entries = currentEntries;
	const auto * current = findAction(action);
	if(!current)
	{
		selected.reset();
		return std::nullopt;
	}
	if(!current->enabled)
		return std::nullopt;

	close();
	return action;
}

void BattleControllerActionRadialState::cancelConfirm()
{
	pendingConfirm.reset();
}

bool BattleControllerActionRadialState::back()
{
	if(!openState)
		return false;

	close();
	return true;
}

void BattleControllerActionRadialState::reset()
{
	close();
}

bool BattleControllerActionRadialState::isOpen() const
{
	return openState;
}

std::optional<BattleControllerActionRadialAction> BattleControllerActionRadialState::selectedAction() const
{
	return selected;
}
