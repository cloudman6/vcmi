/*
 * BattleDirectionArrow.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../StdInc.h"

#include "BattleDirectionArrow.h"

#include "../render/Canvas.h"

#include <cmath>

void BattleDirectionArrow::draw(Canvas & canvas, const Point & from, const Point & to, const ColorRGBA & color)
{
	const double dx = to.x - from.x;
	const double dy = to.y - from.y;
	const double length = std::sqrt(dx * dx + dy * dy);
	if(length < 2.0)
		return;

	const double ux = dx / length;
	const double uy = dy / length;

	// shaft stops where the head wings begin
	const Point headBase(to.x - static_cast<int>(ux * 7), to.y - static_cast<int>(uy * 7));
	canvas.drawLine(from, headBase, color, color);

	// two wings opening backward from the tip
	const double backward = std::atan2(-uy, -ux);
	for(const double sign : {1.0, -1.0})
	{
		const double angle = backward + sign * 0.5;
		const Point wing(
			to.x + static_cast<int>(std::cos(angle) * 9),
			to.y + static_cast<int>(std::sin(angle) * 9));
		canvas.drawLine(to, wing, color, color);
	}
}
