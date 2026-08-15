/*
 * ControllerAxisNormalizer.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "../StdInc.h"

#include "ControllerAxisNormalizer.h"

double ControllerAxisNormalizer::normalize(int value, double deadZone, double fullZone)
{
	if(deadZone < 0.0 || fullZone > 1.0 || fullZone <= deadZone)
		return 0.0;

	constexpr double axisMaximum = std::numeric_limits<int16_t>::max();
	const double ratio = std::clamp(static_cast<double>(value) / axisMaximum, -1.0, 1.0);
	const double magnitude = std::abs(ratio);
	if(magnitude <= deadZone)
		return 0.0;

	const double normalized = std::clamp((magnitude - deadZone) / (fullZone - deadZone), 0.0, 1.0);
	return std::copysign(normalized, ratio);
}
