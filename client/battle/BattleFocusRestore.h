/*
 * BattleFocusRestore.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/battle/BattleHex.h"

#include "../eventsSDL/InputHandler.h"

/// Decides where the controller focus lands after the active stack changed
/// (D8 default focus and restore): on battle entry and after target death,
/// authority rejection or a netpack refresh the focus returns to the active
/// stack hex. The caller resolves that hex - CStack::getPosition() is the
/// head hex for wide units - and keeps the last valid focus whenever the
/// battle has no active stack.
class BattleFocusRestore
{
public:
	/// Returns the hex the focus must move to, or INVALID when the focus
	/// stays where it is.
	static BattleHex decide(InputMode inputMode, const BattleHex & activeStackHead, bool cursorMode = false);

	/// Explicit Cursor Mode exit restores the focus saved on entry when it is
	/// still a valid board hex, then falls back to the current active stack.
	static BattleHex afterCursorMode(const BattleHex & savedFocus, const BattleHex & activeStackHead);
};
