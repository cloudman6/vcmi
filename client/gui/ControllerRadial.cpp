/*
 * ControllerRadial.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "ControllerRadial.h"

#include "../GameEngine.h"
#include "../eventsSDL/ControllerPromptFamily.h"
#include "../eventsSDL/InputHandler.h"
#include "../gui/ShortcutHandler.h"
#include "../gui/TextAlignment.h"
#include "../gui/WindowHandler.h"
#include "../render/Canvas.h"
#include "../render/Colors.h"
#include "../render/EFont.h"
#include "../render/IFont.h"
#include "../render/IImage.h"
#include "../render/IRenderHandler.h"
#include "../windows/CMessage.h"

#include "../../lib/GameLibrary.h"
#include "../../lib/texts/CGeneralTextHandler.h"

namespace
{
constexpr double PI = 3.14159265358979323846;
constexpr double SECTOR_HALF_GAP_ANGLE = 0.018;
constexpr size_t RING_FADE_BANDS = 24;
constexpr int RING_OUTER_RADIUS = 176;
constexpr int RING_INNER_RADIUS = 70;
constexpr int ITEM_RADIUS = 126;
constexpr int RING_FADE_START_RADIUS = ITEM_RADIUS + 8;
constexpr int RING_FADE_END_RADIUS = RING_OUTER_RADIUS - 12;
constexpr int SELECTED_ITEM_RADIUS = ITEM_RADIUS + 6;
constexpr int WHEEL_CANVAS_MARGIN = 8;
constexpr int WHEEL_CANVAS_SIZE = 2 * (RING_OUTER_RADIUS + WHEEL_CANVAS_MARGIN);
constexpr int GLYPH_SIZE = 24;
constexpr int GLYPH_TEXT_SPACING = 4;
constexpr int TEXT_OUTLINE_WIDTH = 1;
constexpr int DETAIL_BAR_HEIGHT = 146;
constexpr int DETAIL_BAR_WHEEL_GAP = 8;
constexpr int DETAIL_BAR_BOTTOM_MARGIN = 4;
constexpr int DETAIL_BAR_FADE_WIDTH = 120;
constexpr size_t DETAIL_BAR_FADE_BANDS = 24;
constexpr int DETAIL_TEXT_PADDING = 32;
constexpr int PREVIEW_CENTER_OFFSET = 290;
constexpr double PREVIEW_SCALE = 0.66;
constexpr uint32_t PAGE_TRANSITION_DURATION = 180;
constexpr ColorRGBA OVERLAY_COLOR(10, 8, 5, 64);
constexpr ColorRGBA SECTOR_EMPTY(70, 63, 53, 185);
constexpr ColorRGBA SECTOR_NORMAL(132, 91, 38, 205);
constexpr ColorRGBA SECTOR_SELECTED(226, 183, 69, 240);
constexpr ColorRGBA SECTOR_DISABLED(102, 102, 98, 210);
constexpr ColorRGBA SECTOR_DISABLED_SELECTED(158, 146, 108, 235);
constexpr ColorRGBA DETAIL_FILL(30, 21, 14, 180);
constexpr ColorRGBA DETAIL_LINE(218, 178, 82, 145);
constexpr ColorRGBA DISABLED_TEXT(192, 188, 174, 255);
const std::string IDLE_STICK_GLYPH = "controllerActionBar/leftstick-normal.png";

double slotAngle(size_t slot, size_t slotCount)
{
	return -PI / 2.0 - static_cast<double>(slot) * 2.0 * PI / static_cast<double>(slotCount);
}

Point polarPoint(Point center, double angle, int radius)
{
	return Point(center.x + static_cast<int>(std::lround(std::cos(angle) * radius)), center.y + static_cast<int>(std::lround(std::sin(angle) * radius)));
}

size_t sectorAtPoint(int x, int y, size_t slotCount)
{
	double counterclockwiseFromNorth = std::atan2(-static_cast<double>(x), -static_cast<double>(y));
	if(counterclockwiseFromNorth < 0.0)
		counterclockwiseFromNorth += 2.0 * PI;
	const double sectorAngle = 2.0 * PI / static_cast<double>(slotCount);
	return static_cast<size_t>(std::floor((counterclockwiseFromNorth + sectorAngle / 2.0) / sectorAngle)) % slotCount;
}

struct RingRun
{
	int y;
	int xBegin;
	int xEnd;
	size_t slot;
	uint8_t opacity;
};

const std::vector<RingRun> & ringRuns(size_t slotCount)
{
	static std::map<size_t, std::vector<RingRun>> cache;
	auto found = cache.find(slotCount);
	if(found != cache.end())
		return found->second;

	auto runs = [slotCount]()
	{
		std::vector<RingRun> result;
		const int innerSquared = RING_INNER_RADIUS * RING_INNER_RADIUS;
		const int outerSquared = RING_OUTER_RADIUS * RING_OUTER_RADIUS;
		for(int y = -RING_OUTER_RADIUS; y <= RING_OUTER_RADIUS; ++y)
		{
			std::optional<size_t> activeSlot;
			std::optional<size_t> activeBand;
			int runStart = 0;
			for(int x = -RING_OUTER_RADIUS; x <= RING_OUTER_RADIUS + 1; ++x)
			{
				std::optional<size_t> slot;
				std::optional<size_t> band;
				const int radiusSquared = x * x + y * y;
				if(x <= RING_OUTER_RADIUS && radiusSquared >= innerSquared && radiusSquared <= outerSquared)
				{
					const size_t candidate = sectorAtPoint(x, y, slotCount);
					const double pointAngle = std::atan2(static_cast<double>(y), static_cast<double>(x));
					const double distanceFromCenter = std::abs(std::remainder(pointAngle - slotAngle(candidate, slotCount), 2.0 * PI));
					const double halfSector = PI / static_cast<double>(slotCount);
					if(distanceFromCenter <= halfSector - SECTOR_HALF_GAP_ANGLE)
					{
						slot = candidate;
						const double radius = std::sqrt(static_cast<double>(radiusSquared));
						const double fadeProgress =
							std::clamp((radius - RING_FADE_START_RADIUS) / static_cast<double>(RING_FADE_END_RADIUS - RING_FADE_START_RADIUS), 0.0, 1.0);
						band = static_cast<size_t>(std::lround(fadeProgress * static_cast<double>(RING_FADE_BANDS - 1)));
					}
				}
				if(slot == activeSlot && band == activeBand)
					continue;
				if(activeSlot)
				{
					const double progress = static_cast<double>(*activeBand) / static_cast<double>(RING_FADE_BANDS - 1);
					const double smoothProgress = progress * progress * (3.0 - 2.0 * progress);
					const auto opacity = static_cast<uint8_t>(std::lround(255.0 * (1.0 - smoothProgress)));
					result.push_back({y, runStart, x - 1, *activeSlot, opacity});
				}
				activeSlot = slot;
				activeBand = band;
				runStart = x;
			}
		}
		return result;
	}();
	return cache.emplace(slotCount, std::move(runs)).first->second;
}

void drawRing(Canvas & to, Point center, const std::vector<ColorRGBA> & colors)
{
	for(const auto & run : ringRuns(colors.size()))
	{
		ColorRGBA color = colors[run.slot];
		color.a = static_cast<uint8_t>(static_cast<uint16_t>(color.a) * run.opacity / 255);
		to.drawColor(Rect(Point(center.x + run.xBegin, center.y + run.y), Point(run.xEnd - run.xBegin + 1, 1)), color);
	}
}

void drawOutlinedText(Canvas & to, Point position, EFonts font, const ColorRGBA & color, const std::string & text)
{
	to.drawText(position + Point(-TEXT_OUTLINE_WIDTH, 0), font, Colors::BLACK, ETextAlignment::CENTER, text);
	to.drawText(position + Point(TEXT_OUTLINE_WIDTH, 0), font, Colors::BLACK, ETextAlignment::CENTER, text);
	to.drawText(position + Point(0, -TEXT_OUTLINE_WIDTH), font, Colors::BLACK, ETextAlignment::CENTER, text);
	to.drawText(position + Point(0, TEXT_OUTLINE_WIDTH), font, Colors::BLACK, ETextAlignment::CENTER, text);
	to.drawText(position, font, color, ETextAlignment::CENTER, text);
}

void drawDetailBar(Canvas & to, const Rect & bar)
{
	const int left = bar.left();
	const int right = bar.right();
	const int fadeWidth = std::min(DETAIL_BAR_FADE_WIDTH, (right - left) / 2);
	for(size_t band = 0; band < DETAIL_BAR_FADE_BANDS; ++band)
	{
		const int outerOffset = static_cast<int>(band * fadeWidth / DETAIL_BAR_FADE_BANDS);
		const int innerOffset = static_cast<int>((band + 1) * fadeWidth / DETAIL_BAR_FADE_BANDS);
		const double progress = static_cast<double>(band) / static_cast<double>(DETAIL_BAR_FADE_BANDS - 1);
		const double smoothProgress = progress * progress * (3.0 - 2.0 * progress);
		ColorRGBA fill = DETAIL_FILL;
		fill.a = static_cast<uint8_t>(std::lround(static_cast<double>(DETAIL_FILL.a) * smoothProgress));
		to.drawColorBlended(Rect(Point(left + outerOffset, bar.top()), Point(innerOffset - outerOffset, bar.h)), fill);
		to.drawColorBlended(Rect(Point(right - innerOffset, bar.top()), Point(innerOffset - outerOffset, bar.h)), fill);
	}
	to.drawColorBlended(Rect(Point(left + fadeWidth, bar.top()), Point(right - left - 2 * fadeWidth, bar.h)), DETAIL_FILL);

	ColorRGBA transparentLine = DETAIL_LINE;
	transparentLine.a = ColorRGBA::ALPHA_TRANSPARENT;
	const int lineLeft = bar.left();
	const int lineRight = bar.right();
	const int center = (lineLeft + lineRight) / 2;
	const int y = bar.top();
	to.drawLine(Point(lineLeft, y), Point(center - 1, y), transparentLine, DETAIL_LINE);
	to.drawLine(Point(center, y), Point(lineRight, y), DETAIL_LINE, transparentLine);
}
}

ControllerRadial::ControllerRadial(
	ItemProvider provider,
	BoundsProvider bounds,
	EShortcut shortcut,
	size_t slots,
	std::optional<ControllerRadialPageShortcuts> pages,
	std::vector<EShortcut> captured,
	std::string selectText,
	std::string closeText)
	: WindowBase(KEYBOARD | TIME | INPUT_MODE_CHANGE),
	  itemProvider(std::move(provider)),
	  boundsProvider(std::move(bounds)),
	  openingShortcut(shortcut),
	  slotCount(std::max<size_t>(1, slots)),
	  pageShortcuts(pages),
	  capturedShortcuts(std::move(captured)),
	  selectLabel(std::move(selectText)),
	  closeLabel(std::move(closeText)),
	  state(slotCount)
{
	onScreenResize();
	state.open(currentEntries(currentItems()));
}

std::vector<ControllerRadialItem> ControllerRadial::currentItems() const
{
	return itemProvider ? itemProvider() : std::vector<ControllerRadialItem>{};
}

std::vector<ControllerRadialEntry> ControllerRadial::currentEntries(const std::vector<ControllerRadialItem> & items) const
{
	std::vector<ControllerRadialEntry> result;
	result.reserve(items.size());
	for(const auto & item : items)
		result.push_back({item.id, item.enabled, item.slot, item.page});
	return result;
}

const std::shared_ptr<IImage> & ControllerRadial::promptSprite(const std::string & path)
{
	auto cached = promptSpriteCache.find(path);
	if(cached == promptSpriteCache.end())
	{
		auto sprite = ENGINE->renderHandler().loadImage(ImagePath::builtin(path), EImageBlitMode::COLORKEY);
		cached = promptSpriteCache.emplace(path, std::move(sprite)).first;
	}
	return cached->second;
}

const std::shared_ptr<IImage> & ControllerRadial::itemSprite(const std::string & animation, int32_t frame)
{
	const auto key = std::make_pair(animation, frame);
	auto cached = itemSpriteCache.find(key);
	if(cached == itemSpriteCache.end())
	{
		auto sprite = ENGINE->renderHandler().loadImage(AnimationPath::builtin(animation), frame, 0, EImageBlitMode::COLORKEY);
		cached = itemSpriteCache.emplace(key, std::move(sprite)).first;
	}
	return cached->second;
}

const std::shared_ptr<IImage> & ControllerRadial::itemImage(const std::string & path)
{
	auto cached = itemImageCache.find(path);
	if(cached == itemImageCache.end())
	{
		auto sprite = ENGINE->renderHandler().loadImage(ImagePath::builtin(path), EImageBlitMode::COLORKEY);
		cached = itemImageCache.emplace(path, std::move(sprite)).first;
	}
	return cached->second;
}

void ControllerRadial::drawKeyPrompt(Canvas & to, Point position, EShortcut shortcut, const std::string & actionText, bool pressed, bool disabled)
{
	const auto bindings = ENGINE->shortcuts().getJoystickButtonBindings(shortcut);
	const auto family = ENGINE->input().getActiveControllerPromptFamily();
	const std::string binding = bindings.size() == 1 ? bindings.front() : "";
	auto stateValue = ControllerPrompt::State::NORMAL;
	if(disabled)
		stateValue = ControllerPrompt::State::DISABLED;
	else if(pressed)
		stateValue = ControllerPrompt::State::PRESSED;
	const auto spritePath = ControllerPrompt::faceButtonSprite(family, binding, stateValue);
	if(spritePath)
		to.draw(promptSprite(*spritePath), position);
	else
	{
		to.drawText(
			position + Point(GLYPH_SIZE / 2, GLYPH_SIZE / 2),
			FONT_SMALL,
			disabled ? DISABLED_TEXT : Colors::WHITE,
			ETextAlignment::CENTER,
			binding.empty() ? "--" : ControllerPrompt::buttonLabel(family, binding)
		);
	}

	const auto & font = ENGINE->renderHandler().loadFont(FONT_SMALL);
	const int textWidth = static_cast<int>(font->getStringWidth(actionText)) + TEXT_OUTLINE_WIDTH * 2;
	drawOutlinedText(to, position + Point(GLYPH_SIZE + GLYPH_TEXT_SPACING + textWidth / 2, GLYPH_SIZE / 2), FONT_SMALL,
		disabled ? DISABLED_TEXT : Colors::WHITE, actionText);
}

void ControllerRadial::drawWheel(Canvas & to, const std::vector<ControllerRadialItem> & items, size_t page, Point center, double scale, bool active)
{
	const auto selected = active ? state.selectedItem() : std::optional<ControllerRadialItemId>{};
	std::vector<ColorRGBA> colors(slotCount, SECTOR_EMPTY);
	for(const auto & item : items)
	{
		if(item.page != page || item.slot >= colors.size())
			continue;
		const bool isSelected = selected == item.id;
		colors[item.slot] = item.enabled ? (isSelected ? SECTOR_SELECTED : SECTOR_NORMAL) : (isSelected ? SECTOR_DISABLED_SELECTED : SECTOR_DISABLED);
	}

	Canvas wheelCanvas(Point(WHEEL_CANVAS_SIZE, WHEEL_CANVAS_SIZE), CanvasScalingPolicy::AUTO);
	wheelCanvas.applyTransparency(true);
	const Point wheelCenter(WHEEL_CANVAS_SIZE / 2, WHEEL_CANVAS_SIZE / 2);
	drawRing(wheelCanvas, wheelCenter, colors);

	for(const auto & item : items)
	{
		if(item.page != page || item.slot >= slotCount || (item.iconImage.empty() && item.iconAnimation.empty()))
			continue;
		const auto & icon = item.iconImage.empty() ? itemSprite(item.iconAnimation, item.iconFrame) : itemImage(item.iconImage);
		const bool isSelected = selected == item.id;
		const Point iconCenter = polarPoint(wheelCenter, slotAngle(item.slot, slotCount), isSelected ? SELECTED_ITEM_RADIUS : ITEM_RADIUS);
		icon->setAlpha(item.enabled ? 255 : 170);
		wheelCanvas.draw(icon, iconCenter - icon->dimensions() / 2);
		icon->setAlpha(255);
	}

	const Point dimensions(static_cast<int>(std::lround(WHEEL_CANVAS_SIZE * scale)), static_cast<int>(std::lround(WHEEL_CANVAS_SIZE * scale)));
	to.drawScaled(wheelCanvas, center - dimensions / 2, dimensions);
}

void ControllerRadial::drawPageSwitchPrompt(Canvas & to, Point promptCenter, EShortcut shortcut, const std::string & arrow)
{
	if(!pageShortcuts)
		return;
	const auto previousBindings = ENGINE->shortcuts().getJoystickButtonBindings(pageShortcuts->previous);
	const auto nextBindings = ENGINE->shortcuts().getJoystickButtonBindings(pageShortcuts->next);
	const auto family = ENGINE->input().getActiveControllerPromptFamily();
	const auto pairSprite = ControllerPrompt::shoulderPairSprite(family, previousBindings, nextBindings);
	if(pairSprite)
	{
		const auto & sprite = promptSprite(*pairSprite);
		const bool previous = shortcut == pageShortcuts->previous;
		const int glyphWidth = sprite->dimensions().x / 2;
		const Rect source(previous ? 0 : glyphWidth, 0, glyphWidth, sprite->dimensions().y);
		to.draw(sprite, promptCenter - Point(glyphWidth / 2, sprite->dimensions().y / 2), source);
	}
	else
	{
		const auto & bindings = shortcut == pageShortcuts->previous ? previousBindings : nextBindings;
		const std::string label = bindings.size() == 1 ? ControllerPrompt::buttonLabel(family, bindings.front()) : "--";
		drawOutlinedText(to, promptCenter, FONT_SMALL, Colors::WHITE, label);
	}
	drawOutlinedText(to, promptCenter + Point(0, 25), FONT_MEDIUM, Colors::WHITE, arrow);
}

void ControllerRadial::closeWithoutCommit()
{
	state.reset();
	confirmPressed = false;
	axisX = axisY = 0.0;
	if(ENGINE->windows().isTopWindow(this))
		WindowBase::close();
}

void ControllerRadial::releaseConfirm()
{
	confirmPressed = false;
	const auto items = currentItems();
	const auto confirmed = state.releaseConfirm(currentEntries(items));
	if(!confirmed)
	{
		ENGINE->windows().totalRedraw();
		return;
	}

	const auto item = std::ranges::find(items, *confirmed, &ControllerRadialItem::id);
	if(item == items.end() || !item->enabled || !item->callback)
		return;
	const auto callback = item->callback;
	WindowBase::close();
	ENGINE->input().hapticFeedback();
	callback();
}

bool ControllerRadial::captureThisKey(EShortcut key)
{
	return key == openingShortcut || vstd::contains(capturedShortcuts, key)
		|| (pageShortcuts && (key == pageShortcuts->previous || key == pageShortcuts->next)) || key == EShortcut::GLOBAL_ACCEPT
		|| key == EShortcut::GLOBAL_CANCEL || key == EShortcut::MOUSE_LEFT || key == EShortcut::MOUSE_RIGHT || key == EShortcut::MOVE_LEFT || key == EShortcut::MOVE_RIGHT
		|| key == EShortcut::MOVE_UP || key == EShortcut::MOVE_DOWN;
}

void ControllerRadial::keyPressed(EShortcut key)
{
	if(key == EShortcut::GLOBAL_ACCEPT)
	{
		if(pageTransitionFrom)
			return;
		confirmPressed = state.pressConfirm(currentEntries(currentItems()));
		ENGINE->windows().totalRedraw();
	}
	else if(key == EShortcut::GLOBAL_CANCEL)
		closeWithoutCommit();
	else if(pageShortcuts && key == pageShortcuts->previous)
	{
		if(!pageTransitionFrom)
			changePage(-1);
	}
	else if(pageShortcuts && key == pageShortcuts->next)
	{
		if(!pageTransitionFrom)
			changePage(1);
	}
}

void ControllerRadial::keyReleased(EShortcut key)
{
	if(key == EShortcut::GLOBAL_ACCEPT)
		releaseConfirm();
}

void ControllerRadial::inputModeChanged(InputMode mode)
{
	if(mode != InputMode::CONTROLLER)
		closeWithoutCommit();
}

void ControllerRadial::tick(uint32_t msPassed)
{
	if(!pageTransitionFrom)
		return;
	pageTransitionElapsed = std::min(pageTransitionElapsed + msPassed, PAGE_TRANSITION_DURATION);
	if(pageTransitionElapsed >= PAGE_TRANSITION_DURATION)
		pageTransitionFrom.reset();
	ENGINE->windows().totalRedraw();
}

void ControllerRadial::changePage(int offset)
{
	const size_t previousPage = state.currentPage();
	if(!state.changePage(offset))
		return;

	pageTransitionFrom = previousPage;
	pageTransitionElapsed = 0;
	confirmPressed = false;
	axisX = axisY = 0.0;
	ENGINE->windows().totalRedraw();
}

void ControllerRadial::showAll(Canvas & to)
{
	const Point center(pos.center().x, pos.center().y - 18);
	const int detailBarTop = std::min(
		center.y + RING_OUTER_RADIUS + DETAIL_BAR_WHEEL_GAP,
		pos.bottom() - DETAIL_BAR_HEIGHT - DETAIL_BAR_BOTTOM_MARGIN);
	const Rect requestedBounds = boundsProvider();
	const int detailBarLeft = std::clamp(requestedBounds.left(), pos.left(), pos.right());
	const int detailBarRight = std::clamp(requestedBounds.right(), detailBarLeft, pos.right());
	const int detailBarBottom = std::clamp(requestedBounds.bottom(), detailBarTop + 1, pos.bottom());
	const Rect detailBar(Point(detailBarLeft, detailBarTop), Point(detailBarRight - detailBarLeft, detailBarBottom - detailBarTop));
	const int promptY = detailBar.bottom() - GLYPH_SIZE - 4;
	const auto items = currentItems();
	const auto selected = state.selectedItem();
	const auto selectedItem = selected ? std::ranges::find(items, *selected, &ControllerRadialItem::id) : items.end();
	const bool confirmDisabled = selectedItem == items.end() || !selectedItem->enabled;

	to.drawColorBlended(pos, OVERLAY_COLOR);
	const size_t currentPage = state.currentPage();
	if(pageTransitionFrom)
	{
		const bool movingForward = currentPage > *pageTransitionFrom;
		const double rawProgress = static_cast<double>(pageTransitionElapsed) / PAGE_TRANSITION_DURATION;
		const double progress = rawProgress * rawProgress * (3.0 - 2.0 * rawProgress);
		const int direction = movingForward ? 1 : -1;
		const Point fromCenter(center.x - static_cast<int>(std::lround(direction * PREVIEW_CENTER_OFFSET * progress)), center.y);
		const Point toCenter(center.x + static_cast<int>(std::lround(direction * PREVIEW_CENTER_OFFSET * (1.0 - progress))), center.y);
		const double fromScale = 1.0 - (1.0 - PREVIEW_SCALE) * progress;
		const double toScale = PREVIEW_SCALE + (1.0 - PREVIEW_SCALE) * progress;
		drawWheel(to, items, *pageTransitionFrom, fromCenter, fromScale, false);
		drawWheel(to, items, currentPage, toCenter, toScale, true);
	}
	else
	{
		if(pageShortcuts && currentPage > 0)
		{
			const Point previousCenter = center - Point(PREVIEW_CENTER_OFFSET, 0);
			drawWheel(to, items, currentPage - 1, previousCenter, PREVIEW_SCALE, false);
			const int gapCenter = (previousCenter.x + static_cast<int>(std::lround(RING_OUTER_RADIUS * PREVIEW_SCALE)) + center.x - RING_OUTER_RADIUS) / 2;
			drawPageSwitchPrompt(to, Point(gapCenter, center.y - 82), pageShortcuts->previous, "<");
		}
		drawWheel(to, items, currentPage, center, 1.0, true);
		if(pageShortcuts && currentPage + 1 < state.pageCount())
		{
			const Point nextCenter = center + Point(PREVIEW_CENTER_OFFSET, 0);
			drawWheel(to, items, currentPage + 1, nextCenter, PREVIEW_SCALE, false);
			const int gapCenter = (center.x + RING_OUTER_RADIUS + nextCenter.x - static_cast<int>(std::lround(RING_OUTER_RADIUS * PREVIEW_SCALE))) / 2;
			drawPageSwitchPrompt(to, Point(gapCenter, center.y - 82), pageShortcuts->next, ">");
		}
	}
	if(selected)
	{
		const auto item = std::ranges::find(items, *selected, &ControllerRadialItem::id);
		if(item != items.end())
		{
			drawOutlinedText(to, Point(center.x, center.y - 4), FONT_MEDIUM, item->enabled ? Colors::WHITE : DISABLED_TEXT, item->label);
		}
	}
	else if(!pageTransitionFrom)
	{
		const auto & stick = promptSprite(IDLE_STICK_GLYPH);
		to.draw(stick, center - stick->dimensions() / 2);
	}

	drawDetailBar(to, detailBar);
	if(selected)
	{
		const auto item = std::ranges::find(items, *selected, &ControllerRadialItem::id);
		if(item != items.end())
		{
			const bool hasUnavailableReason = !item->unavailableReason.empty();
			const int labelY = detailBar.top() + (hasUnavailableReason ? 12 : 15);
			const int reasonY = promptY - 8;
			const int compressedStep = std::max(1, (reasonY - labelY) / 3);
			const int summaryY = hasUnavailableReason ? labelY + compressedStep : detailBar.top() + 34;
			const int detailY = hasUnavailableReason ? labelY + 2 * compressedStep : detailBar.top() + 52;
			drawOutlinedText(to, Point(center.x, labelY), FONT_MEDIUM, item->enabled ? Colors::WHITE : DISABLED_TEXT, item->label);
			if(!item->detailSummary.empty())
				drawOutlinedText(to, Point(center.x, summaryY), FONT_SMALL, item->enabled ? Colors::WHITE : DISABLED_TEXT, item->detailSummary);

			const int detailTextWidth = detailBar.w - 2 * DETAIL_TEXT_PADDING;
			const auto detailLines = CMessage::breakText(item->detailDescription, detailTextWidth, FONT_SMALL);
			const size_t maxDetailLines = hasUnavailableReason
				? 1
				: 1 + static_cast<size_t>(std::max(0, promptY - detailY - 10) / 13);
			for(size_t line = 0; line < std::min(maxDetailLines, detailLines.size()); ++line)
				drawOutlinedText(
					to,
					Point(center.x, detailY + static_cast<int>(line) * 13),
					FONT_SMALL,
					item->enabled ? Colors::WHITE : DISABLED_TEXT,
					detailLines[line]
				);
			if(hasUnavailableReason)
			{
				const auto reasonLines = CMessage::breakText(item->unavailableReason, detailTextWidth, FONT_SMALL);
				if(!reasonLines.empty())
					drawOutlinedText(to, Point(center.x, reasonY), FONT_SMALL, SECTOR_SELECTED, reasonLines.front());
			}
		}
	}

	drawKeyPrompt(
		to,
		Point(center.x - 115, promptY),
		EShortcut::GLOBAL_ACCEPT,
		selectLabel,
		confirmPressed,
		confirmDisabled
	);
	drawKeyPrompt(
		to, Point(center.x + 45, promptY), EShortcut::GLOBAL_CANCEL, closeLabel, false
	);
}

void ControllerRadial::onScreenResize()
{
	pos = Rect(Point(0, 0), ENGINE->screenDimensions());
}

bool ControllerRadial::usesNativeControllerAxis() const
{
	return true;
}

bool ControllerRadial::controllerAxisMoved(int, const std::vector<EShortcut> & actions, double value)
{
	if(vstd::contains(actions, EShortcut::MOUSE_CURSOR_X))
		axisX = value;
	if(vstd::contains(actions, EShortcut::MOUSE_CURSOR_Y))
		axisY = value;
	if(!pageTransitionFrom && state.selectDirection(axisX, axisY))
		ENGINE->windows().totalRedraw();
	return true;
}

void ControllerRadial::controllerInputReset()
{
	closeWithoutCommit();
}
