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
#include "../render/ImageLocator.h"
#include "../render/IRenderHandler.h"
#include "../renderSDL/ScalableImage.h"
#include "../renderSDL/SDLImage.h"
#include "../lib/filesystem/Filesystem.h"
#include "../lib/filesystem/ResourcePath.h"

#include <SDL_surface.h>

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

/// Loads a dedicated instance of the official per-hex highlight remapped by
/// a color filter. The official asset reaches the client as a 32-bit surface,
/// so the palette based IImage::adjustPalette would silently no-op; render
/// the source once into a private surface and apply the shifter per pixel.
static std::shared_ptr<IImage> loadRemappedOfficial(const ColorFilter & filter)
{
	auto image = ENGINE->renderHandler().loadImage(OFFICIAL_HIGHLIGHT, EImageBlitMode::COLORKEY);

	SDL_Surface * surface = SDL_CreateRGBSurfaceWithFormat(
		0, image->width(), image->height(), 32, SDL_PIXELFORMAT_ARGB8888);
	if(!surface)
		return image;

	image->draw(surface, Point(0, 0), nullptr, 1);

	if(SDL_LockSurface(surface) == 0)
	{
		auto pixels = static_cast<uint32_t *>(surface->pixels);
		for(int y = 0; y < surface->h; ++y)
			for(int x = 0; x < surface->w; ++x)
			{
				uint8_t r = 0;
				uint8_t g = 0;
				uint8_t b = 0;
				uint8_t a = 0;
				SDL_GetRGBA(pixels[y * (surface->pitch / 4) + x], surface->format, &r, &g, &b, &a);
				if(a == 0)
					continue;
				// the draw above alpha-blends onto the empty surface, so the
				// stored RGB is premultiplied; recover straight colors before
				// remapping and store straight again for the final blend
				auto straight = [&](uint8_t channel) -> uint8_t
				{
					return static_cast<uint8_t>(std::min(255, channel * 255 / a));
				};
				const auto shifted = filter.shiftColor(ColorRGBA(straight(r), straight(g), straight(b), a));
				pixels[y * (surface->pitch / 4) + x] = SDL_MapRGBA(surface->format, shifted.r, shifted.g, shifted.b, a);
			}
		SDL_UnlockSurface(surface);
	}

	auto remapped = std::make_shared<SDLImageShared>(surface);
	SDL_FreeSurface(surface); // SDLImageShared keeps its own reference
	// wrap like RenderHandler does so the alpha blit matches production
	auto scalable = std::make_shared<ScalableImageShared>(
		ImageLocator(OFFICIAL_HIGHLIGHT, EImageBlitMode::COLORKEY), remapped);
	return scalable->createImageReference();
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
			{
				// same 0.45 tint floor as the core fallback so the tier color
				// stays perceivable on the mostly-dark official ring art
				const auto tierStrengthFilter = ColorFilter::genRangeShifter(
					0.45f * MOVABLE_TINT.r / 255.f, 0.45f * MOVABLE_TINT.g / 255.f, 0.45f * MOVABLE_TINT.b / 255.f,
					MOVABLE_TINT.r / 255.f, MOVABLE_TINT.g / 255.f, MOVABLE_TINT.b / 255.f);
				return {loadRemappedOfficial(ColorFilter::genCombined(grayscaleFilter(), tierStrengthFilter)), true, false};
			}
			case BattleFocusTier::Tier::ATTACKABLE:
			{
				const auto tierStrengthFilter = ColorFilter::genRangeShifter(
					0.45f * ATTACKABLE_TINT.r / 255.f, 0.45f * ATTACKABLE_TINT.g / 255.f, 0.45f * ATTACKABLE_TINT.b / 255.f,
					ATTACKABLE_TINT.r / 255.f, ATTACKABLE_TINT.g / 255.f, ATTACKABLE_TINT.b / 255.f);
				return {loadRemappedOfficial(ColorFilter::genCombined(grayscaleFilter(), tierStrengthFilter)), false, true};
			}
			case BattleFocusTier::Tier::ILLEGAL:
			{
				const auto dimFilter = ColorFilter::genRangeShifter(0, 0, 0, ILLEGAL_LUMINANCE, ILLEGAL_LUMINANCE, ILLEGAL_LUMINANCE);
				return {loadRemappedOfficial(dimFilter), true, false};
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
