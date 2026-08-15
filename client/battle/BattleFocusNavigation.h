/*
 * BattleFocusNavigation.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "BattleFocusModel.h"

class BattleFocusNavigation
{
public:
	enum class Axis
	{
		HORIZONTAL,
		VERTICAL
	};

	explicit BattleFocusNavigation(BattleFocusModel & model);

	void updateAxis(int instanceId, Axis axis, double value);
	bool update(uint32_t msPassed);
	void reset();

private:
	BattleFocusModel & model;
	int activeInstance = -1;
	double axisX = 0.0;
	double axisY = 0.0;
	BattleHex::EDir direction = BattleHex::NONE;
	bool initialPending = false;
	uint32_t elapsed = 0;

	BattleHex::EDir quantizedDirection() const;
	bool moveFocusWithVerticalEdgeFallback();
};
