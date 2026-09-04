/*
 * ControllerE2EScenario.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "ControllerE2EScenario.h"

#include "../../lib/json/JsonNode.h"

#include <algorithm>
#include <set>

namespace ControllerE2E
{

namespace
{

const std::map<std::string, ScenarioStep::Kind> KIND_BY_OP = {
	{"attach", ScenarioStep::Kind::ATTACH},
	{"detach", ScenarioStep::Kind::DETACH},
	{"reconnect", ScenarioStep::Kind::RECONNECT},
	{"remap", ScenarioStep::Kind::REMAP},
	{"select_device", ScenarioStep::Kind::SELECT_DEVICE},
	{"press", ScenarioStep::Kind::PRESS},
	{"release", ScenarioStep::Kind::RELEASE},
	{"tap", ScenarioStep::Kind::TAP},
	{"set_axis", ScenarioStep::Kind::SET_AXIS},
	{"ramp_axis", ScenarioStep::Kind::RAMP_AXIS},
	{"neutralize", ScenarioStep::Kind::NEUTRALIZE},
	{"press_key", ScenarioStep::Kind::PRESS_KEY},
	{"release_key", ScenarioStep::Kind::RELEASE_KEY},
	{"text_input", ScenarioStep::Kind::TEXT_INPUT},
	{"move_mouse", ScenarioStep::Kind::MOVE_MOUSE},
	{"click_mouse", ScenarioStep::Kind::CLICK_MOUSE},
	{"scroll_mouse", ScenarioStep::Kind::SCROLL_MOUSE},
	{"reject_next_battle_action", ScenarioStep::Kind::REJECT_NEXT_BATTLE_ACTION},
	{"authority_remove_battle_stack", ScenarioStep::Kind::AUTHORITY_REMOVE_BATTLE_STACK},
	{"authority_set_active_battle_stack", ScenarioStep::Kind::AUTHORITY_SET_ACTIVE_BATTLE_STACK},
	{"authority_swap_adventure_heroes", ScenarioStep::Kind::AUTHORITY_SWAP_ADVENTURE_HEROES},
	{"authority_remove_adventure_object", ScenarioStep::Kind::AUTHORITY_REMOVE_ADVENTURE_OBJECT},
	{"wait_frames", ScenarioStep::Kind::WAIT_FRAMES},
	{"wait_until", ScenarioStep::Kind::WAIT_UNTIL},
	{"assert", ScenarioStep::Kind::ASSERT},
	{"assert_unchanged", ScenarioStep::Kind::ASSERT_UNCHANGED},
	{"assert_event_count", ScenarioStep::Kind::ASSERT_EVENT_COUNT},
	{"checkpoint", ScenarioStep::Kind::CHECKPOINT},
	{"capture_frame", ScenarioStep::Kind::CAPTURE_FRAME},
	{"capture_region", ScenarioStep::Kind::CAPTURE_REGION},
	{"manual_hold", ScenarioStep::Kind::MANUAL_HOLD},
	{"request_shutdown", ScenarioStep::Kind::REQUEST_SHUTDOWN},
};

const std::set<ScenarioStep::Kind> PRELUDE_ALLOWED = {
	ScenarioStep::Kind::ATTACH,
	ScenarioStep::Kind::DETACH,
	ScenarioStep::Kind::RECONNECT,
	ScenarioStep::Kind::SELECT_DEVICE,
	ScenarioStep::Kind::SET_AXIS,
	ScenarioStep::Kind::NEUTRALIZE,
};

/// Fields accepted by each operation; anything else fails closed
const std::map<ScenarioStep::Kind, std::set<std::string>> ALLOWED_FIELDS = {
	{ScenarioStep::Kind::ATTACH, {"op", "device", "profile"}},
	{ScenarioStep::Kind::DETACH, {"op", "device"}},
	{ScenarioStep::Kind::RECONNECT, {"op", "device", "held_controls"}},
	{ScenarioStep::Kind::REMAP, {"op", "device"}},
	{ScenarioStep::Kind::SELECT_DEVICE, {"op", "device"}},
	{ScenarioStep::Kind::PRESS, {"op", "device", "control"}},
	{ScenarioStep::Kind::RELEASE, {"op", "device", "control"}},
	{ScenarioStep::Kind::TAP, {"op", "device", "control", "hold_frames"}},
	{ScenarioStep::Kind::SET_AXIS, {"op", "device", "control", "value"}},
	{ScenarioStep::Kind::RAMP_AXIS, {"op", "device", "control", "from", "to", "frames"}},
	{ScenarioStep::Kind::NEUTRALIZE, {"op", "device"}},
	{ScenarioStep::Kind::PRESS_KEY, {"op", "key"}},
	{ScenarioStep::Kind::RELEASE_KEY, {"op", "key"}},
	{ScenarioStep::Kind::TEXT_INPUT, {"op", "text"}},
	{ScenarioStep::Kind::MOVE_MOUSE, {"op", "x", "y"}},
	{ScenarioStep::Kind::CLICK_MOUSE, {"op", "button", "x", "y", "move_pointer"}},
	{ScenarioStep::Kind::SCROLL_MOUSE, {"op", "x", "y"}},
	{ScenarioStep::Kind::REJECT_NEXT_BATTLE_ACTION, {"op"}},
	{ScenarioStep::Kind::AUTHORITY_REMOVE_BATTLE_STACK, {"op", "stack_id"}},
	{ScenarioStep::Kind::AUTHORITY_SET_ACTIVE_BATTLE_STACK, {"op", "stack_id"}},
	{ScenarioStep::Kind::AUTHORITY_SWAP_ADVENTURE_HEROES, {"op", "first_index", "second_index"}},
	{ScenarioStep::Kind::AUTHORITY_REMOVE_ADVENTURE_OBJECT, {"op", "object_id"}},
	{ScenarioStep::Kind::WAIT_FRAMES, {"op", "frames"}},
	{ScenarioStep::Kind::WAIT_UNTIL, {"op", "probe", "field", "equals", "changes", "timeout_ms", "stable_for_frames"}},
	{ScenarioStep::Kind::ASSERT, {"op", "probe", "field", "equals"}},
	{ScenarioStep::Kind::ASSERT_UNCHANGED, {"op", "probe", "field", "frames"}},
	{ScenarioStep::Kind::ASSERT_EVENT_COUNT, {"op", "kind", "count"}},
	{ScenarioStep::Kind::CHECKPOINT, {"op", "name"}},
	{ScenarioStep::Kind::CAPTURE_FRAME, {"op", "name", "region"}},
	{ScenarioStep::Kind::CAPTURE_REGION, {"op", "name", "x", "y", "w", "h"}},
	{ScenarioStep::Kind::MANUAL_HOLD, {"op"}},
	{ScenarioStep::Kind::REQUEST_SHUTDOWN, {"op"}},
};

const std::map<ScenarioStep::Kind, std::set<std::string>> REQUIRED_FIELDS = {
	{ScenarioStep::Kind::ATTACH, {"device", "profile"}},
	{ScenarioStep::Kind::DETACH, {"device"}},
	{ScenarioStep::Kind::RECONNECT, {"device"}},
	{ScenarioStep::Kind::REMAP, {"device"}},
	{ScenarioStep::Kind::SELECT_DEVICE, {"device"}},
	{ScenarioStep::Kind::PRESS, {"device", "control"}},
	{ScenarioStep::Kind::RELEASE, {"device", "control"}},
	{ScenarioStep::Kind::TAP, {"device", "control"}},
	{ScenarioStep::Kind::SET_AXIS, {"device", "control", "value"}},
	{ScenarioStep::Kind::RAMP_AXIS, {"device", "control", "from", "to", "frames"}},
	{ScenarioStep::Kind::NEUTRALIZE, {"device"}},
	{ScenarioStep::Kind::PRESS_KEY, {"key"}},
	{ScenarioStep::Kind::RELEASE_KEY, {"key"}},
	{ScenarioStep::Kind::TEXT_INPUT, {"text"}},
	{ScenarioStep::Kind::MOVE_MOUSE, {"x", "y"}},
	{ScenarioStep::Kind::CLICK_MOUSE, {"button", "x", "y"}},
	{ScenarioStep::Kind::SCROLL_MOUSE, {"x", "y"}},
	{ScenarioStep::Kind::REJECT_NEXT_BATTLE_ACTION, {}},
	{ScenarioStep::Kind::AUTHORITY_REMOVE_BATTLE_STACK, {"stack_id"}},
	{ScenarioStep::Kind::AUTHORITY_SET_ACTIVE_BATTLE_STACK, {"stack_id"}},
	{ScenarioStep::Kind::AUTHORITY_SWAP_ADVENTURE_HEROES, {"first_index", "second_index"}},
	{ScenarioStep::Kind::AUTHORITY_REMOVE_ADVENTURE_OBJECT, {"object_id"}},
	{ScenarioStep::Kind::WAIT_FRAMES, {"frames"}},
	{ScenarioStep::Kind::WAIT_UNTIL, {"probe", "field", "timeout_ms"}},
	{ScenarioStep::Kind::ASSERT, {"probe", "field", "equals"}},
	{ScenarioStep::Kind::ASSERT_UNCHANGED, {"probe", "field", "frames"}},
	{ScenarioStep::Kind::ASSERT_EVENT_COUNT, {"kind", "count"}},
	{ScenarioStep::Kind::CHECKPOINT, {"name"}},
	{ScenarioStep::Kind::CAPTURE_FRAME, {"name"}},
	{ScenarioStep::Kind::CAPTURE_REGION, {"name", "x", "y", "w", "h"}},
	{ScenarioStep::Kind::MANUAL_HOLD, {}},
	{ScenarioStep::Kind::REQUEST_SHUTDOWN, {}},
};

bool requireStringField(
	const JsonNode & step,
	const std::string & field,
	std::string & target,
	const std::string & where,
	std::vector<std::string> & errors
)
{
	const auto & value = step[field];
	if(!value.isString() || value.String().empty())
	{
		errors.push_back(where + ": field '" + field + "' must be a non-empty string");
		return false;
	}
	target = value.String();
	return true;
}

bool requireIntField(
	const JsonNode & step,
	const std::string & field,
	int & target,
	int minValue,
	int maxValue,
	const std::string & where,
	std::vector<std::string> & errors
)
{
	const auto & value = step[field];
	if(value.getType() != JsonNode::JsonType::DATA_INTEGER || value.Integer() < minValue || value.Integer() > maxValue)
	{
		errors.push_back(
			where + ": field '" + field + "' must be an integer in [" + std::to_string(minValue) + ", "
			+ std::to_string(maxValue) + "]"
		);
		return false;
	}
	target = static_cast<int>(value.Integer());
	return true;
}

bool requireFloatField(
	const JsonNode & step,
	const std::string & field,
	double & target,
	double minValue,
	double maxValue,
	const std::string & where,
	std::vector<std::string> & errors
)
{
	const auto & value = step[field];
	if(!value.isNumber())
	{
		errors.push_back(where + ": field '" + field + "' must be a number");
		return false;
	}
	const double number =
		value.getType() == JsonNode::JsonType::DATA_INTEGER ? static_cast<double>(value.Integer()) : value.Float();
	if(number < minValue || number > maxValue)
	{
		errors.push_back(
			where + ": field '" + field + "' must be in [" + std::to_string(minValue) + ", " + std::to_string(maxValue)
			+ "]"
		);
		return false;
	}
	target = number;
	return true;
}

void parseRegion(
	const JsonNode & region,
	ScenarioStep & target,
	const std::string & where,
	std::vector<std::string> & errors
)
{
	if(!region.isStruct())
	{
		errors.push_back(where + ": field 'region' must be an object");
		return;
	}
	target.hasRegion = true;
	requireIntField(region, "x", target.regionX, 0, 100000, where + ".region", errors);
	requireIntField(region, "y", target.regionY, 0, 100000, where + ".region", errors);
	requireIntField(region, "w", target.regionW, 1, 100000, where + ".region", errors);
	requireIntField(region, "h", target.regionH, 1, 100000, where + ".region", errors);
}

void parseHeldControls(const JsonNode & node, ScenarioStep & target, const std::string & where,
	std::vector<std::string> & errors)
{
	const auto & controls = node["held_controls"];
	if(controls.getType() == JsonNode::JsonType::DATA_NULL)
		return;
	if(!controls.isVector() || controls.Vector().empty())
	{
		errors.push_back(where + ": field 'held_controls' must be a non-empty string array");
		return;
	}

	std::set<std::string> uniqueControls;
	for(const auto & control : controls.Vector())
	{
		if(!control.isString() || control.String().empty())
		{
			errors.push_back(where + ": field 'held_controls' must contain non-empty strings");
			continue;
		}
		if(!uniqueControls.insert(control.String()).second)
		{
			errors.push_back(where + ": field 'held_controls' contains duplicate control '" + control.String() + "'");
			continue;
		}
		target.heldControls.push_back(control.String());
	}
}

void parseStepFields(
	const JsonNode & node,
	ScenarioStep & step,
	const std::string & where,
	std::vector<std::string> & errors
)
{
	switch(step.kind)
	{
	case ScenarioStep::Kind::ATTACH:
		requireStringField(node, "device", step.device, where, errors);
		requireStringField(node, "profile", step.profileId, where, errors);
		break;
	case ScenarioStep::Kind::DETACH:
	case ScenarioStep::Kind::REMAP:
	case ScenarioStep::Kind::SELECT_DEVICE:
	case ScenarioStep::Kind::NEUTRALIZE:
		requireStringField(node, "device", step.device, where, errors);
		break;
	case ScenarioStep::Kind::RECONNECT:
		requireStringField(node, "device", step.device, where, errors);
		parseHeldControls(node, step, where, errors);
		break;
	case ScenarioStep::Kind::PRESS:
	case ScenarioStep::Kind::RELEASE:
		requireStringField(node, "device", step.device, where, errors);
		requireStringField(node, "control", step.control, where, errors);
		break;
	case ScenarioStep::Kind::TAP:
		requireStringField(node, "device", step.device, where, errors);
		requireStringField(node, "control", step.control, where, errors);
		if(node["hold_frames"].getType() != JsonNode::JsonType::DATA_NULL)
			requireIntField(node, "hold_frames", step.holdFrames, 1, 10000, where, errors);
		break;
	case ScenarioStep::Kind::SET_AXIS:
		requireStringField(node, "device", step.device, where, errors);
		requireStringField(node, "control", step.control, where, errors);
		requireFloatField(node, "value", step.axisValue, -1.0, 1.0, where, errors);
		break;
	case ScenarioStep::Kind::RAMP_AXIS:
		requireStringField(node, "device", step.device, where, errors);
		requireStringField(node, "control", step.control, where, errors);
		requireFloatField(node, "from", step.axisFrom, -1.0, 1.0, where, errors);
		requireFloatField(node, "to", step.axisTo, -1.0, 1.0, where, errors);
		requireIntField(node, "frames", step.rampFrames, 1, 100000, where, errors);
		break;
	case ScenarioStep::Kind::PRESS_KEY:
	case ScenarioStep::Kind::RELEASE_KEY:
		requireStringField(node, "key", step.keyName, where, errors);
		break;
	case ScenarioStep::Kind::TEXT_INPUT:
		if(requireStringField(node, "text", step.text, where, errors) && step.text.size() > 31)
			errors.push_back(where + ": field 'text' exceeds SDL's 31-byte event payload");
		break;
	case ScenarioStep::Kind::MOVE_MOUSE:
		requireIntField(node, "x", step.mouseX, 0, 100000, where, errors);
		requireIntField(node, "y", step.mouseY, 0, 100000, where, errors);
		break;
	case ScenarioStep::Kind::CLICK_MOUSE:
		requireStringField(node, "button", step.mouseButton, where, errors);
		requireIntField(node, "x", step.mouseX, 0, 100000, where, errors);
		requireIntField(node, "y", step.mouseY, 0, 100000, where, errors);
		if(node["move_pointer"].getType() != JsonNode::JsonType::DATA_NULL)
		{
			if(!node["move_pointer"].isBool())
				errors.push_back(where + ": field 'move_pointer' must be a boolean");
			else
				step.moveMousePointer = node["move_pointer"].Bool();
		}
		break;
	case ScenarioStep::Kind::SCROLL_MOUSE:
		requireIntField(node, "x", step.wheelX, -100, 100, where, errors);
		requireIntField(node, "y", step.wheelY, -100, 100, where, errors);
		break;
	case ScenarioStep::Kind::REJECT_NEXT_BATTLE_ACTION:
		break;
	case ScenarioStep::Kind::AUTHORITY_REMOVE_BATTLE_STACK:
	case ScenarioStep::Kind::AUTHORITY_SET_ACTIVE_BATTLE_STACK:
		requireIntField(node, "stack_id", step.stackId, 0, 1000000, where, errors);
		break;
	case ScenarioStep::Kind::AUTHORITY_SWAP_ADVENTURE_HEROES:
		requireIntField(node, "first_index", step.firstIndex, 0, 1000, where, errors);
		requireIntField(node, "second_index", step.secondIndex, 0, 1000, where, errors);
		break;
	case ScenarioStep::Kind::AUTHORITY_REMOVE_ADVENTURE_OBJECT:
		requireIntField(node, "object_id", step.objectId, 0, 1000000, where, errors);
		break;
	case ScenarioStep::Kind::WAIT_FRAMES:
		requireIntField(node, "frames", step.frames, 0, 1000000, where, errors);
		break;
	case ScenarioStep::Kind::WAIT_UNTIL:
		requireStringField(node, "probe", step.probe, where, errors);
		if(!requireIntField(node, "timeout_ms", step.timeoutMs, 1, 600000, where, errors))
			step.timeoutMs = 0; // fail closed: zero timeout rejects the wait
		if(node["stable_for_frames"].getType() != JsonNode::JsonType::DATA_NULL)
			requireIntField(node, "stable_for_frames", step.stableForFrames, 1, 10000, where, errors);
		if(node["equals"].getType() != JsonNode::JsonType::DATA_NULL
		   && node["changes"].getType() != JsonNode::JsonType::DATA_NULL)
		{
			errors.push_back(where + ": wait_until must use either 'equals' or 'changes', not both");
		}
		else if(node["equals"].getType() != JsonNode::JsonType::DATA_NULL)
		{
			step.expected = node["equals"];
		}
		else if(node["changes"].isBool() && node["changes"].Bool())
		{
			step.expectChange = true;
		}
		else
		{
			errors.push_back(where + ": wait_until needs a boolean true 'changes' or an 'equals' value");
		}
		requireStringField(node, "field", step.field, where, errors);
		break;
	case ScenarioStep::Kind::ASSERT:
		requireStringField(node, "probe", step.probe, where, errors);
		requireStringField(node, "field", step.field, where, errors);
		step.expected = node["equals"];
		break;
	case ScenarioStep::Kind::ASSERT_UNCHANGED:
		requireStringField(node, "probe", step.probe, where, errors);
		requireStringField(node, "field", step.field, where, errors);
		requireIntField(node, "frames", step.frames, 1, 1000000, where, errors);
		break;
	case ScenarioStep::Kind::ASSERT_EVENT_COUNT:
		requireStringField(node, "kind", step.eventKind, where, errors);
		requireIntField(node, "count", step.eventCount, 0, 1000000, where, errors);
		break;
	case ScenarioStep::Kind::CHECKPOINT:
	case ScenarioStep::Kind::CAPTURE_FRAME:
		requireStringField(node, "name", step.name, where, errors);
		if(step.kind == ScenarioStep::Kind::CAPTURE_FRAME && node["region"].getType() != JsonNode::JsonType::DATA_NULL)
			parseRegion(node["region"], step, where, errors);
		break;
	case ScenarioStep::Kind::CAPTURE_REGION:
		requireStringField(node, "name", step.name, where, errors);
		requireIntField(node, "x", step.regionX, 0, 100000, where, errors);
		requireIntField(node, "y", step.regionY, 0, 100000, where, errors);
		requireIntField(node, "w", step.regionW, 1, 100000, where, errors);
		requireIntField(node, "h", step.regionH, 1, 100000, where, errors);
		step.hasRegion = true;
		break;
	case ScenarioStep::Kind::MANUAL_HOLD:
	case ScenarioStep::Kind::REQUEST_SHUTDOWN:
		break;
	}
}

bool parseStepList(
	const JsonNode & node,
	const std::string & field,
	bool prelude,
	std::vector<ScenarioStep> & target,
	std::vector<std::string> & errors
)
{
	const auto & list = node[field];
	if(list.getType() == JsonNode::JsonType::DATA_NULL)
		return true;
	if(!list.isVector())
	{
		errors.push_back("scenario: field '" + field + "' must be an array");
		return false;
	}

	int index = 0;
	for(const auto & entry : list.Vector())
	{
		const std::string where = field + "[" + std::to_string(index) + "]";
		if(!entry.isStruct())
		{
			errors.push_back(where + ": step must be an object");
			++index;
			continue;
		}

		ScenarioStep step;
		step.index = index;
		step.raw = entry;

		const auto & opNode = entry["op"];
		if(!opNode.isString())
		{
			errors.push_back(where + ": field 'op' must be a string");
			++index;
			continue;
		}
		const auto kindIterator = KIND_BY_OP.find(opNode.String());
		if(kindIterator == KIND_BY_OP.end())
		{
			errors.push_back(where + ": unknown operation '" + opNode.String() + "'");
			++index;
			continue;
		}
		step.kind = kindIterator->second;

		if(prelude && !PRELUDE_ALLOWED.count(step.kind))
			errors.push_back(where + ": operation '" + opNode.String() + "' is not allowed in prelude");

		const auto & allowed = ALLOWED_FIELDS.at(step.kind);
		for(const auto & [key, value] : entry.Struct())
		{
			if(!allowed.count(key))
				errors.push_back(where + ": unknown field '" + key + "'");
		}

		const auto & required = REQUIRED_FIELDS.at(step.kind);
		for(const auto & requiredField : required)
		{
			if(entry[requiredField].getType() == JsonNode::JsonType::DATA_NULL)
				errors.push_back(where + ": missing required field '" + requiredField + "'");
		}

		parseStepFields(entry, step, where, errors);
		target.push_back(std::move(step));
		++index;
	}
	return true;
}

}

std::string ScenarioStep::kindName() const
{
	for(const auto & [name, kind] : KIND_BY_OP)
		if(kind == this->kind)
			return name;
	return "unknown";
}

ScenarioParseResult parseScenarioText(const std::string & text)
{
	ScenarioParseResult result;

	JsonParsingSettings settings;
	settings.mode = JsonParsingSettings::JsonFormatMode::JSON;
	settings.strict = true;

	JsonNode document;
	try
	{
		document = JsonNode(text.c_str(), text.size(), settings, "controller-e2e-scenario");
	}
	catch(const std::exception & e)
	{
		result.errors.push_back(std::string("scenario: JSON parse error: ") + e.what());
		return result;
	}

	if(!document.isStruct())
	{
		result.errors.push_back("scenario: document must be a JSON object");
		return result;
	}

	const std::set<std::string> allowedTopLevel = {
		"schema", "id", "profile", "settingsOverride", "fixture", "timeout_ms", "prelude", "steps"
	};
	for(const auto & [key, value] : document.Struct())
		if(!allowedTopLevel.count(key))
			result.errors.push_back("scenario: unknown top-level field '" + key + "'");

	const auto overrideIt = document.Struct().find("settingsOverride");
	if(overrideIt != document.Struct().end() && !overrideIt->second.isStruct())
		result.errors.push_back("scenario: field 'settingsOverride' must be an object");

	ScenarioSpec scenario;

	const auto & schema = document["schema"];
	if(!schema.isString() || schema.String() != ScenarioSpec::SCHEMA)
		result.errors.push_back("scenario: field 'schema' must be \"" + std::string(ScenarioSpec::SCHEMA) + "\"");

	if(!requireStringField(document, "id", scenario.id, "scenario", result.errors))
		scenario.id.clear();

	if(!requireStringField(document, "profile", scenario.profileId, "scenario", result.errors))
		scenario.profileId.clear();

	if(document["timeout_ms"].getType() != JsonNode::JsonType::DATA_NULL)
	{
		if(!requireIntField(document, "timeout_ms", scenario.timeoutMs, 1, 3600000, "scenario", result.errors))
			scenario.timeoutMs = 0; // fail closed
	}

	const auto & fixture = document["fixture"];
	if(fixture.getType() != JsonNode::JsonType::DATA_NULL)
	{
		if(!fixture.isStruct())
			result.errors.push_back("scenario: field 'fixture' must be an object");
		else
			scenario.fixture = fixture;
	}

	parseStepList(document, "prelude", true, scenario.prelude, result.errors);
	parseStepList(document, "steps", false, scenario.steps, result.errors);

	if(scenario.steps.empty())
		result.errors.push_back("scenario: field 'steps' must contain at least one step");

	// device references must be created by an attach step before use
	std::set<std::string> createdDevices;
	auto checkDeviceOrder = [&](const std::vector<ScenarioStep> & list, const std::string & section)
	{
		for(const auto & step : list)
		{
			if(step.kind == ScenarioStep::Kind::ATTACH)
			{
				if(!createdDevices.insert(step.device).second)
					result.errors.push_back(
						section + "[" + std::to_string(step.index) + "]: device '" + step.device + "' attached twice"
					);
				continue;
			}
			if(step.device.empty())
				continue;
			if(step.kind == ScenarioStep::Kind::DETACH)
			{
				// detach/reconnect keep the alias valid for later reconnect/attach-less use
				if(!createdDevices.count(step.device))
					result.errors.push_back(
						section + "[" + std::to_string(step.index) + "]: references unknown device '" + step.device
						+ "'"
					);
				continue;
			}
			if(!createdDevices.count(step.device))
				result.errors.push_back(
					section + "[" + std::to_string(step.index) + "]: references unknown device '" + step.device + "'"
				);
		}
	};
	checkDeviceOrder(scenario.prelude, "prelude");
	checkDeviceOrder(scenario.steps, "steps");

	if(!result.errors.empty())
		return result;

	scenario.resolved = document;
	result.scenario = std::move(scenario);
	return result;
}

bool validateScenarioDevices(const ScenarioSpec & scenario, const ScenarioProfileButtons & profileButtons,
	std::vector<std::string> & errors)
{
	bool valid = true;
	std::map<std::string, std::string> deviceProfiles;
	auto checkSteps = [&](const std::vector<ScenarioStep> & list, const std::string & section)
	{
		for(const auto & step : list)
		{
			if(step.kind == ScenarioStep::Kind::ATTACH)
			{
				if(!profileButtons.count(step.profileId))
				{
					errors.push_back(section + "[" + std::to_string(step.index) + "]: unknown profile '" + step.profileId + "'");
					valid = false;
				}
				else
				{
					deviceProfiles[step.device] = step.profileId;
				}
				continue;
			}
			if(step.heldControls.empty())
				continue;

			const auto device = deviceProfiles.find(step.device);
			if(device == deviceProfiles.end())
				continue; // structural parser already reports unknown device aliases
			const auto & buttons = profileButtons.at(device->second);
			for(const auto & control : step.heldControls)
			{
				if(!buttons.count(control))
				{
					errors.push_back(section + "[" + std::to_string(step.index)
						+ "]: held control '" + control + "' is not a button in profile '" + device->second + "'");
					valid = false;
				}
			}
		}
	};
	checkSteps(scenario.prelude, "prelude");
	checkSteps(scenario.steps, "steps");
	return valid;
}

}
