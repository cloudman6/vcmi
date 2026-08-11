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

	/// Returns the tier for the focused hex, or nothing when there is no
	/// focus. A target that cannot be committed dominates every other
	/// affordance, attacking outranks moving, and a plain focused hex is
	/// neutral.
	static std::optional<Tier> classify(bool hasFocus, bool movable, bool attackable, bool illegalTarget);
};
