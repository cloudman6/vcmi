/*
 * CExchangePanelNavigationSpikeTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../../client/widgets/CExchangePanelNavigationSpike.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
/// Supplies a synthetic CExchangeWindow snapshot without game data or rules.
class CExchangePanelFixture
{
public:
	CExchangePanelNavigationSpike makePanel() const
	{
		return CExchangePanelNavigationSpike(makeSlots());
	}

	std::vector<ExchangePanelSlot> slotsWithout(const std::string & stableId) const
	{
		auto slots = makeSlots();
		std::erase_if(slots, [&stableId](const ExchangePanelSlot & slot)
		{
			return slot.stableId == stableId;
		});
		return slots;
	}

private:
	std::vector<ExchangePanelSlot> makeSlots() const
	{
		std::vector<ExchangePanelSlot> slots = {
			{"left-equipment-helm", "artifact-instance-helm", EExchangePanelCategory::LEFT_EQUIPMENT, false, false, "", {}, {}},
			{"right-backpack-0", "", EExchangePanelCategory::RIGHT_BACKPACK, true, false, "", {}, {}},
			{"left-army-pikemen", "creature-stack-pikemen", EExchangePanelCategory::LEFT_ARMY, false, false, "", 1, 9},
			{"right-army-empty", "", EExchangePanelCategory::RIGHT_ARMY, false, true, "", {}, {}},
			{"left-army-locked", "creature-stack-ally", EExchangePanelCategory::LEFT_ARMY, false, false, "Ally army cannot be moved", 1, 9}
		};

		for(int index = 0; index < 20; ++index)
		{
			slots.push_back({
				"left-backpack-" + std::to_string(index),
				"artifact-instance-backpack-" + std::to_string(index),
				EExchangePanelCategory::LEFT_BACKPACK,
				false,
				false,
				"",
				{},
				{}
			});
		}

		return slots;
	}
};

void require(bool condition, const std::string & message)
{
	if(!condition)
		throw std::runtime_error(message);
}

void testCategoriesAndScrolling()
{
	CExchangePanelFixture fixture;
	auto panel = fixture.makePanel();

	require(panel.selectCategory(EExchangePanelCategory::LEFT_BACKPACK), "left backpack category must be selectable");
	require(panel.currentCategory() == EExchangePanelCategory::LEFT_BACKPACK, "selected category must be retained");
	require(panel.focusSlot("left-backpack-14"), "off-page backpack artifact must be focusable");
	require(panel.focusedSlotId() == "left-backpack-14", "stable focus must identify the selected artifact");
	require(panel.scrollOffset() == 11, "focus must scroll a four-slot viewport to include item fourteen");
}

void testArtifactPickAndDropUsesExistingInstance()
{
	CExchangePanelFixture fixture;
	auto panel = fixture.makePanel();

	require(panel.focusSlot("left-equipment-helm"), "equipped artifact must be focusable");
	require(panel.pickArtifact(), "focused artifact must enter presentation pickup state");
	require(panel.focusSlot("right-backpack-0"), "backpack target must be focusable");
	require(panel.dropPickedArtifact(), "snapshot-declared artifact target must accept a drop request");
	require(panel.requests().size() == 1, "drop must emit exactly one request");
	require(panel.requests().front().type == EExchangePanelRequestType::DROP_ARTIFACT, "request must keep artifact operation semantic");
	require(panel.requests().front().sourceSlotId == "left-equipment-helm", "drop must retain the source slot id");
	require(panel.requests().front().targetSlotId == "right-backpack-0", "drop must retain the target slot id");
}

void testQuantityAndUnavailableReason()
{
	CExchangePanelFixture fixture;
	auto panel = fixture.makePanel();

	require(panel.focusSlot("left-army-pikemen"), "source stack must be focusable");
	require(panel.beginQuantity("right-army-empty"), "snapshot-declared target must enter quantity mode");
	require(!panel.setQuantity(0), "quantity below the supplied range must not be accepted");
	require(panel.setQuantity(5), "quantity inside the supplied range must be accepted");
	require(panel.confirmQuantity(), "valid quantity must emit one split request");
	require(panel.requests().size() == 1, "quantity confirmation must emit exactly one request");
	require(panel.requests().front().type == EExchangePanelRequestType::SPLIT_STACK, "quantity request must preserve split semantics");
	require(panel.requests().front().quantity == 5, "quantity request must retain the chosen count");

	require(panel.focusSlot("left-army-locked"), "disabled slot remains focusable for explanation");
	require(panel.unavailableReason() == "Ally army cannot be moved", "disabled reason must remain observable");
	require(!panel.beginQuantity("right-army-empty"), "disabled source must not emit a quantity request");
	require(panel.requests().size() == 1, "disabled action must not duplicate an existing request");
}

void testModalRestoreUsesStableFocusThenDeterministicFallback()
{
	CExchangePanelFixture fixture;
	auto panel = fixture.makePanel();

	require(panel.focusSlot("left-backpack-14"), "focus must enter the large list before modal suspension");
	panel.suspendForModal();
	panel.restoreAfterModal(fixture.slotsWithout("left-backpack-14"));
	require(panel.focusedSlotId() == "left-backpack-15", "removed focus must recover to the next stable slot in its category");
	require(panel.scrollOffset() == 11, "fallback focus must remain scrolled into view");
}
}

int main()
{
	const std::vector<std::pair<std::string, std::function<void()>>> tests = {
		{"categories-and-scrolling", testCategoriesAndScrolling},
		{"artifact-pick-and-drop", testArtifactPickAndDropUsesExistingInstance},
		{"quantity-and-unavailable-reason", testQuantityAndUnavailableReason},
		{"modal-focus-restore", testModalRestoreUsesStableFocusThenDeterministicFallback}
	};

	for(const auto & [name, test] : tests)
	{
		try
		{
			test();
			std::cout << "PASS " << name << '\n';
		}
		catch(const std::exception & error)
		{
			std::cerr << "FAIL " << name << ": " << error.what() << '\n';
			return 1;
		}
	}

	return 0;
}
