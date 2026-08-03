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
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
class CExchangeWindowRuleOwnerDouble final : public CExchangeWindowPrivate::RequestOwner
{
public:
	bool acceptRequests = true;
	std::optional<ExchangePanelRequest> receivedRequest;

	bool deliverExchangePanelRequest(const ExchangePanelRequest & request) override
	{
		receivedRequest = request;
		return acceptRequests;
	}
};

/// Supplies a synthetic CExchangeWindow snapshot without game data or rules.
class CExchangePanelFixture
{
public:
	CExchangePanelNavigationSpike makePanel(CExchangeWindowPrivate::PanelRequestAdapter & requestAdapter) const
	{
		return CExchangePanelNavigationSpike(makeSlots(), requestAdapter);
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
			{"left-equipment-helm", "artifact-instance-helm", EExchangePanelCategory::LEFT_EQUIPMENT, {EExchangePanelSide::LEFT, EExchangePanelRouteKind::ARTIFACT, 0}, false, false, "", {}, {}},
			{"right-backpack-0", "", EExchangePanelCategory::RIGHT_BACKPACK, {EExchangePanelSide::RIGHT, EExchangePanelRouteKind::ARTIFACT, 19}, true, false, "", {}, {}},
			{"left-army-pikemen", "creature-stack-pikemen", EExchangePanelCategory::LEFT_ARMY, {EExchangePanelSide::LEFT, EExchangePanelRouteKind::ARMY, 0}, false, false, "", 1, 9},
			{"right-army-empty", "", EExchangePanelCategory::RIGHT_ARMY, {EExchangePanelSide::RIGHT, EExchangePanelRouteKind::ARMY, 1}, false, true, "", {}, {}},
			{"left-army-locked", "creature-stack-ally", EExchangePanelCategory::LEFT_ARMY, {EExchangePanelSide::LEFT, EExchangePanelRouteKind::ARMY, 2}, false, false, "Ally army cannot be moved", 1, 9}
		};

