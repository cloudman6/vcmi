/*
 * BattleNavigationArbiter.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "../StdInc.h"

#include "BattleNavigationArbiter.h"

void BattleNavigationArbiter::update(Source source, bool sourceActive, bool otherActive)
{
	if(currentOwner == Source::NONE)
	{
		if(sourceActive)
			currentOwner = source;
		return;
	}

	if(currentOwner != source || sourceActive)
		return;

	if(otherActive)
		currentOwner = source == Source::HEX ? Source::UNIT : Source::HEX;
	else
		currentOwner = Source::NONE;
}

void BattleNavigationArbiter::reset()
{
	currentOwner = Source::NONE;
}

BattleNavigationArbiter::Source BattleNavigationArbiter::owner() const
{
	return currentOwner;
}
