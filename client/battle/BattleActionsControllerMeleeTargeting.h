/*
 * BattleActionsControllerMeleeTargeting.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/battle/BattleHexArray.h"

class CBattleInfoCallback;

namespace battle
{
class Unit;

namespace controller
{
/// Internal BattleActionsController seam. It only orders candidate directions;
/// CBattleInfoCallback remains the authority for reachability and attack rules.
class MeleeTargeting
{
public:
	static BattleHex resolve(const CBattleInfoCallback & battle, const Unit * attacker, const BattleHex & targetHex, BattleHex::EDir preferredDirection, bool allowLongWeapon);
	static BattleHex resolve(const CBattleInfoCallback & battle, const BattleHexArray & availableHexes, const Unit * attacker, const BattleHex & targetHex, BattleHex::EDir preferredDirection, bool allowLongWeapon);
};
}
}
