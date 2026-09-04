/*
 * BattleControllerRadialState.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "BattleControllerRadialState.h"

namespace
{
constexpr double PI = 3.14159265358979323846;
}

std::optional<BattleControllerRadialItemId> BattleControllerRadialState::itemAt(size_t slot) const
{
	if(activePageIndex >= pageIds.size())
		return std::nullopt;

	const auto entry = std::ranges::find_if(
		entries,
		[this, slot](const auto & candidate)
		{
			return candidate.page == pageIds[activePageIndex] && candidate.slot == slot;
		}
	);
	return entry == entries.end() ? std::optional<BattleControllerRadialItemId>{} : std::optional<BattleControllerRadialItemId>{entry->id};
}

const BattleControllerRadialEntry *
BattleControllerRadialState::findCurrent(BattleControllerRadialItemId id, const std::vector<BattleControllerRadialEntry> & currentEntries) const
{
	const auto current = std::ranges::find(currentEntries, id, &BattleControllerRadialEntry::id);
	return current == currentEntries.end() ? nullptr : &*current;
}

void BattleControllerRadialState::open(const std::vector<BattleControllerRadialEntry> & initialEntries)
{
	openState = true;
	entries.clear();
	pageIds.clear();
	for(const auto & entry : initialEntries)
	{
		if(entry.slot >= SLOT_COUNT)
			continue;
		entries.push_back(entry);
		if(!vstd::contains(pageIds, entry.page))
			pageIds.push_back(entry.page);
	}
	std::ranges::sort(pageIds);
	activePageIndex = 0;
	selected.reset();
	pendingConfirm.reset();
}

bool BattleControllerRadialState::selectDirection(double x, double y)
{
	if(!openState)
		return false;

	std::optional<BattleControllerRadialItemId> newSelection;
	if(x != 0.0 || y != 0.0)
	{
		double counterclockwiseFromNorth = std::atan2(-x, -y);
		if(counterclockwiseFromNorth < 0.0)
			counterclockwiseFromNorth += 2.0 * PI;

		const double sectorAngle = 2.0 * PI / static_cast<double>(SLOT_COUNT);
		const auto slot = static_cast<size_t>(std::floor(counterclockwiseFromNorth / sectorAngle + 0.5)) % SLOT_COUNT;
		newSelection = itemAt(slot);
	}

	if(newSelection != selected)
		pendingConfirm.reset();
	selected = newSelection;
	return true;
}

bool BattleControllerRadialState::changePage(int offset)
{
	if(!openState || offset == 0 || pageIds.size() < 2)
		return false;

	const int nextPageIndex = static_cast<int>(activePageIndex) + offset;
	if(nextPageIndex < 0 || nextPageIndex >= static_cast<int>(pageIds.size()))
		return false;

	activePageIndex = static_cast<size_t>(nextPageIndex);
	selected.reset();
	pendingConfirm.reset();
	return true;
}

bool BattleControllerRadialState::pressConfirm(const std::vector<BattleControllerRadialEntry> & currentEntries)
{
	if(!openState || !selected.has_value())
		return false;

	const auto * current = findCurrent(*selected, currentEntries);
	if(!current)
	{
		selected.reset();
		pendingConfirm.reset();
		return false;
	}
	if(!current->enabled)
	{
		pendingConfirm.reset();
		return false;
	}

	pendingConfirm = selected;
	return true;
}

std::optional<BattleControllerRadialItemId> BattleControllerRadialState::releaseConfirm(const std::vector<BattleControllerRadialEntry> & currentEntries)
{
	if(!openState || !pendingConfirm.has_value() || pendingConfirm != selected)
	{
		pendingConfirm.reset();
		return std::nullopt;
	}

	const auto * current = findCurrent(*pendingConfirm, currentEntries);
	if(!current)
	{
		selected.reset();
		pendingConfirm.reset();
		return std::nullopt;
	}

	if(!current->enabled)
	{
		pendingConfirm.reset();
		return std::nullopt;
	}

	const auto result = pendingConfirm;
	reset();
	return result;
}

void BattleControllerRadialState::reset()
{
	openState = false;
	entries.clear();
	pageIds.clear();
	activePageIndex = 0;
	selected.reset();
	pendingConfirm.reset();
}

size_t BattleControllerRadialState::currentPage() const
{
	return activePageIndex < pageIds.size() ? pageIds[activePageIndex] : 0;
}

size_t BattleControllerRadialState::pageCount() const
{
	return pageIds.size();
}

std::optional<BattleControllerRadialItemId> BattleControllerRadialState::selectedItem() const
{
	return selected;
}
