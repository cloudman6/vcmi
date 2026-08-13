/*
 * BattleFocusNavigation.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "BattleFocusModel.h"

#include "../eventsSDL/InputHandler.h"
#include "../gui/Shortcut.h"

/// Compatibility seam for shortcut navigation. Battle Native Focus does not
/// consume D-pad movement shortcuts; analog navigation uses the axis receiver.
class BattleFocusNavigation
{
public:
	explicit BattleFocusNavigation(BattleFocusModel & model);

	void updateAxis(int instanceId, bool horizontal, double value);
	bool update(uint32_t msPassed);
	void reset();

	/// Returns true when the shortcut is a focus navigation shortcut consumed
	/// under the given input mode, regardless of whether focus could move.
	bool handleShortcut(EShortcut shortcut, InputMode inputMode);

	static bool isNavigationShortcut(EShortcut shortcut);
	static BattleFocusModel::PadDirection padDirection(EShortcut shortcut);

private:
	BattleFocusModel & model;
	int activeInstance = -1;
	double axisX = 0.0;
	double axisY = 0.0;
	BattleHex::EDir direction = BattleHex::NONE;
	bool initialPending = false;
	uint32_t elapsed = 0;

	BattleHex::EDir quantizedDirection() const;
};
