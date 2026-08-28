/*
 * BattleControllerActionPromptRenderer.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "StdInc.h"

#include "BattleControllerActionPrompt.h"

#include "../GameEngine.h"
#include "../eventsSDL/ControllerPromptFamily.h"
#include "../eventsSDL/InputHandler.h"
#include "../gui/Shortcut.h"
#include "../gui/ShortcutHandler.h"
#include "../gui/TextAlignment.h"
#include "../render/Canvas.h"
#include "../render/Colors.h"
#include "../render/EFont.h"
#include "../render/IFont.h"
#include "../render/IRenderHandler.h"
#include "../../lib/GameLibrary.h"
#include "../../lib/texts/CGeneralTextHandler.h"
#include "../../lib/texts/TextOperations.h"

namespace
{
constexpr int GLYPH_SIZE = 24;
constexpr int DIRECTION_GLYPH_WIDTH = 72;
constexpr int DIRECTION_GLYPH_HEIGHT = 20;
constexpr int GLYPH_TEXT_SPACING = 4;
constexpr int TEXT_OUTLINE_WIDTH = 1;
constexpr ColorRGBA GENERIC_FACE_LABEL_COLOR(58, 40, 20, 255);

std::string fitPromptText(const std::string & text, const IFont & font, int maxWidth)
{
	if(maxWidth <= 0)
		return {};
	if(font.getStringWidth(text) <= maxWidth)
		return text;

	const std::string ellipsis = "...";
	if(font.getStringWidth(ellipsis) > maxWidth)
		return {};

	std::string result;
	for(size_t index = 0; index < text.size();)
	{
		const size_t characterSize = TextOperations::getUnicodeCharacterSize(text[index]);
		const std::string candidate = result + text.substr(index, characterSize) + ellipsis;
		if(font.getStringWidth(candidate) > maxWidth)
			break;
		result += text.substr(index, characterSize);
		index += characterSize;
	}
	return result + ellipsis;
}

int outlinedTextWidth(const IFont & font, const std::string & text)
{
	if(text.empty())
		return 0;
	return static_cast<int>(font.getStringWidth(text)) + TEXT_OUTLINE_WIDTH * 2;
}

void drawOutlinedPromptText(Canvas & to, const Point & position, const std::string & text)
{
	to.drawText(position + Point(-TEXT_OUTLINE_WIDTH, 0), FONT_MEDIUM,
		Colors::BLACK, ETextAlignment::CENTER, text);
	to.drawText(position + Point(TEXT_OUTLINE_WIDTH, 0), FONT_MEDIUM,
		Colors::BLACK, ETextAlignment::CENTER, text);
	to.drawText(position + Point(0, -TEXT_OUTLINE_WIDTH), FONT_MEDIUM,
		Colors::BLACK, ETextAlignment::CENTER, text);
	to.drawText(position + Point(0, TEXT_OUTLINE_WIDTH), FONT_MEDIUM,
		Colors::BLACK, ETextAlignment::CENTER, text);
	to.drawText(position, FONT_MEDIUM, Colors::WHITE, ETextAlignment::CENTER, text);
}
}

void BattleControllerActionPrompt::draw(Canvas & to, BattleControllerPrimaryAction action,
	const Rect & anchorRect, const Rect & unobscuredBattlefield, bool pressed,
	bool holdInspectAvailable, bool attackDirectionAvailable)
{
	const auto actionTextKey = textKey(action);
	const auto acceptBindings = ENGINE->shortcuts().getJoystickButtonBindings(EShortcut::GLOBAL_ACCEPT);
	const auto inspectBindings = ENGINE->shortcuts().getJoystickButtonBindings(EShortcut::GLOBAL_CANCEL);
	const auto previousBindings = ENGINE->shortcuts().getJoystickButtonBindings(
		EShortcut::BATTLE_CONTROLLER_PREVIOUS_ATTACK_ORIGIN);
	const auto nextBindings = ENGINE->shortcuts().getJoystickButtonBindings(
		EShortcut::BATTLE_CONTROLLER_NEXT_ATTACK_ORIGIN);
	const auto family = ENGINE->input().getActiveControllerPromptFamily();
	const bool drawPrimary = !actionTextKey.empty() && acceptBindings.size() == 1;
	const bool drawInspect = holdInspectAvailable && inspectBindings.size() == 1;
	const auto directionBindings = bindingPairLabel(previousBindings, nextBindings, family);
	const auto directionSprite = bindingPairSprite(previousBindings, nextBindings, family);
	const bool drawDirection = attackDirectionAvailable && drawPrimary
		&& (!directionSprite.empty() || !directionBindings.empty());
	if(!drawPrimary && !drawInspect)
		return;

	const auto layout = promptLayout(drawPrimary ? action : BattleControllerPrimaryAction::NONE,
		anchorRect, unobscuredBattlefield, drawInspect, drawDirection);
	const auto & font = ENGINE->renderHandler().loadFont(FONT_MEDIUM);
	auto drawButtonPrompt = [&](const Rect & rect, const std::vector<std::string> & bindings,
		const std::string & text, bool buttonPressed)
	{
		const std::string fittedText = fitPromptText(
			text, *font, std::max(0, unobscuredBattlefield.w - GLYPH_SIZE
				- GLYPH_TEXT_SPACING - TEXT_OUTLINE_WIDTH * 2));
		const auto content = contentLayout(rect, outlinedTextWidth(*font, fittedText),
			GLYPH_SIZE, GLYPH_SIZE, unobscuredBattlefield);
		const auto spritePath = buttonSpritePath(bindings, family, buttonPressed);
		if(!spritePath.empty())
		{
			const auto sprite = ENGINE->renderHandler().loadImage(
				ImagePath::builtin(spritePath), EImageBlitMode::COLORKEY);
			to.draw(sprite, content.glyphTopLeft);
			if(spritePath.find("generic-face") != std::string::npos)
			{
				to.drawText(content.glyphTopLeft + Point(GLYPH_SIZE / 2, GLYPH_SIZE / 2),
					FONT_SMALL, GENERIC_FACE_LABEL_COLOR,
					ETextAlignment::CENTER, bindingLabel(bindings.front(), family));
			}
		}
		else
		{
			to.drawText(content.glyphTopLeft + Point(GLYPH_SIZE / 2, GLYPH_SIZE / 2),
				FONT_SMALL, Colors::WHITE, ETextAlignment::CENTER,
				bindingLabel(bindings.front(), family));
		}
		drawOutlinedPromptText(to, content.textCenter, fittedText);
	};

	if(layout.primaryAction)
		drawButtonPrompt(*layout.primaryAction, acceptBindings,
			LIBRARY->generaltexth->translate(actionTextKey), pressed);
	if(layout.attackDirection)
	{
		const auto directionText = LIBRARY->generaltexth->translate(
			"vcmi.battleWindow.controller.attackDirection");
		if(!directionSprite.empty())
		{
			const std::string fittedText = fitPromptText(directionText, *font,
				std::max(0, unobscuredBattlefield.w - DIRECTION_GLYPH_WIDTH
					- GLYPH_TEXT_SPACING - TEXT_OUTLINE_WIDTH * 2));
			const auto content = contentLayout(
				*layout.attackDirection,
				outlinedTextWidth(*font, fittedText),
				DIRECTION_GLYPH_WIDTH,
				DIRECTION_GLYPH_HEIGHT,
				unobscuredBattlefield);
			const auto sprite = ENGINE->renderHandler().loadImage(
				ImagePath::builtin(directionSprite), EImageBlitMode::COLORKEY);
			to.draw(sprite, content.glyphTopLeft);
			drawOutlinedPromptText(to, content.textCenter, fittedText);
		}
		else
		{
			const std::string fittedText = fitPromptText(
				directionBindings + " " + directionText, *font,
				std::max(0, unobscuredBattlefield.w - TEXT_OUTLINE_WIDTH * 2));
			const auto content = contentLayout(*layout.attackDirection,
				outlinedTextWidth(*font, fittedText), 0, 0, unobscuredBattlefield);
			drawOutlinedPromptText(to, content.textCenter, fittedText);
		}
	}
	if(layout.holdInspect)
		drawButtonPrompt(*layout.holdInspect, inspectBindings,
			LIBRARY->generaltexth->translate("vcmi.battleWindow.controller.holdInspect"), false);
}
