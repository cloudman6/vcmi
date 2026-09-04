/*
 * ControllerNavigationState.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */
#pragma once

class ControllerNavigationRepeatState
{
	uint32_t elapsed = 0;
	bool initialPending = false;
	bool repeating = false;

public:
	void start(bool settleFirst);
	bool ready(uint32_t msPassed);
	void reset();
};

/// Preserves paired stick components and supplies deterministic settle/repeat timing.
class ControllerNavigationState
{
	double axisX = 0.0;
	double axisY = 0.0;
	double directionX = 0.0;
	double directionY = 0.0;
	bool active = false;
	ControllerNavigationRepeatState repeat;

public:
	void update(bool horizontal, double value);
	bool ready(uint32_t msPassed);
	void reset();

	bool isActive() const;
	double x() const;
	double y() const;
	std::pair<double, double> direction() const;
};
