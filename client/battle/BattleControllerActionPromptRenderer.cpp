/*
 * BattleControllerActionPromptRenderer.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "../StdInc.h"

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

namespace
{
constexpr int GLYPH_SIZE = 24;

std::optional<ImagePath> acceptSprite(const std::vector<std::string> & bindings)
{
	const auto family = ENGINE->input().getActiveControllerPromptFamily();
	if(family == ControllerPrompt::Family::UNKNOWN || bindings.size() != 1
		|| (bindings.front() != "a" && bindings.front() != "b"))
		return std::nullopt;

	const std::string familyPrefix = family == ControllerPrompt::Family::PLAYSTATION
		? "playstation"
		: "xbox";
	return ImagePath::builtin(
		"controllerActionBar/" + familyPrefix + "-" + bindings.front() + "-normal.png");
}
}

void BattleControllerActionPrompt::draw(Canvas & to, BattleControllerPrimaryAction action,
	const Rect & anchorRect, const Rect & unobscuredBattlefield)
{
	const auto actionTextKey = textKey(action);
	if(actionTextKey.empty())
		return;

	const auto bindings = ENGINE->shortcuts().getJoystickButtonBindings(EShortcut::GLOBAL_ACCEPT);
	if(bindings.size() != 1)
		return;

	const auto rect = promptRect(anchorRect, unobscuredBattlefield);
	const auto spritePath = acceptSprite(bindings);
	const auto glyphLabel = fallbackBindingLabel(bindings);
	const auto text = LIBRARY->generaltexth->translate(actionTextKey);
	const auto & font = ENGINE->renderHandler().loadFont(FONT_SMALL);
	const auto layout = contentLayout(rect, static_cast<int>(font->getStringWidth(text)));

	if(spritePath)
	{
		const auto sprite = ENGINE->renderHandler().loadImage(*spritePath, EImageBlitMode::COLORKEY);
		to.draw(sprite, layout.glyphTopLeft);
	}
	else
	{
		to.drawText(layout.glyphTopLeft + Point(GLYPH_SIZE / 2, GLYPH_SIZE / 2),
			FONT_SMALL, Colors::WHITE, ETextAlignment::CENTER, glyphLabel);
	}

	to.drawText(layout.textCenter,
		FONT_SMALL, Colors::WHITE, ETextAlignment::CENTER, text);
}
