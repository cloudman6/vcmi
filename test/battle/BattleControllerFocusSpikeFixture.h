/*
 * BattleControllerFocusSpikeFixture.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/battle/BattleHex.h"

#include <array>

namespace BattleControllerFocusSpikeFixture
{
	inline const BattleHex initialFocus(77);

	constexpr std::array<BattleHex::EDir, 6> sixWayDirections =
	{
		BattleHex::EDir::TOP_LEFT,
		BattleHex::EDir::TOP_RIGHT,
		BattleHex::EDir::RIGHT,
		BattleHex::EDir::BOTTOM_RIGHT,
		BattleHex::EDir::BOTTOM_LEFT,
		BattleHex::EDir::LEFT,
	};
}
