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
	inline const BattleHex doubleWideHead(76);
	inline const BattleHex doubleWideTail = doubleWideHead.cloneInDirection(BattleHex::EDir::RIGHT, false);
	inline const BattleHex primaryAttackFrom = doubleWideHead.cloneInDirection(BattleHex::EDir::TOP_LEFT, false);
	inline const BattleHex alternateAttackFrom = doubleWideHead.cloneInDirection(BattleHex::EDir::BOTTOM_RIGHT, false);

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
