/*
 * BattleFocusTier.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include <optional>

/// Four-tier focus affordance for the controller battle focus (freeze D1:
/// neutral, movable, attackable, illegal). Pure classification; the battle
/// affordance flags are queried by the caller through the existing
/// BattleActionsController / battle legality paths so no rule is duplicated.
class BattleFocusTier
{
public:
	enum class Tier
	{
		NEUTRAL,
		MOVABLE,
		ATTACKABLE,
		ILLEGAL
	};

	/// Draw recipe of a focused hex. Every tier pairs its tint with a
	/// non-color cue so tiers stay distinguishable without hue (freeze F-7).
	struct FocusVisual
	{
		bool shadeOverlay = false; ///< cell shade is drawn under the highlight
		bool borderOverlay = false; ///< cell border is drawn over the highlight
		bool dimmedHighlight = false; ///< highlight uses the dimmed variant
	};

	/// Returns the tier for the focused hex, or nothing when there is no
	/// focus. A target that cannot be committed dominates every other
	/// affordance, attacking outranks moving, and a plain focused hex is
	/// neutral.
	static std::optional<Tier> classify(bool hasFocus, bool movable, bool attackable, bool illegalTarget);

	/// Returns the non-color draw cues of the tier: neutral shows the plain
	/// highlight, movement adds a shade, attacking adds a border overlay and
	/// an illegal target dims the highlight under a shade.
	static FocusVisual visual(Tier tier);
};
