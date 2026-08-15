/*
 * BattleNavigationArbiter.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */
#pragma once

/// Gives one native battle navigation source exclusive ownership until that
/// source returns to neutral. A held secondary source takes over only after
/// the current owner releases, so a frame can never move focus twice.
class BattleNavigationArbiter
{
public:
	enum class Source
	{
		NONE,
		HEX,
		UNIT
	};

	void update(Source source, bool sourceActive, bool otherActive);
	void reset();
	Source owner() const;

private:
	Source currentOwner = Source::NONE;
};