		for(int index = 0; index < 20; ++index)
		{
			slots.push_back({
				"left-backpack-" + std::to_string(index),
				"artifact-instance-backpack-" + std::to_string(index),
				EExchangePanelCategory::LEFT_BACKPACK,
				{EExchangePanelSide::LEFT, EExchangePanelRouteKind::ARTIFACT, 19 + index},
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
	CExchangeWindowRuleOwnerDouble owner;
	CExchangeWindowPrivate::PanelRequestAdapter requestAdapter(owner);
	auto panel = fixture.makePanel(requestAdapter);

	require(panel.selectCategory(EExchangePanelCategory::LEFT_BACKPACK), "left backpack category must be selectable");
	require(panel.currentCategory() == EExchangePanelCategory::LEFT_BACKPACK, "selected category must be retained");
	require(panel.focusSlot("left-backpack-14"), "off-page backpack artifact must be focusable");
	require(panel.focusedSlotId() == "left-backpack-14", "stable focus must identify the selected artifact");
	require(panel.scrollOffset() == 11, "focus must scroll a four-slot viewport to include item fourteen");
}

void testArtifactPickAndDropUsesExistingInstance()
{
	CExchangePanelFixture fixture;
	CExchangeWindowRuleOwnerDouble owner;
	CExchangeWindowPrivate::PanelRequestAdapter requestAdapter(owner);
	auto panel = fixture.makePanel(requestAdapter);

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
	CExchangeWindowRuleOwnerDouble owner;
	CExchangeWindowPrivate::PanelRequestAdapter requestAdapter(owner);
	auto panel = fixture.makePanel(requestAdapter);

	require(panel.focusSlot("left-army-pikemen"), "source stack must be focusable");
	require(panel.beginQuantity("right-army-empty"), "snapshot-declared target must enter quantity mode");
	require(!panel.setQuantity(0), "quantity below the supplied range must not be accepted");
	require(panel.setQuantity(1), "supplied minimum quantity must be accepted");
	require(panel.confirmQuantity(), "valid quantity must emit one split request");
	require(panel.requests().size() == 1, "quantity confirmation must emit exactly one request");
	require(panel.requests().front().type == EExchangePanelRequestType::SPLIT_STACK, "quantity request must preserve split semantics");
	require(panel.requests().front().quantity == 1, "quantity request must retain the supplied minimum count");

	auto maximumPanel = fixture.makePanel(requestAdapter);
	require(maximumPanel.focusSlot("left-army-pikemen"), "maximum-bound source stack must be focusable");
	require(maximumPanel.beginQuantity("right-army-empty"), "maximum-bound target must enter quantity mode");
	require(maximumPanel.setQuantity(9), "supplied maximum quantity must be accepted");
	require(maximumPanel.confirmQuantity(), "maximum quantity must emit one split request");
	require(maximumPanel.requests().front().quantity == 9, "quantity request must retain the supplied maximum count");

	auto exceedingPanel = fixture.makePanel(requestAdapter);
	require(exceedingPanel.focusSlot("left-army-pikemen"), "above-maximum source stack must be focusable");
	require(exceedingPanel.beginQuantity("right-army-empty"), "above-maximum target must enter quantity mode");
	require(!exceedingPanel.setQuantity(10), "quantity above the supplied range must not be accepted");
	require(exceedingPanel.requests().empty(), "rejected above-maximum quantity must not emit a request");

	require(panel.focusSlot("left-army-locked"), "disabled slot remains focusable for explanation");
	require(panel.unavailableReason() == "Ally army cannot be moved", "disabled reason must remain observable");
	require(!panel.beginQuantity("right-army-empty"), "disabled source must not emit a quantity request");
	require(panel.requests().size() == 1, "disabled action must not duplicate an existing request");
}

void testCExchangeWindowOwnerDelivery()
{
	CExchangePanelFixture fixture;
	CExchangeWindowRuleOwnerDouble artifactOwner;
	CExchangeWindowPrivate::PanelRequestAdapter artifactRequestAdapter(artifactOwner);
	auto panel = fixture.makePanel(artifactRequestAdapter);

	require(panel.focusSlot("left-equipment-helm"), "delivery source must be focusable");
	require(panel.pickArtifact(), "delivery source must be pickable");
	require(panel.focusSlot("right-backpack-0"), "delivery target must be focusable");
	require(panel.dropPickedArtifact(), "drop must deliver a semantic artifact request");
	require(artifactOwner.receivedRequest.has_value(), "CExchangeWindow artifact owner must receive a request");
	require(artifactOwner.receivedRequest->type == EExchangePanelRequestType::DROP_ARTIFACT, "owner must receive unmodified artifact semantics");
	require(artifactOwner.receivedRequest->sourceSlotId == "left-equipment-helm", "owner must receive the artifact source identity");
	require(artifactOwner.receivedRequest->targetSlotId == "right-backpack-0", "owner must receive the artifact target identity");
	require(artifactOwner.receivedRequest->quantity == 0, "owner must receive the artifact quantity unchanged");

	CExchangeWindowRuleOwnerDouble splitOwner;
	CExchangeWindowPrivate::PanelRequestAdapter splitRequestAdapter(splitOwner);
	auto quantityPanel = fixture.makePanel(splitRequestAdapter);
	require(quantityPanel.focusSlot("left-army-pikemen"), "quantity delivery source must be focusable");
	require(quantityPanel.beginQuantity("right-army-empty"), "quantity delivery target must enter quantity mode");
	require(quantityPanel.setQuantity(1), "quantity delivery must accept the supplied lower bound");
	require(quantityPanel.confirmQuantity(), "quantity confirmation must deliver a semantic split request");
	require(splitOwner.receivedRequest.has_value(), "CExchangeWindow stack owner must receive a request");
	require(splitOwner.receivedRequest->type == EExchangePanelRequestType::SPLIT_STACK, "owner must receive unmodified split semantics");
	require(splitOwner.receivedRequest->sourceSlotId == "left-army-pikemen", "owner must receive the split source identity");
	require(splitOwner.receivedRequest->targetSlotId == "right-army-empty", "owner must receive the split target identity");
	require(splitOwner.receivedRequest->quantity == 1, "owner must receive the chosen split count unchanged");
}

void testOwnerRejectionPreventsReportedSuccess()
{
	CExchangePanelFixture fixture;
	CExchangeWindowRuleOwnerDouble owner;
	owner.acceptRequests = false;
	CExchangeWindowPrivate::PanelRequestAdapter requestAdapter(owner);
	auto panel = fixture.makePanel(requestAdapter);

	require(panel.focusSlot("left-equipment-helm"), "rejected delivery source must be focusable");
	require(panel.pickArtifact(), "rejected delivery source must still be pickable");
	require(panel.focusSlot("right-backpack-0"), "rejected delivery target must be focusable");
	require(!panel.dropPickedArtifact(), "owner rejection must prevent a reported artifact success");
	require(owner.receivedRequest.has_value(), "owner must receive the rejected artifact request");
	require(panel.requests().empty(), "owner rejection must not record a completed request");
}

void testModalRestoreUsesStableFocusThenDeterministicFallback()
{
	CExchangePanelFixture fixture;
	CExchangeWindowRuleOwnerDouble owner;
	CExchangeWindowPrivate::PanelRequestAdapter requestAdapter(owner);
	auto panel = fixture.makePanel(requestAdapter);

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
		{"cexchangewindow-owner-delivery", testCExchangeWindowOwnerDelivery},
		{"owner-rejection-prevents-success", testOwnerRejectionPreventsReportedSuccess},
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
