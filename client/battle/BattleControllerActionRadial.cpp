/*
 * BattleControllerActionRadial.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "BattleControllerActionRadial.h"
#include "BattleControllerPromptGlyph.h"

#include "../GameEngine.h"
#include "../eventsSDL/InputHandler.h"
#include "../gui/ShortcutHandler.h"
#include "../gui/TextAlignment.h"
#include "../gui/WindowHandler.h"
#include "../render/Canvas.h"
#include "../render/Colors.h"
#include "../render/EFont.h"
#include "../render/IImage.h"
#include "../render/IRenderHandler.h"

#include "../../lib/GameLibrary.h"
#include "../../lib/texts/CGeneralTextHandler.h"

namespace
{
constexpr double PI = 3.14159265358979323846;
constexpr int RING_OUTER_RADIUS = 176;
constexpr int RING_INNER_RADIUS = 70;
constexpr int LABEL_RADIUS = 126;
constexpr int GLYPH_SIZE = 24;
constexpr int PROMPT_TEXT_CENTER_OFFSET = 70;
constexpr int TEXT_OUTLINE_WIDTH = 1;
constexpr ColorRGBA OVERLAY_COLOR(10, 8, 5, 155);
constexpr ColorRGBA SECTOR_EMPTY(45, 40, 31, 245);
constexpr ColorRGBA SECTOR_NORMAL(120, 82, 34, 255);
constexpr ColorRGBA SECTOR_SELECTED(226, 183, 69, 255);
constexpr ColorRGBA SECTOR_DISABLED(72, 69, 62, 255);
constexpr ColorRGBA SECTOR_DISABLED_SELECTED(125, 116, 92, 255);
constexpr ColorRGBA RING_DIVIDER(28, 22, 16, 255);
constexpr ColorRGBA RING_OUTLINE(218, 178, 82, 255);
constexpr ColorRGBA CENTER_FILL(25, 23, 19, 255);
constexpr ColorRGBA KEYCAP_FILL(225, 205, 155, 255);
constexpr ColorRGBA KEYCAP_PRESSED(255, 226, 142, 255);
constexpr ColorRGBA KEYCAP_BORDER(72, 45, 21, 255);
constexpr ColorRGBA KEYCAP_TEXT(54, 37, 19, 255);
constexpr ColorRGBA DISABLED_TEXT(192, 188, 174, 255);

size_t slotForAction(BattleControllerActionRadialAction action)
{
	switch(action)
	{
	case BattleControllerActionRadialAction::WAIT:
		return 6;
	case BattleControllerActionRadialAction::DEFEND:
		return 2;
	case BattleControllerActionRadialAction::AUTOCOMBAT:
		return 4;
	}
	return 0;
}

double slotAngle(size_t slot)
{
	return -PI / 2.0 + static_cast<double>(slot) * PI / 4.0;
}

Point polarPoint(Point center, double angle, int radius)
{
	return Point(
		center.x + static_cast<int>(std::lround(std::cos(angle) * radius)),
		center.y + static_cast<int>(std::lround(std::sin(angle) * radius)));
}

size_t sectorAtPoint(int x, int y)
{
	double clockwiseFromNorth = std::atan2(static_cast<double>(x), -static_cast<double>(y));
	if(clockwiseFromNorth < 0.0)
		clockwiseFromNorth += 2.0 * PI;
	return static_cast<size_t>(std::floor((clockwiseFromNorth + PI / 8.0) / (PI / 4.0))) % 8;
}

struct RingRun
{
	int y;
	int xBegin;
	int xEnd;
	size_t slot;
};

const std::vector<RingRun> & ringRuns()
{
	static const auto runs = []()
	{
		std::vector<RingRun> result;
		const int innerSquared = RING_INNER_RADIUS * RING_INNER_RADIUS;
		const int outerSquared = RING_OUTER_RADIUS * RING_OUTER_RADIUS;
		for(int y = -RING_OUTER_RADIUS; y <= RING_OUTER_RADIUS; ++y)
		{
			std::optional<size_t> activeSlot;
			int runStart = 0;
			for(int x = -RING_OUTER_RADIUS; x <= RING_OUTER_RADIUS + 1; ++x)
			{
				std::optional<size_t> slot;
				const int radiusSquared = x * x + y * y;
				if(x <= RING_OUTER_RADIUS && radiusSquared >= innerSquared
					&& radiusSquared <= outerSquared)
					slot = sectorAtPoint(x, y);
				if(slot == activeSlot)
					continue;
				if(activeSlot)
					result.push_back({y, runStart, x - 1, *activeSlot});
				activeSlot = slot;
				runStart = x;
			}
		}
		return result;
	}();
	return runs;
}

void drawRing(Canvas & to, Point center, const std::array<ColorRGBA, 8> & colors)
{
	for(const auto & run : ringRuns())
		to.drawLine(Point(center.x + run.xBegin, center.y + run.y),
			Point(center.x + run.xEnd, center.y + run.y), colors[run.slot], colors[run.slot]);
}

void drawCircle(Canvas & to, Point center, int radius, const ColorRGBA & color, int width)
{
	constexpr size_t SEGMENTS = 360;
	for(int stroke = 0; stroke < width; ++stroke)
	{
		const int currentRadius = radius - stroke;
		Point previous = polarPoint(center, 0.0, currentRadius);
		for(size_t segment = 1; segment <= SEGMENTS; ++segment)
		{
			const Point next = polarPoint(center,
				2.0 * PI * static_cast<double>(segment) / static_cast<double>(SEGMENTS),
				currentRadius);
			to.drawLine(previous, next, color, color);
			previous = next;
		}
	}
}

void drawDisc(Canvas & to, Point center, int radius, const ColorRGBA & color)
{
	for(int y = -radius; y <= radius; ++y)
	{
		const int halfWidth = static_cast<int>(std::floor(std::sqrt(radius * radius - y * y)));
		to.drawLine(Point(center.x - halfWidth, center.y + y),
			Point(center.x + halfWidth, center.y + y), color, color);
	}
}

void drawOutlinedText(Canvas & to, Point position, EFonts font, const ColorRGBA & color,
	const std::string & text)
{
	to.drawText(position + Point(-TEXT_OUTLINE_WIDTH, 0), font, Colors::BLACK, ETextAlignment::CENTER, text);
	to.drawText(position + Point(TEXT_OUTLINE_WIDTH, 0), font, Colors::BLACK, ETextAlignment::CENTER, text);
	to.drawText(position + Point(0, -TEXT_OUTLINE_WIDTH), font, Colors::BLACK, ETextAlignment::CENTER, text);
	to.drawText(position + Point(0, TEXT_OUTLINE_WIDTH), font, Colors::BLACK, ETextAlignment::CENTER, text);
	to.drawText(position, font, color, ETextAlignment::CENTER, text);
}
}

BattleControllerActionRadial::BattleControllerActionRadial(ItemProvider provider)
	: WindowBase(KEYBOARD | INPUT_MODE_CHANGE)
	, itemProvider(std::move(provider))
{
	onScreenResize();
	state.open(currentEntries(currentItems()));
}

std::vector<BattleControllerActionRadialItem> BattleControllerActionRadial::currentItems() const
{
	return itemProvider ? itemProvider() : std::vector<BattleControllerActionRadialItem>{};
}

std::vector<BattleControllerActionRadialEntry> BattleControllerActionRadial::currentEntries(
	const std::vector<BattleControllerActionRadialItem> & items) const
{
	std::vector<BattleControllerActionRadialEntry> result;
	result.reserve(items.size());
	for(const auto & item : items)
		result.push_back({item.action, item.enabled, slotForAction(item.action)});
	return result;
}

const std::shared_ptr<IImage> & BattleControllerActionRadial::promptSprite(const std::string & path)
{
	auto cached = promptSpriteCache.find(path);
	if(cached == promptSpriteCache.end())
	{
		auto sprite = ENGINE->renderHandler().loadImage(
			ImagePath::builtin(path), EImageBlitMode::COLORKEY);
		cached = promptSpriteCache.emplace(path, std::move(sprite)).first;
	}
	return cached->second;
}

void BattleControllerActionRadial::drawKeyPrompt(Canvas & to, Point position, EShortcut shortcut,
	const std::string & actionText, bool pressed)
{
	const auto bindings = ENGINE->shortcuts().getJoystickButtonBindings(shortcut);
	const auto family = ENGINE->input().getActiveControllerPromptFamily();
	const auto glyph = BattleControllerPromptGlyph::resolve(bindings, family, pressed);
	if(!glyph.spritePath.empty())
	{
		to.draw(promptSprite(glyph.spritePath), position);
		if(!glyph.runtimeLabel.empty())
			to.drawText(position + Point(GLYPH_SIZE / 2, GLYPH_SIZE / 2), FONT_SMALL,
				KEYCAP_TEXT, ETextAlignment::CENTER, glyph.runtimeLabel);
	}
	else
	{
		const Rect keycap(position, Point(36, GLYPH_SIZE));
		to.drawColor(keycap, pressed ? KEYCAP_PRESSED : KEYCAP_FILL);
		to.drawBorder(keycap, KEYCAP_BORDER, 2);
		to.drawText(keycap.center(), FONT_SMALL, KEYCAP_TEXT, ETextAlignment::CENTER,
			glyph.runtimeLabel.empty() ? "--" : glyph.runtimeLabel);
	}
	drawOutlinedText(to, position + Point(PROMPT_TEXT_CENTER_OFFSET, 12), FONT_SMALL, Colors::WHITE, actionText);
}

void BattleControllerActionRadial::closeWithoutCommit()
{
	state.reset();
	confirmPressed = false;
	axisX = axisY = 0.0;
	if(ENGINE->windows().isTopWindow(this))
		WindowBase::close();
}

void BattleControllerActionRadial::releaseConfirm()
{
	confirmPressed = false;
	const auto items = currentItems();
	const auto confirmed = state.releaseConfirm(currentEntries(items));
	if(!confirmed)
	{
		ENGINE->windows().totalRedraw();
		return;
	}

	const auto item = std::ranges::find(items, *confirmed, &BattleControllerActionRadialItem::action);
	if(item == items.end() || !item->enabled || !item->callback)
		return;
	const auto callback = item->callback;
	WindowBase::close();
	ENGINE->input().hapticFeedback();
	callback();
}

bool BattleControllerActionRadial::captureThisKey(EShortcut key)
{
	return key == EShortcut::BATTLE_CONTROLLER_ACTION_RADIAL
		|| key == EShortcut::GLOBAL_ACCEPT
		|| key == EShortcut::GLOBAL_CANCEL
		|| key == EShortcut::MOVE_LEFT
		|| key == EShortcut::MOVE_RIGHT
		|| key == EShortcut::MOVE_UP
		|| key == EShortcut::MOVE_DOWN;
}

void BattleControllerActionRadial::keyPressed(EShortcut key)
{
	if(key == EShortcut::GLOBAL_ACCEPT)
	{
		confirmPressed = state.pressConfirm();
		ENGINE->windows().totalRedraw();
	}
	else if(key == EShortcut::GLOBAL_CANCEL)
		closeWithoutCommit();
}

void BattleControllerActionRadial::keyReleased(EShortcut key)
{
	if(key == EShortcut::GLOBAL_ACCEPT)
		releaseConfirm();
}

void BattleControllerActionRadial::keyCancelled(EShortcut key)
{
	if(key == EShortcut::GLOBAL_ACCEPT)
	{
		confirmPressed = false;
		state.cancelConfirm();
		ENGINE->windows().totalRedraw();
	}
}

void BattleControllerActionRadial::inputModeChanged(InputMode mode)
{
	if(mode != InputMode::CONTROLLER)
		closeWithoutCommit();
}

void BattleControllerActionRadial::showAll(Canvas & to)
{
	const Point center(pos.center().x, pos.center().y - 18);
	const auto items = currentItems();
	const auto selected = state.selectedAction();
	std::array<ColorRGBA, 8> colors;
	colors.fill(SECTOR_EMPTY);
	for(const auto & item : items)
	{
		const bool isSelected = selected == item.action;
		colors[slotForAction(item.action)] = item.enabled
			? (isSelected ? SECTOR_SELECTED : SECTOR_NORMAL)
			: (isSelected ? SECTOR_DISABLED_SELECTED : SECTOR_DISABLED);
	}

	to.drawColorBlended(pos, OVERLAY_COLOR);
	drawRing(to, center, colors);
	for(size_t slot = 0; slot < 8; ++slot)
	{
		const double boundary = slotAngle(slot) - PI / 8.0;
		to.drawLine(polarPoint(center, boundary, RING_INNER_RADIUS),
			polarPoint(center, boundary, RING_OUTER_RADIUS), RING_DIVIDER, RING_DIVIDER);
	}
	drawCircle(to, center, RING_OUTER_RADIUS, RING_OUTLINE, 3);
	drawDisc(to, center, RING_INNER_RADIUS - 3, CENTER_FILL);
	drawCircle(to, center, RING_INNER_RADIUS, RING_OUTLINE, 3);

	for(const auto & item : items)
	{
		std::string label = item.label;
		if(item.action == BattleControllerActionRadialAction::AUTOCOMBAT)
			label += item.active
				? " " + LIBRARY->generaltexth->translate("vcmi.battleWindow.controller.actionRadial.on")
				: " " + LIBRARY->generaltexth->translate("vcmi.battleWindow.controller.actionRadial.off");
		const bool isSelected = selected == item.action;
		drawOutlinedText(to, polarPoint(center, slotAngle(slotForAction(item.action)), LABEL_RADIUS),
			FONT_MEDIUM, item.enabled
				? (isSelected ? Colors::YELLOW : Colors::WHITE)
				: DISABLED_TEXT,
			label);
	}

	drawOutlinedText(to, Point(center.x, center.y - 29), FONT_SMALL, RING_OUTLINE,
		LIBRARY->generaltexth->translate("vcmi.battleWindow.controller.actionRadial.actions"));
	if(selected)
	{
		const auto item = std::ranges::find(items, *selected, &BattleControllerActionRadialItem::action);
		if(item != items.end())
		{
			drawOutlinedText(to, Point(center.x, center.y - 4), FONT_MEDIUM,
				item->enabled ? Colors::WHITE : DISABLED_TEXT, item->label);
			drawOutlinedText(to, Point(center.x, center.y + 22), FONT_SMALL,
				item->enabled ? Colors::WHITE : DISABLED_TEXT,
				item->enabled
					? LIBRARY->generaltexth->translate("vcmi.battleWindow.controller.actionRadial.confirm")
					: LIBRARY->generaltexth->translate("vcmi.battleWindow.controller.actionRadial.unavailable"));
		}
	}
	else
		drawOutlinedText(to, Point(center.x, center.y + 8), FONT_SMALL, Colors::WHITE,
			LIBRARY->generaltexth->translate("vcmi.battleWindow.controller.actionRadial.select"));

	const int promptY = pos.bottom() - 39;
	const Rect promptBar(Point(center.x - 215, promptY - 7), Point(430, 38));
	to.drawColorBlended(promptBar, ColorRGBA(30, 21, 14, 230));
	to.drawBorder(promptBar, RING_OUTLINE, 2);
	drawKeyPrompt(to, Point(center.x - 190, promptY), EShortcut::BATTLE_CONTROLLER_ACTION_RADIAL,
		LIBRARY->generaltexth->translate("vcmi.battleWindow.controller.actionRadial.actions"), false);
	drawKeyPrompt(to, Point(center.x - 42, promptY), EShortcut::GLOBAL_ACCEPT,
		LIBRARY->generaltexth->translate("vcmi.battleWindow.controller.actionRadial.confirm"), confirmPressed);
	drawKeyPrompt(to, Point(center.x + 105, promptY), EShortcut::GLOBAL_CANCEL,
		LIBRARY->generaltexth->translate("vcmi.battleWindow.controller.actionRadial.back"), false);
}

void BattleControllerActionRadial::onScreenResize()
{
	pos = Rect(Point(0, 0), ENGINE->screenDimensions());
}

ControllerAxisRoute BattleControllerActionRadial::controllerAxisMoved(const ControllerAxisEvent & event)
{
	if(vstd::contains(event.actions, EShortcut::CONTROLLER_NAVIGATE_X))
		axisX = event.value;
	if(vstd::contains(event.actions, EShortcut::CONTROLLER_NAVIGATE_Y))
		axisY = event.value;
	if(state.selectDirection(axisX, axisY))
		ENGINE->windows().totalRedraw();
	return ControllerAxisRoute::CAPTURED;
}

void BattleControllerActionRadial::controllerAxisUpdate(uint32_t)
{
}

void BattleControllerActionRadial::controllerAxisReset()
{
	closeWithoutCommit();
}

bool BattleControllerActionRadial::controllerCursorAllowed() const
{
	return false;
}
