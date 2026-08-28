/*
 * BattleUnitSelector.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */
#pragma once

#include "../../lib/battle/BattleHex.h"

/// Typed battlefield unit candidate used by controller directional browsing.
/// Battle state is responsible for producing current eligibility; selection
/// only compares stable identity and occupied geometry.
struct BattleUnitNavigationCandidate
{
	uint32_t unitId = 0;
	BattleHex headHex = BattleHex::INVALID;
	BattleHex tailHex = BattleHex::INVALID;
};

/// Stateless directional selection for the battle controller. It deliberately
/// has no battle, input, pointer, rendering or action-legality dependencies.
class BattleUnitSelector
{
public:
	/// Selects the best eligible unit strictly in front of the supplied vector.
	/// Alignment outranks distance; stable unit id breaks geometric ties.
	static std::optional<BattleUnitNavigationCandidate> select(
		const std::vector<BattleUnitNavigationCandidate> & candidates,
		const BattleHex & focusedHex,
		double directionX,
		double directionY);
};
