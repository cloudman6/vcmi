/*
 * BattleControllerPromptGlyph.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "BattleControllerPromptGlyph.h"

std::string BattleControllerPromptGlyph::bindingLabel(
	const std::string & binding, ControllerPrompt::Family family)
{
	if(family == ControllerPrompt::Family::PLAYSTATION)
	{
		if(binding == "a") return "×";
		if(binding == "b") return "○";
		if(binding == "x") return "□";
		if(binding == "y") return "△";
		if(binding == "leftshoulder") return "L1";
		if(binding == "rightshoulder") return "R1";
		if(binding == "lefttrigger") return "L2";
		if(binding == "righttrigger") return "R2";
	}
	else if(family == ControllerPrompt::Family::NINTENDO)
	{
		if(binding == "a") return "B";
		if(binding == "b") return "A";
		if(binding == "x") return "Y";
		if(binding == "y") return "X";
		if(binding == "leftshoulder") return "L";
		if(binding == "rightshoulder") return "R";
		if(binding == "lefttrigger") return "ZL";
		if(binding == "righttrigger") return "ZR";
	}
	else
	{
		if(binding == "a") return "A";
		if(binding == "b") return "B";
		if(binding == "x") return "X";
		if(binding == "y") return "Y";
		if(binding == "leftshoulder") return "LB";
		if(binding == "rightshoulder") return "RB";
		if(binding == "lefttrigger") return "LT";
		if(binding == "righttrigger") return "RT";
	}

	std::string result = binding;
	std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character)
	{
		return static_cast<char>(std::toupper(character));
	});
	return result;
}

BattleControllerPromptGlyph::Presentation BattleControllerPromptGlyph::resolve(
	const std::vector<std::string> & bindings, ControllerPrompt::Family family, bool pressed)
{
	if(bindings.size() != 1)
		return {};

	const auto & binding = bindings.front();
	const bool faceButton = binding == "a" || binding == "b" || binding == "x" || binding == "y";
	if(!faceButton)
		return {"", bindingLabel(binding, family)};

	const std::string state = pressed ? "pressed" : "normal";
	if(family == ControllerPrompt::Family::PLAYSTATION && (binding == "a" || binding == "b"))
		return {"controllerActionBar/playstation-" + binding + "-" + state + ".png", ""};
	if(family != ControllerPrompt::Family::PLAYSTATION)
		return {"controllerActionBar/generic-face-" + state + ".png", bindingLabel(binding, family)};
	return {"", bindingLabel(binding, family)};
}
