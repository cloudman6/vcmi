/*
 * BattleControllerActionPrompt.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "StdInc.h"

#include "BattleControllerActionPrompt.h"
#include "BattleControllerPromptGlyph.h"

namespace
{
constexpr int PROMPT_WIDTH = 196;
constexpr int PROMPT_HEIGHT = 27;
constexpr int PROMPT_GAP = 3;
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
	case BattleControllerPrimaryAction::ATTACK:
		return "vcmi.battleWindow.controller.attack";
	case BattleControllerPrimaryAction::SHOOT:
		return "vcmi.battleWindow.controller.shoot";
	case BattleControllerPrimaryAction::INSPECT:
		return "vcmi.battleWindow.controller.inspect";
	default:
		return "";
	}
}

std::string BattleControllerActionPrompt::bindingPairLabel(
	const std::vector<std::string> & previousBindings,
	const std::vector<std::string> & nextBindings,
	ControllerPrompt::Family family)
{
	if(previousBindings.size() != 1 || nextBindings.size() != 1)
		return "";
	return BattleControllerPromptGlyph::bindingLabel(previousBindings.front(), family) + "/"
		+ BattleControllerPromptGlyph::bindingLabel(nextBindings.front(), family);
}

std::string BattleControllerActionPrompt::bindingPairSprite(
	const std::vector<std::string> & previousBindings,
	const std::vector<std::string> & nextBindings,
	ControllerPrompt::Family family)
{
	if(previousBindings.size() != 1 || nextBindings.size() != 1)
		return "";
	if(previousBindings.front() != "leftshoulder" || nextBindings.front() != "rightshoulder")
		return "";

	if(family == ControllerPrompt::Family::NINTENDO)
		return "";
	const std::string familyPrefix = family == ControllerPrompt::Family::PLAYSTATION
		? "playstation" : "generic";
	return "controllerActionBar/" + familyPrefix + "-shoulders-normal.png";
}

BattleControllerActionPrompt::ContentLayout BattleControllerActionPrompt::contentLayout(
	const Rect & promptRect, int textWidth, int glyphWidth, int glyphHeight, const Rect & contentBounds)
{
	const int clampedTextWidth = std::max(0, textWidth);
	const int clampedGlyphWidth = std::max(0, glyphWidth);
	const int clampedGlyphHeight = std::max(0, glyphHeight);
	const int spacing = clampedGlyphWidth > 0 ? GLYPH_TEXT_SPACING : 0;
	const int contentWidth = clampedGlyphWidth + spacing + clampedTextWidth;
	int glyphX = promptRect.center().x - contentWidth / 2;
	if(contentBounds.w > 0)
	{
		const int rightmostX = contentBounds.x + std::max(0, contentBounds.w - contentWidth);
		glyphX = std::clamp(glyphX, contentBounds.x, rightmostX);
	}
	const int glyphY = promptRect.center().y - clampedGlyphHeight / 2;
	return {
		Point(glyphX, glyphY),
		Point(glyphX + clampedGlyphWidth + spacing + clampedTextWidth / 2,
			promptRect.center().y)
	};
}

BattleControllerActionPrompt::PromptLayout BattleControllerActionPrompt::promptLayout(
	BattleControllerPrimaryAction action, const Rect & anchorRect, const Rect & unobscuredBattlefield,
	bool holdInspectAvailable, bool attackDirectionAvailable)
{
	const bool primaryAvailable = !textKey(action).empty();
	attackDirectionAvailable = attackDirectionAvailable && action == BattleControllerPrimaryAction::ATTACK;
	const int elementCount = static_cast<int>(primaryAvailable)
		+ static_cast<int>(holdInspectAvailable) + static_cast<int>(attackDirectionAvailable);
	if(elementCount == 0)
		return {};

	const int width = std::min(PROMPT_WIDTH, unobscuredBattlefield.w);
	const int requestedHeight = (primaryAvailable ? PROMPT_HEIGHT : 0)
		+ (holdInspectAvailable ? PROMPT_HEIGHT : 0)
		+ (attackDirectionAvailable ? PROMPT_HEIGHT : 0)
		+ (elementCount - 1) * PROMPT_GAP;
	const int height = std::min(requestedHeight, unobscuredBattlefield.h);
	const int right = unobscuredBattlefield.x + unobscuredBattlefield.w;
	const int bottom = unobscuredBattlefield.y + unobscuredBattlefield.h;

	const int x = std::clamp(anchorRect.center().x - width / 2, unobscuredBattlefield.x, right - width);
	const bool preferBelow = action != BattleControllerPrimaryAction::INSPECT;
	int y = preferBelow
		? anchorRect.y + anchorRect.h + PROMPT_GAP
		: anchorRect.y - height - PROMPT_GAP;
	if(y < unobscuredBattlefield.y || y + height > bottom)
	{
		y = preferBelow
			? anchorRect.y - height - PROMPT_GAP
			: anchorRect.y + anchorRect.h + PROMPT_GAP;
	}
	y = std::clamp(y, unobscuredBattlefield.y, bottom - height);

	PromptLayout result;
	int elementY = y;
	auto addPrompt = [&](std::optional<Rect> & target)
	{
		target = Rect(x, elementY, width, PROMPT_HEIGHT);
		elementY += PROMPT_HEIGHT + PROMPT_GAP;
	};

	if(attackDirectionAvailable) addPrompt(result.attackDirection);
	if(primaryAvailable) addPrompt(result.primaryAction);
	if(holdInspectAvailable) addPrompt(result.holdInspect);
	return result;
}

Rect BattleControllerActionPrompt::unobscuredBattlefieldRect(const Point & battlefieldOrigin)
{
	return Rect(
		battlefieldOrigin.x + UNOBSCURED_LEFT,
		battlefieldOrigin.y + UNOBSCURED_TOP,
		UNOBSCURED_RIGHT - UNOBSCURED_LEFT,
		UNOBSCURED_BOTTOM - UNOBSCURED_TOP);
}
