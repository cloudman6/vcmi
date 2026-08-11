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

/// Routes navigation shortcuts onto the battle focus model. The shortcuts are
/// produced by the existing moveUp/moveDown/moveLeft/moveRight bindings,
/// which the controller D-pad maps onto, so no new binding is introduced.
/// Navigation is only consumed while the game controller input mode is
/// active; keyboard and mouse behaviour stays untouched.
class BattleFocusNavigation
{
public:
	explicit BattleFocusNavigation(BattleFocusModel & model);

	/// Returns true when the shortcut is a focus navigation shortcut consumed
	/// under the given input mode, regardless of whether focus could move.
	bool handleShortcut(EShortcut shortcut, InputMode inputMode);

	static bool isNavigationShortcut(EShortcut shortcut);
	static BattleFocusModel::PadDirection padDirection(EShortcut shortcut);

private:
	BattleFocusModel & model;
};
