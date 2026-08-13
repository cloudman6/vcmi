/*
 * BattleHintBarPresenter.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../StdInc.h"
#include "BattleHintBarPresenter.h"

#include "BattleHintBar.h"
#include "BattleInterface.h"

#include "../GameEngine.h"
#include "../eventsSDL/InputHandler.h"
#include "../eventsSDL/InputSourceGameController.h"
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
/// Same M2 sprite recipe as CObjectListWindow's controllerActionPromptSprite:
/// the accept/cancel and shoulder bindings have raster glyphs; every other
/// shortcut (D-pad direction adjust) renders as text only.
std::optional<ImagePath> hintGlyphSprite(EShortcut shortcut, bool acceptPressed)
{
	const auto presentation = ENGINE->input().getControllerPresentation();
	const auto bindings = ENGINE->shortcuts().getJoystickBindings(shortcut, InputSourceGameController::getProfileName(presentation));
	if(bindings.size() != 1)
		return std::nullopt;

	const bool accept = shortcut == EShortcut::GLOBAL_ACCEPT && bindings.front() == "a";
	const bool cancel = shortcut == EShortcut::GLOBAL_CANCEL && bindings.front() == "b";
	const bool leftShoulder = shortcut == EShortcut::BATTLE_DEFEND && bindings.front() == "leftshoulder";
	const bool rightShoulder = shortcut == EShortcut::BATTLE_WAIT && bindings.front() == "rightshoulder";
	if(!accept && !cancel && !leftShoulder && !rightShoulder)
		return std::nullopt;

	const std::string family = presentation == ControllerPresentation::PLAYSTATION ? "playstation" : "generic";
	const std::string action = accept ? "add" : cancel ? "cancel" : leftShoulder ? "lb" : "rb";
	const std::string stateName = accept && acceptPressed ? "pressed" : "normal";
	return ImagePath::builtin("controllerActionBar/" + family + "-" + action + "-" + stateName + ".png");
}

struct HintCell
{
	std::shared_ptr<IImage> sprite;
	std::string text;
	int width = 0;
};
}

ColorRGBA BattleHintBarPresenter::backgroundColor()
{
	return {73, 53, 34};
}

void BattleHintBarPresenter::draw(Canvas & to, const std::vector<BattleHintEntry> & entries, const Rect & barRect, bool acceptPressed)
{
	if(entries.empty())
		return;

	const auto & font = ENGINE->renderHandler().loadFont(FONT_SMALL);

	std::vector<HintCell> cells;
	int totalWidth = 0;
	for(const auto & entry : entries)
	{
		HintCell cell;
		const auto spritePath = entry.showGlyph ? hintGlyphSprite(entry.glyph, acceptPressed) : std::nullopt;
		if(spritePath)
			cell.sprite = ENGINE->renderHandler().loadImage(*spritePath, EImageBlitMode::COLORKEY);
		cell.text = LIBRARY->generaltexth->translate(entry.textKey);
		const int textWidth = static_cast<int>(font->getStringWidth(cell.text));
		cell.width = textWidth;
		if(cell.sprite)
			cell.width += BattleHintBarLayout::GLYPH_SIZE + BattleHintBarLayout::GLYPH_TEXT_SPACING;
		cells.push_back(std::move(cell));
		totalWidth += cells.back().width;
	}
	totalWidth += BattleHintBarLayout::ENTRY_SPACING * static_cast<int>(cells.size() - 1);

	to.drawColor(barRect, backgroundColor());

	int x = barRect.x + (barRect.w - totalWidth) / 2;
	const int spriteY = barRect.y + (barRect.h - BattleHintBarLayout::GLYPH_SIZE) / 2;
	const int textTopY = barRect.y + (barRect.h - static_cast<int>(font->getLineHeight())) / 2;
	for(const auto & cell : cells)
	{
		int textLeft = x;
		if(cell.sprite)
		{
			to.draw(cell.sprite, Point(x, spriteY));
			textLeft += BattleHintBarLayout::GLYPH_SIZE + BattleHintBarLayout::GLYPH_TEXT_SPACING;
		}
		to.drawText(
			Point(textLeft + static_cast<int>(font->getStringWidth(cell.text)) / 2, textTopY),
			FONT_SMALL, Colors::WHITE, ETextAlignment::CENTER, cell.text);
		x += cell.width + BattleHintBarLayout::ENTRY_SPACING;
	}
}

BattleHintBarWidget::BattleHintBarWidget(BattleInterface & owner_)
	: CIntObject(CIntObject::SHOWALL, Point(0, BattleHintBarLayout::TOP))
	, owner(owner_)
{
	pos.w = 800;
	pos.h = BattleHintBarLayout::HEIGHT;
}

void BattleHintBarWidget::setAcceptPressed(bool on)
{
	acceptPressed = on;
}

void BattleHintBarWidget::showAll(Canvas & to)
{
	// the tactics phase has its own button set and is outside the M3-1
	// controller contract
	if(owner.tacticsMode)
		return;
	if(owner.isControllerCursorMode())
	{
		BattleHintBarPresenter::draw(to, {{EShortcut::NONE, "vcmi.battleWindow.hints.cursorMode", false}}, Rect(pos.x, pos.y, pos.w, pos.h), false);
		return;
	}

	const auto entries = BattleHintBar::entries(
		ENGINE->input().getCurrentInputMode(),
		owner.controllerStates.top(),
		owner.buildHintContext());
	BattleHintBarPresenter::draw(to, entries, Rect(pos.x, pos.y, pos.w, pos.h), acceptPressed);
}
