/*
 * BattleControllerActionPrompt.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "../StdInc.h"

#include "BattleControllerActionPrompt.h"

namespace
{
constexpr int PROMPT_WIDTH = 138;
constexpr int PROMPT_HEIGHT = 27;
constexpr int PROMPT_GAP = 3;
constexpr int GLYPH_SIZE = 24;
constexpr int GLYPH_TEXT_SPACING = 4;
constexpr int UNOBSCURED_LEFT = 79;
constexpr int UNOBSCURED_TOP = 86;
constexpr int UNOBSCURED_RIGHT = 721;
constexpr int UNOBSCURED_BOTTOM = 555;
}
std::string BattleControllerActionPrompt::textKey(BattleControllerPrimaryAction action)
{
	switch(action)
	{
	case BattleControllerPrimaryAction::MOVE:
		return "vcmi.battleWindow.controller.move";
	case BattleControllerPrimaryAction::INSPECT:
		return "vcmi.battleWindow.controller.inspect";
	default:
		return "";
	}
}

std::string BattleControllerActionPrompt::fallbackBindingLabel(const std::vector<std::string> & bindings)
{
	if(bindings.size() != 1)
		return "";

	std::string result = bindings.front();
	std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character)
	{
		return static_cast<char>(std::toupper(character));
	});
	return result;
}

BattleControllerActionPrompt::ContentLayout BattleControllerActionPrompt::contentLayout(
	const Rect & promptRect, int textWidth)
{
	const int clampedTextWidth = std::max(0, textWidth);
	const int contentWidth = GLYPH_SIZE + GLYPH_TEXT_SPACING + clampedTextWidth;
	const int glyphX = promptRect.center().x - contentWidth / 2;
	const int glyphY = promptRect.center().y - GLYPH_SIZE / 2;
	return {
		Point(glyphX, glyphY),
		Point(glyphX + GLYPH_SIZE + GLYPH_TEXT_SPACING + clampedTextWidth / 2,
			promptRect.center().y)
	};
}

Rect BattleControllerActionPrompt::promptRect(BattleControllerPrimaryAction action,
	const Rect & anchorRect, const Rect & unobscuredBattlefield)
{
	const int width = std::min(PROMPT_WIDTH, unobscuredBattlefield.w);
	const int height = std::min(PROMPT_HEIGHT, unobscuredBattlefield.h);
	const int right = unobscuredBattlefield.x + unobscuredBattlefield.w;
	const int bottom = unobscuredBattlefield.y + unobscuredBattlefield.h;

	const int x = std::clamp(anchorRect.center().x - width / 2, unobscuredBattlefield.x, right - width);
	const bool preferBelow = action != BattleControllerPrimaryAction::INSPECT;
	int y = preferBelow
		? anchorRect.y + anchorRect.h + PROMPT_GAP
		: anchorRect.y - height - PROMPT_GAP;
	if(y < unobscuredBattlefield.y || y + height > bottom)
		y = preferBelow
			? anchorRect.y - height - PROMPT_GAP
			: anchorRect.y + anchorRect.h + PROMPT_GAP;
	y = std::clamp(y, unobscuredBattlefield.y, bottom - height);
	return Rect(x, y, width, height);
}

Rect BattleControllerActionPrompt::unobscuredBattlefieldRect(const Point & battlefieldOrigin)
{
	return Rect(
		battlefieldOrigin.x + UNOBSCURED_LEFT,
		battlefieldOrigin.y + UNOBSCURED_TOP,
		UNOBSCURED_RIGHT - UNOBSCURED_LEFT,
		UNOBSCURED_BOTTOM - UNOBSCURED_TOP);
}
