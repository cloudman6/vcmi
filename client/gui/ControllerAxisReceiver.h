/*
 * ControllerAxisReceiver.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "Shortcut.h"

struct ControllerAxisEvent
{
	int instanceId = -1;
	std::string axisName;
	std::vector<EShortcut> actions;
	double value = 0.0;
};

enum class ControllerAxisRoute
{
	UNOWNED,
	CAPTURED,
	BLOCKED
};

class IControllerAxisReceiver
{
public:
	virtual ControllerAxisRoute controllerAxisMoved(const ControllerAxisEvent & event) = 0;
	virtual void controllerAxisUpdate(uint32_t msPassed) = 0;
	virtual void controllerAxisReset() = 0;
	virtual bool controllerCursorAllowed() const { return true; }
	virtual ~IControllerAxisReceiver() = default;
};
