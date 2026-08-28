/*
 * BattleUnitNavigation.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */
#pragma once

#include "BattleFocusModel.h"
#include "BattleUnitSelector.h"

/// Samples the independent unit-navigation axes and applies deterministic
/// selections to the shared battle focus model.
class BattleUnitNavigation
{
public:
	enum class Axis
	{
		HORIZONTAL,
		VERTICAL
	};

	using CandidateProvider = std::function<std::vector<BattleUnitNavigationCandidate>()>;

	BattleUnitNavigation(BattleFocusModel & model, CandidateProvider candidateProvider);

	void updateAxis(int instanceId, Axis axis, double value);
	bool update(uint32_t msPassed);
	void reset();
	bool isActive() const;

private:
	BattleFocusModel & model;
	CandidateProvider candidateProvider;
	int activeInstance = -1;
	double axisX = 0.0;
	double axisY = 0.0;
	double directionX = 0.0;
	double directionY = 0.0;
	bool active = false;
	bool initialPending = false;
	uint32_t elapsed = 0;

	bool selectUnit();
};
