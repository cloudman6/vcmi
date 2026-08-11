/*
 * BattleDirectionArrow.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/Point.h"
#include "../../lib/Color.h"

class Canvas;

/// Minimal geometry direction arrow for the melee approach choice (D3):
/// a shaft from the chosen attack-from hex toward the target plus two
/// head wings. No art asset; frozen through real composited frames.
class BattleDirectionArrow
{
public:
	static void draw(Canvas & canvas, const Point & from, const Point & to, const ColorRGBA & color);
};
