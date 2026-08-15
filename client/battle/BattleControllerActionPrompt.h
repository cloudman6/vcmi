/*
 * BattleControllerActionPrompt.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */
#pragma once

#include "BattleControllerAction.h"

#include "../../lib/Point.h"
#include "../../lib/Rect.h"

class Canvas;

/// Focus-local binding-derived action prompt for native battle navigation.
class BattleControllerActionPrompt
{
	struct ContentLayout
	{
		Point glyphTopLeft;
		Point textCenter;
	};

	static std::string textKey(BattleControllerPrimaryAction action);
	static std::string fallbackBindingLabel(const std::vector<std::string> & bindings);
	static ContentLayout contentLayout(const Rect & promptRect, int textWidth);
	static Rect promptRect(const Rect & anchorRect, const Rect & unobscuredBattlefield);

public:
	static Rect unobscuredBattlefieldRect(const Point & battlefieldOrigin);
	static void draw(Canvas & to, BattleControllerPrimaryAction action,
		const Rect & anchorRect, const Rect & unobscuredBattlefield);
};
