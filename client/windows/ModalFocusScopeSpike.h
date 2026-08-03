/*
 * ModalFocusScopeSpike.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */
#pragma once

#include "../gui/Shortcut.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

enum class EModalFocusScopeSpikeResult
{
	IGNORED,
	FOCUS_CHANGED,
	ACCEPTED,
	DISABLED,
	CANCELED
};

class ModalFocusScopeSpike
{
public:
	struct Entry
	{
		std::string id;
		bool enabled;
		std::string disabledReason;
	};

	ModalFocusScopeSpike(std::vector<Entry> entries, std::string initialFocus)
		: entries(std::move(entries))
		, initialFocus(std::move(initialFocus))
	{
		restoreFocus();
	}

	EModalFocusScopeSpikeResult handleShortcut(EShortcut shortcut)
	{
		if(suspended)
			return EModalFocusScopeSpikeResult::IGNORED;

		const auto currentIndex = focusedIndex();
		if(!currentIndex)
			return EModalFocusScopeSpikeResult::IGNORED;

		switch(shortcut)
		{
		case EShortcut::MOVE_UP:
		case EShortcut::MOVE_LEFT:
			if(*currentIndex == 0)
				return EModalFocusScopeSpikeResult::IGNORED;
			focused = entries[*currentIndex - 1].id;
			return EModalFocusScopeSpikeResult::FOCUS_CHANGED;

		case EShortcut::MOVE_DOWN:
		case EShortcut::MOVE_RIGHT:
			if(*currentIndex + 1 == entries.size())
				return EModalFocusScopeSpikeResult::IGNORED;
			focused = entries[*currentIndex + 1].id;
			return EModalFocusScopeSpikeResult::FOCUS_CHANGED;

		case EShortcut::GLOBAL_ACCEPT:
			return entries[*currentIndex].enabled
				? EModalFocusScopeSpikeResult::ACCEPTED
				: EModalFocusScopeSpikeResult::DISABLED;

		case EShortcut::GLOBAL_CANCEL:
			return EModalFocusScopeSpikeResult::CANCELED;

		default:
			return EModalFocusScopeSpikeResult::IGNORED;
		}
	}

	bool focusFromMouse(std::string_view id)
	{
		if(suspended)
			return false;

		const auto index = findIndex(id);
		if(!index)
			return false;

		focused = entries[*index].id;
		return true;
	}

	void suspend()
	{
		suspended = true;
	}

	void resume()
	{
		suspended = false;
		restoreFocus();
	}

	void replaceEntries(std::vector<Entry> entries)
	{
		this->entries = std::move(entries);
		restoreFocus();
	}

	bool isSuspended() const
	{
		return suspended;
	}

	std::optional<size_t> focusedIndex() const
	{
		return findIndex(focused);
	}

	std::string_view focusedId() const
	{
		return focused;
	}

	std::string_view disabledReason() const
	{
		const auto index = focusedIndex();
		if(!index || entries[*index].enabled)
			return {};
		return entries[*index].disabledReason;
	}

private:
	std::optional<size_t> findIndex(std::string_view id) const
	{
		for(size_t index = 0; index < entries.size(); ++index)
			if(entries[index].id == id)
				return index;
		return std::nullopt;
	}

	void restoreFocus()
	{
		if(findIndex(focused))
			return;
		if(findIndex(initialFocus))
		{
			focused = initialFocus;
			return;
		}
		focused = entries.empty() ? std::string() : entries.front().id;
	}

	std::vector<Entry> entries;
	std::string initialFocus;
	std::string focused;
	bool suspended = false;
};
