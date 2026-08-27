/*
 * BattleControllerPromptGlyph.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../eventsSDL/ControllerPromptFamily.h"

class BattleControllerPromptGlyph
{
public:
	struct Presentation
	{
		std::string spritePath;
		std::string runtimeLabel;
	};

	static std::string bindingLabel(const std::string & binding, ControllerPrompt::Family family);
	static Presentation resolve(const std::vector<std::string> & bindings,
		ControllerPrompt::Family family, bool pressed);
};
