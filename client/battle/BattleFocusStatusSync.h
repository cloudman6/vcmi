/*
 * BattleFocusStatusSync.h, part of VCMI engine
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

/// Decides which hex owns the official hover status feedback (damage and
/// retaliation preview in the status bar) while controller focus is active.
/// The controller focus reuses the same text host as the mouse hover path -
/// BattleActionsController::onHexHovered - so no second message route exists.
class BattleFocusStatusSync
{
public:
	/// Returns the hex the hover status message should follow, or INVALID
	/// when the status bar stays under pointer control.
	static BattleHex decide(InputMode inputMode, const BattleFocusModel & model);
};
