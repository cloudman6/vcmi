/*
 * ControllerAxisNormalizer.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

class ControllerAxisNormalizer
{
public:
	/// Maps a signed 16-bit controller sample into [-1, 1], with a symmetric
	/// dead zone and saturation zone. Invalid zone configurations fail closed.
	static double normalize(int value, double deadZone, double fullZone);
};
