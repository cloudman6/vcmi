/*
 * ControllerE2EProfiles.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "ControllerE2EVirtualController.h"

#include "../../lib/json/JsonNode.h"

#include <map>

namespace ControllerE2E
{

namespace
{

/// Profiles are pure data. The executor core parses every profile through the
/// same strict JSON contract, so adding a controller family never requires
/// executor changes. dualsense is the first product profile; synthetic-generic
/// and xbox-series exist to prove the extension contract without claiming any
/// real-device player acceptance for those families.
const std::map<std::string, std::string> BUILTIN_PROFILE_DOCUMENTS = {
	{"dualsense", R"(
{
	"schema": "vcmi.controller-e2e/profile/v1",
	"id": "dualsense",
	"display_name": "VCMI E2E DualSense",
	"vendor_id": 1356,
	"product_id": 3302,
	"sdl_type_hint": "ps5",
	"buttons": {
		"south": 0,
		"east": 1,
		"west": 2,
		"north": 3,
		"create": 4,
		"ps": 5,
		"options": 6,
		"left_stick_click": 7,
		"right_stick_click": 8,
		"l1": 9,
		"r1": 10,
		"dpad_up": 11,
		"dpad_down": 12,
		"dpad_left": 13,
		"dpad_right": 14,
		"touchpad_click": 15
	},
	"axes": {
		"left_stick_x": 0,
		"left_stick_y": 1,
		"right_stick_x": 2,
		"right_stick_y": 3,
		"l2": 4,
		"r2": 5
	},
	"axis_kinds": {
		"left_stick_x": "stick",
		"left_stick_y": "stick",
		"right_stick_x": "stick",
		"right_stick_y": "stick",
		"l2": "trigger",
		"r2": "trigger"
	},
	"sdl_bindings": {
		"a": "south",
		"b": "east",
		"x": "west",
		"y": "north",
		"back": "create",
		"guide": "ps",
		"start": "options",
		"leftstick": "left_stick_click",
		"rightstick": "right_stick_click",
		"leftshoulder": "l1",
		"rightshoulder": "r1",
		"dpup": "dpad_up",
		"dpdown": "dpad_down",
		"dpleft": "dpad_left",
		"dpright": "dpad_right",
		"touchpad": "touchpad_click",
		"leftx": "left_stick_x",
		"lefty": "left_stick_y",
		"rightx": "right_stick_x",
		"righty": "right_stick_y",
		"lefttrigger": "l2",
		"righttrigger": "r2"
	}
}
)"},
	{"synthetic-generic", R"(
{
	"schema": "vcmi.controller-e2e/profile/v1",
	"id": "synthetic-generic",
	"display_name": "VCMI E2E synthetic controller",
	"vendor_id": 4660,
	"product_id": 22136,
	"sdl_type_hint": "",
	"buttons": {
		"south": 0,
		"east": 1,
		"west": 2,
		"north": 3,
		"back": 4,
		"guide": 5,
		"start": 6,
		"left_stick_click": 7,
		"right_stick_click": 8,
		"left_shoulder": 9,
		"right_shoulder": 10,
		"dpad_up": 11,
		"dpad_down": 12,
		"dpad_left": 13,
		"dpad_right": 14
	},
	"axes": {
		"left_stick_x": 0,
		"left_stick_y": 1,
		"right_stick_x": 2,
		"right_stick_y": 3,
		"left_trigger": 4,
		"right_trigger": 5
	},
	"axis_kinds": {
		"left_stick_x": "stick",
		"left_stick_y": "stick",
		"right_stick_x": "stick",
		"right_stick_y": "stick",
		"left_trigger": "trigger",
		"right_trigger": "trigger"
	},
	"sdl_bindings": {
		"a": "south",
		"b": "east",
		"x": "west",
		"y": "north",
		"back": "back",
		"guide": "guide",
		"start": "start",
		"leftstick": "left_stick_click",
		"rightstick": "right_stick_click",
		"leftshoulder": "left_shoulder",
		"rightshoulder": "right_shoulder",
		"dpup": "dpad_up",
		"dpdown": "dpad_down",
		"dpleft": "dpad_left",
		"dpright": "dpad_right",
		"leftx": "left_stick_x",
		"lefty": "left_stick_y",
		"rightx": "right_stick_x",
		"righty": "right_stick_y",
		"lefttrigger": "left_trigger",
		"righttrigger": "right_trigger"
	}
}
)"},
	{"xbox-series", R"(
{
	"schema": "vcmi.controller-e2e/profile/v1",
	"id": "xbox-series",
	"display_name": "VCMI E2E Xbox fixture",
	"vendor_id": 1118,
	"product_id": 746,
	"sdl_type_hint": "xboxone",
	"buttons": {
		"south": 0,
		"east": 1,
		"west": 2,
		"north": 3,
		"view": 4,
		"guide": 5,
		"menu": 6,
		"left_stick_click": 7,
		"right_stick_click": 8,
		"lb": 9,
		"rb": 10,
		"dpad_up": 11,
		"dpad_down": 12,
		"dpad_left": 13,
		"dpad_right": 14
	},
	"axes": {
		"left_stick_x": 0,
		"left_stick_y": 1,
		"right_stick_x": 2,
		"right_stick_y": 3,
		"lt": 4,
		"rt": 5
	},
	"axis_kinds": {
		"left_stick_x": "stick",
		"left_stick_y": "stick",
		"right_stick_x": "stick",
		"right_stick_y": "stick",
		"lt": "trigger",
		"rt": "trigger"
	},
	"sdl_bindings": {
		"a": "south",
		"b": "east",
		"x": "west",
		"y": "north",
		"back": "view",
		"guide": "guide",
		"start": "menu",
		"leftstick": "left_stick_click",
		"rightstick": "right_stick_click",
		"leftshoulder": "lb",
		"rightshoulder": "rb",
		"dpup": "dpad_up",
		"dpdown": "dpad_down",
		"dpleft": "dpad_left",
		"dpright": "dpad_right",
		"leftx": "left_stick_x",
		"lefty": "left_stick_y",
		"rightx": "right_stick_x",
		"righty": "right_stick_y",
		"lefttrigger": "lt",
		"righttrigger": "rt"
	}
}
)"}
};

}

std::map<std::string, std::string> builtinProfileDocuments()
{
	return BUILTIN_PROFILE_DOCUMENTS;
}

}
