/*
 * CExchangePanelNavigationSpike.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

/// Presentation-only controller navigation experiment for CExchangeWindow data.
enum class EExchangePanelCategory
{
	LEFT_EQUIPMENT,
	LEFT_BACKPACK,
	LEFT_ARMY,
	RIGHT_EQUIPMENT,
	RIGHT_BACKPACK,
	RIGHT_ARMY
};

struct ExchangePanelSlot
{
	std::string stableId;
	std::string instanceId;
	EExchangePanelCategory category;
	bool acceptsArtifact = false;
	bool acceptsQuantity = false;
	std::string unavailableReason;
	std::optional<int> minimumQuantity;
	std::optional<int> maximumQuantity;
};

enum class EExchangePanelRequestType
{
	DROP_ARTIFACT,
	SPLIT_STACK
};

struct ExchangePanelRequest
{
	EExchangePanelRequestType type;
	std::string sourceSlotId;
	std::string targetSlotId;
	int quantity = 0;
};

/// Exercises only presentation state; existing exchange callbacks remain rule owners.
class CExchangePanelNavigationSpike
{
public:
	explicit CExchangePanelNavigationSpike(std::vector<ExchangePanelSlot> slots, size_t pageSize = 4);

	bool selectCategory(EExchangePanelCategory category);
	bool focusSlot(const std::string & stableId);
	bool pickArtifact();
	bool dropPickedArtifact();
	bool beginQuantity(const std::string & targetSlotId);
	bool setQuantity(int quantity);
	bool confirmQuantity();
	void suspendForModal();
	void restoreAfterModal(std::vector<ExchangePanelSlot> slots);

	EExchangePanelCategory currentCategory() const;
	const std::string & focusedSlotId() const;
	const std::string & unavailableReason() const;
	size_t scrollOffset() const;
	const std::vector<ExchangePanelRequest> & requests() const;

private:
	const ExchangePanelSlot * focusedSlot() const;
	const ExchangePanelSlot * findSlot(const std::string & stableId) const;
	void updateScrollOffset();

	std::vector<ExchangePanelSlot> slots;
	std::vector<ExchangePanelRequest> emittedRequests;
	std::optional<EExchangePanelCategory> activeCategory;
	std::optional<std::string> focusedStableId;
	std::optional<std::string> pickedArtifactSlotId;
	std::optional<std::string> quantitySourceSlotId;
	std::optional<std::string> quantityTargetSlotId;
	std::optional<int> selectedQuantity;
	std::optional<EExchangePanelCategory> suspendedCategory;
	std::optional<size_t> suspendedCategoryIndex;
	size_t visiblePageSize;
	size_t firstVisibleSlot = 0;
};
