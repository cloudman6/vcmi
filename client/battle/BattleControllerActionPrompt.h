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

#include "../eventsSDL/ControllerPromptFamily.h"

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
	struct PromptLayout
	{
		std::optional<Rect> primaryAction;
		std::optional<Rect> holdInspect;
		std::optional<Rect> attackDirection;
	};

	static std::string textKey(BattleControllerPrimaryAction action);
	static std::string bindingPairLabel(const std::vector<std::string> & previousBindings,
		const std::vector<std::string> & nextBindings, ControllerPrompt::Family family);
	static std::string bindingPairSprite(const std::vector<std::string> & previousBindings,
		const std::vector<std::string> & nextBindings, ControllerPrompt::Family family);
	static ContentLayout contentLayout(const Rect & promptRect, int textWidth,
		int glyphWidth = 24, int glyphHeight = 24, const Rect & contentBounds = Rect());
	static PromptLayout promptLayout(BattleControllerPrimaryAction action,
		const Rect & anchorRect, const Rect & unobscuredBattlefield,
		bool holdInspectAvailable, bool attackDirectionAvailable);

public:
	static Rect unobscuredBattlefieldRect(const Point & battlefieldOrigin);
	static void draw(Canvas & to, BattleControllerPrimaryAction action,
		const Rect & anchorRect, const Rect & unobscuredBattlefield, bool pressed,
		bool holdInspectAvailable, bool attackDirectionAvailable);
};
