/*
 * BattleFocusHighlights.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../StdInc.h"

#include "BattleFocusHighlights.h"

#include "../GameEngine.h"
#include "../render/ColorFilter.h"
#include "../render/IImage.h"
#include "../render/IRenderHandler.h"
#include "../lib/filesystem/Filesystem.h"
#include "../lib/filesystem/ResourcePath.h"

namespace BattleFocusHighlights
{

const ColorRGBA MOVABLE_TINT = ColorRGBA(90, 200, 90);
const ColorRGBA ATTACKABLE_TINT = ColorRGBA(225, 80, 80);

static const ImagePath OFFICIAL_HIGHLIGHT = ImagePath::builtin("UnitMovementHighlight.PNG");
static const ImagePath CORE_SHADE = ImagePath::builtin("CCELLSHD.BMP");

bool officialHighlightAvailable()
{
	return CResourceHandler::get()->existsResource(OFFICIAL_HIGHLIGHT.addPrefix("SPRITES/"));
}

static const ColorFilter & grayscaleFilter()
{
	static const auto filter = ColorFilter::genMuxerShifter(
		{0.299, 0.587, 0.114, 0}, {0.299, 0.587, 0.114, 0}, {0.299, 0.587, 0.114, 0}, 1);
	return filter;
}

/// Loads a dedicated instance of the official per-hex highlight tinted
/// towards a tier color. Palette index 0 stays untouched so the COLORKEY
/// transparency survives the remap.
static std::shared_ptr<IImage> loadTintedOfficial(const ColorRGBA & color)
{
	auto image = ENGINE->renderHandler().loadImage(OFFICIAL_HIGHLIGHT, EImageBlitMode::COLORKEY);
	const auto tierStrengthFilter = ColorFilter::genRangeShifter(0, 0, 0, color.r / 255.f, color.g / 255.f, color.b / 255.f);
	image->adjustPalette(ColorFilter::genCombined(grayscaleFilter(), tierStrengthFilter), 1);
	return image;
}

/// Core fallback: remaps the semi-transparent cell shade into a bright
/// tinted hex fill so focused hexes stay perceivable without the HD-mod asset
static std::shared_ptr<IImage> loadBrightShadeFill(const ColorRGBA & color)
{
	auto image = ENGINE->renderHandler().loadImage(CORE_SHADE, EImageBlitMode::SIMPLE);
	const auto brightTint = ColorFilter::genRangeShifter(
		0.45f * color.r / 255.f, 0.45f * color.g / 255.f, 0.45f * color.b / 255.f,
		color.r / 255.f, color.g / 255.f, color.b / 255.f);
	image->adjustPalette(ColorFilter::genCombined(grayscaleFilter(), brightTint), 0);
	return image;
}

static std::shared_ptr<IImage> loadDimmedShadeFill()
{
	auto image = ENGINE->renderHandler().loadImage(CORE_SHADE, EImageBlitMode::SIMPLE);
	image->adjustPalette(ColorFilter::genRangeShifter(0, 0, 0, ILLEGAL_LUMINANCE, ILLEGAL_LUMINANCE, ILLEGAL_LUMINANCE), 0);
	return image;
}

TierRender loadTierRender(BattleFocusTier::Tier tier)
{
	if(officialHighlightAvailable())
	{
		switch(tier)
		{
			case BattleFocusTier::Tier::NEUTRAL:
				return {ENGINE->renderHandler().loadImage(OFFICIAL_HIGHLIGHT, EImageBlitMode::COLORKEY), false, false};
			case BattleFocusTier::Tier::MOVABLE:
				return {loadTintedOfficial(MOVABLE_TINT), true, false};
			case BattleFocusTier::Tier::ATTACKABLE:
				return {loadTintedOfficial(ATTACKABLE_TINT), false, true};
			case BattleFocusTier::Tier::ILLEGAL:
			{
				auto image = ENGINE->renderHandler().loadImage(OFFICIAL_HIGHLIGHT, EImageBlitMode::COLORKEY);
				image->adjustPalette(ColorFilter::genRangeShifter(0, 0, 0, ILLEGAL_LUMINANCE, ILLEGAL_LUMINANCE, ILLEGAL_LUMINANCE), 1);
				return {image, true, false};
			}
		}
	}

	// core fallback keeps the same non-color cue algebra: neutral is the
	// plain hover shade, movable adds a bright fill, attackable adds the
	// border overlay, illegal dims the fill under a shade
	switch(tier)
	{
		case BattleFocusTier::Tier::NEUTRAL:
			return {ENGINE->renderHandler().loadImage(CORE_SHADE, EImageBlitMode::SIMPLE), false, false};
		case BattleFocusTier::Tier::MOVABLE:
			return {loadBrightShadeFill(MOVABLE_TINT), false, false};
		case BattleFocusTier::Tier::ATTACKABLE:
			return {loadBrightShadeFill(ATTACKABLE_TINT), false, true};
		case BattleFocusTier::Tier::ILLEGAL:
			return {loadDimmedShadeFill(), true, false};
	}
	return {};
}

}
