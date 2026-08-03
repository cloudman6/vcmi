/*
 * CExchangeWindowPanelRequestAdapter.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

struct ExchangePanelRequest;

namespace CExchangeWindowPrivate
{
/// Private CExchangeWindow rule-owner boundary for controller navigation requests.
class RequestOwner
{
public:
	virtual ~RequestOwner() = default;
	virtual bool deliverExchangePanelRequest(const ExchangePanelRequest & request) = 0;
};

/// Non-optional bridge from presentation navigation to the existing window rule owner.
class PanelRequestAdapter
{
public:
	explicit PanelRequestAdapter(RequestOwner & owner);
	bool deliver(const ExchangePanelRequest & request) const;

private:
	RequestOwner & owner;
};
}
