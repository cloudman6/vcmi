/*
 * BattleActionsControllerMeleeTargeting.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "BattleActionsControllerMeleeTargeting.h"

#include "../../lib/battle/CBattleInfoCallback.h"
#include "../../lib/battle/Unit.h"

namespace battle::controller
{
namespace
{
BattleHex resolveForDirection(const CBattleInfoCallback & battle, const BattleHexArray & availableHexes, const Unit * attacker, const BattleHex & targetHex, BattleHex::EDir direction, bool allowLongWeapon)
{
	if(!battle.battleCanAttackHex(availableHexes, attacker, targetHex, direction))
		return BattleHex::INVALID;

	const BattleHex approachHex = battle.fromWhichHexAttack(attacker, targetHex, direction, allowLongWeapon);
	return availableHexes.contains(approachHex) ? approachHex : BattleHex::INVALID;
}
}

bool MeleeActionDecision::isLegal() const
{
	return approachHex.isValid();
}

BattleHex MeleeTargeting::resolve(const CBattleInfoCallback & battle, const Unit * attacker, const BattleHex & targetHex, BattleHex::EDir preferredDirection, bool allowLongWeapon)
{
	if(!attacker || !targetHex.isValid())
		return BattleHex::INVALID;

	return resolve(battle, battle.battleGetAvailableHexes(attacker, false), attacker, targetHex, preferredDirection, allowLongWeapon);
}

BattleHex MeleeTargeting::resolve(const CBattleInfoCallback & battle, const BattleHexArray & availableHexes, const Unit * attacker, const BattleHex & targetHex, BattleHex::EDir preferredDirection, bool allowLongWeapon)
{
	if(!attacker || !targetHex.isValid())
		return BattleHex::INVALID;

	BattleHex approachHex = resolveForDirection(battle, availableHexes, attacker, targetHex, preferredDirection, allowLongWeapon);
	if(approachHex.isValid())
		return approachHex;

	for(int direction = 0; direction < 8; ++direction)
	{
		approachHex = resolveForDirection(battle, availableHexes, attacker, targetHex, static_cast<BattleHex::EDir>(direction), allowLongWeapon);
		if(approachHex.isValid())
			return approachHex;
	}

	return BattleHex::INVALID;
}
}
