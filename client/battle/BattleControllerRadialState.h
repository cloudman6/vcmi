/*
 * BattleControllerRadialState.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../gui/Shortcut.h"

struct BattleControllerRadialItemId
{
	EShortcut shortcut;
	int32_t value = 0;

	BattleControllerRadialItemId(EShortcut shortcut) : shortcut(shortcut) {}

	BattleControllerRadialItemId(EShortcut shortcut, int32_t value) : shortcut(shortcut), value(value) {}

	bool operator==(const BattleControllerRadialItemId &) const = default;
};

struct BattleControllerRadialEntry
{
	BattleControllerRadialItemId id;
	bool enabled;
	size_t slot;
	size_t page = 0;
};

class BattleControllerRadialState
{
public:
	static constexpr size_t SLOT_COUNT = 12;

private:
	bool openState = false;
	std::vector<BattleControllerRadialEntry> entries;
	std::vector<size_t> pageIds;
	size_t activePageIndex = 0;
	std::optional<BattleControllerRadialItemId> selected;
	std::optional<BattleControllerRadialItemId> pendingConfirm;

	std::optional<BattleControllerRadialItemId> itemAt(size_t slot) const;
	const BattleControllerRadialEntry * findCurrent(BattleControllerRadialItemId id, const std::vector<BattleControllerRadialEntry> & currentEntries) const;

public:
	void open(const std::vector<BattleControllerRadialEntry> & initialEntries);
	bool selectDirection(double x, double y);
	bool changePage(int offset);
	bool pressConfirm(const std::vector<BattleControllerRadialEntry> & currentEntries);
	std::optional<BattleControllerRadialItemId> releaseConfirm(const std::vector<BattleControllerRadialEntry> & currentEntries);
	void reset();

	size_t currentPage() const;
	size_t pageCount() const;
	std::optional<BattleControllerRadialItemId> selectedItem() const;
};
