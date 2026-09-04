/*
 * ControllerRadialState.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../gui/Shortcut.h"

struct ControllerRadialItemId
{
	EShortcut shortcut;
	int32_t value = 0;

	ControllerRadialItemId(EShortcut shortcut) : shortcut(shortcut) {}

	ControllerRadialItemId(EShortcut shortcut, int32_t value) : shortcut(shortcut), value(value) {}

	bool operator==(const ControllerRadialItemId &) const = default;
};

struct ControllerRadialEntry
{
	ControllerRadialItemId id;
	bool enabled;
	size_t slot;
	size_t page = 0;
};

class ControllerRadialState
{
public:
	static constexpr size_t DEFAULT_SLOT_COUNT = 12;

private:
	size_t slotCount;
	bool openState = false;
	std::vector<ControllerRadialEntry> entries;
	std::vector<size_t> pageIds;
	size_t activePageIndex = 0;
	std::optional<ControllerRadialItemId> selected;
	std::optional<ControllerRadialItemId> pendingConfirm;

	std::optional<ControllerRadialItemId> itemAt(size_t slot) const;
	const ControllerRadialEntry * findCurrent(ControllerRadialItemId id, const std::vector<ControllerRadialEntry> & currentEntries) const;

public:
	explicit ControllerRadialState(size_t slotCount = DEFAULT_SLOT_COUNT);

	void open(const std::vector<ControllerRadialEntry> & initialEntries);
	bool selectDirection(double x, double y);
	bool changePage(int offset);
	bool pressConfirm(const std::vector<ControllerRadialEntry> & currentEntries);
	std::optional<ControllerRadialItemId> releaseConfirm(const std::vector<ControllerRadialEntry> & currentEntries);
	void reset();

	size_t currentPage() const;
	size_t pageCount() const;
	std::optional<ControllerRadialItemId> selectedItem() const;
};
