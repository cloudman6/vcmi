/*
 * CExchangePanelNavigationSpike.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "CExchangePanelNavigationSpike.h"

#include <algorithm>
#include <utility>

namespace
{
const std::string EMPTY_STRING;
}

CExchangePanelNavigationSpike::CExchangePanelNavigationSpike(std::vector<ExchangePanelSlot> slots, size_t pageSize)
	: slots(std::move(slots))
	, visiblePageSize(std::max<size_t>(1, pageSize))
{
}

bool CExchangePanelNavigationSpike::selectCategory(EExchangePanelCategory category)
{
	activeCategory = category;
	for(const auto & slot : slots)
	{
		if(slot.category == category)
			return focusSlot(slot.stableId);
	}
	return false;
}

bool CExchangePanelNavigationSpike::focusSlot(const std::string & stableId)
{
	const auto * slot = findSlot(stableId);
	if(!slot)
		return false;

	activeCategory = slot->category;
	focusedStableId = stableId;
	updateScrollOffset();
	return true;
}

bool CExchangePanelNavigationSpike::pickArtifact()
{
	const auto * slot = focusedSlot();
	if(!slot || !slot->unavailableReason.empty() || slot->instanceId.empty())
		return false;

	pickedArtifactSlotId = slot->stableId;
	return true;
}

bool CExchangePanelNavigationSpike::dropPickedArtifact()
{
	const auto * slot = focusedSlot();
	if(!slot || !pickedArtifactSlotId || !slot->unavailableReason.empty() || !slot->acceptsArtifact)
		return false;

	emittedRequests.push_back({EExchangePanelRequestType::DROP_ARTIFACT, *pickedArtifactSlotId, slot->stableId, 0});
	pickedArtifactSlotId.reset();
	return true;
}

bool CExchangePanelNavigationSpike::beginQuantity(const std::string & targetSlotId)
{
	const auto * source = focusedSlot();
	const auto * target = findSlot(targetSlotId);
	if(!source || !target || !source->unavailableReason.empty() || !target->unavailableReason.empty()
		|| !source->minimumQuantity || !source->maximumQuantity || !target->acceptsQuantity)
		return false;

	quantitySourceSlotId = source->stableId;
	quantityTargetSlotId = target->stableId;
	selectedQuantity.reset();
	return true;
}

bool CExchangePanelNavigationSpike::setQuantity(int quantity)
{
	if(!quantitySourceSlotId)
		return false;

	const auto * source = findSlot(*quantitySourceSlotId);
	if(!source || !source->minimumQuantity || !source->maximumQuantity)
		return false;

	if(quantity < *source->minimumQuantity || quantity > *source->maximumQuantity)
		return false;

	selectedQuantity = quantity;
	return true;
}

bool CExchangePanelNavigationSpike::confirmQuantity()
{
	if(!quantitySourceSlotId || !quantityTargetSlotId || !selectedQuantity)
		return false;

	emittedRequests.push_back({EExchangePanelRequestType::SPLIT_STACK, *quantitySourceSlotId, *quantityTargetSlotId, *selectedQuantity});
	quantitySourceSlotId.reset();
	quantityTargetSlotId.reset();
	selectedQuantity.reset();
	return true;
}

void CExchangePanelNavigationSpike::suspendForModal()
{
	if(!focusedStableId || !activeCategory)
		return;

	suspendedCategory = *activeCategory;
	suspendedCategoryIndex = 0;
	for(const auto & slot : slots)
	{
		if(slot.category != *suspendedCategory)
			continue;
		if(slot.stableId == *focusedStableId)
			return;
		++*suspendedCategoryIndex;
	}
}

void CExchangePanelNavigationSpike::restoreAfterModal(std::vector<ExchangePanelSlot> slots)
{
	this->slots = std::move(slots);
	if(focusedStableId && findSlot(*focusedStableId))
	{
		updateScrollOffset();
		return;
	}

	if(suspendedCategory && suspendedCategoryIndex)
	{
		std::vector<const ExchangePanelSlot *> categorySlots;
		for(const auto & slot : this->slots)
		{
			if(slot.category == *suspendedCategory)
				categorySlots.push_back(&slot);
		}
		if(!categorySlots.empty())
		{
			const auto index = std::min(*suspendedCategoryIndex, categorySlots.size() - 1);
			focusSlot(categorySlots[index]->stableId);
			return;
		}
	}

	focusedStableId.reset();
	firstVisibleSlot = 0;
}

EExchangePanelCategory CExchangePanelNavigationSpike::currentCategory() const
{
	return activeCategory.value_or(EExchangePanelCategory::LEFT_EQUIPMENT);
}

const std::string & CExchangePanelNavigationSpike::focusedSlotId() const
{
	return focusedStableId ? *focusedStableId : EMPTY_STRING;
}

const std::string & CExchangePanelNavigationSpike::unavailableReason() const
{
	const auto * slot = focusedSlot();
	return slot ? slot->unavailableReason : EMPTY_STRING;
}

size_t CExchangePanelNavigationSpike::scrollOffset() const
{
	return firstVisibleSlot;
}

const std::vector<ExchangePanelRequest> & CExchangePanelNavigationSpike::requests() const
{
	return emittedRequests;
}

const ExchangePanelSlot * CExchangePanelNavigationSpike::focusedSlot() const
{
	return focusedStableId ? findSlot(*focusedStableId) : nullptr;
}

const ExchangePanelSlot * CExchangePanelNavigationSpike::findSlot(const std::string & stableId) const
{
	const auto found = std::find_if(slots.begin(), slots.end(), [&stableId](const ExchangePanelSlot & slot)
	{
		return slot.stableId == stableId;
	});
	return found == slots.end() ? nullptr : &*found;
}

void CExchangePanelNavigationSpike::updateScrollOffset()
{
	if(!activeCategory || !focusedStableId)
	{
		firstVisibleSlot = 0;
		return;
	}

	size_t focusedIndex = 0;
	size_t categorySize = 0;
	for(const auto & slot : slots)
	{
		if(slot.category != *activeCategory)
			continue;
		if(slot.stableId == *focusedStableId)
			focusedIndex = categorySize;
		++categorySize;
	}

	if(categorySize == 0)
	{
		firstVisibleSlot = 0;
		return;
	}

	if(focusedIndex < firstVisibleSlot)
		firstVisibleSlot = focusedIndex;
	else if(focusedIndex >= firstVisibleSlot + visiblePageSize)
		firstVisibleSlot = focusedIndex - visiblePageSize + 1;
}
