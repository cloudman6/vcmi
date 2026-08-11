/*
 * BattleFocusHighlights.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/Color.h"
#include "BattleFocusTier.h"

#include <memory>

class IImage;

/// Loads the per-hex focus highlight variants of the controller battle focus
/// tiers (freeze D1). When the official HD-mod per-hex highlight asset is
/// installed the variants are dedicated instances of it with the tier tint
/// applied; otherwise the recipe falls back to core H3 cell assets (owner
/// decision 2026-08-11: official-first with core fallback). The tint values
/// here are the single source of the frozen tier colors.
namespace BattleFocusHighlights
{
	/// Tier tints applied to the per-hex highlight asset
	extern const ColorRGBA MOVABLE_TINT;
	extern const ColorRGBA ATTACKABLE_TINT;
	/// luminance factor of the dimmed variant used for illegal targets
	constexpr float ILLEGAL_LUMINANCE = 0.4f;

	/// Draw recipe of one focus tier: the main per-hex image plus the
	/// non-color overlays (freeze F-7)
	struct TierRender
	{
		std::shared_ptr<IImage> highlight;
		bool shadeOverlay = false;
		bool borderOverlay = false;
	};

	/// true when the official per-hex highlight asset is installed
	bool officialHighlightAvailable();

	/// Resolves the draw recipe of a tier for the current asset environment
	TierRender loadTierRender(BattleFocusTier::Tier tier);
}
