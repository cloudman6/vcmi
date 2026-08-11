/*
 * BattleFocusTier.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../StdInc.h"

#include "BattleFocusTier.h"

std::optional<BattleFocusTier::Tier> BattleFocusTier::classify(bool hasFocus, bool movable, bool attackable, bool illegalTarget)
{
	if(!hasFocus)
		return std::nullopt;

	if(illegalTarget)
		return Tier::ILLEGAL;

	if(attackable)
		return Tier::ATTACKABLE;

	if(movable)
		return Tier::MOVABLE;

	return Tier::NEUTRAL;
}
