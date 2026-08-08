/*
 * ControllerE2EVirtualController.cpp, part of VCMI engine
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

#include <SDL.h>
#include <SDL_gamecontroller.h>

namespace ControllerE2E
{

namespace
{

bool requireString(const JsonNode & node, const std::string & field, std::string & target, const std::string & context, std::vector<std::string> & errors)
{
	const auto & value = node[field];
	if(!value.isString() || value.String().empty())
	{
		errors.push_back(context + ": field '" + field + "' must be a non-empty string");
		return false;
	}
	target = value.String();
	return true;
}

bool requireIntValue(const JsonNode & value, int & target, int minValue, int maxValue, const std::string & context, std::vector<std::string> & errors)
{
	if(value.getType() != JsonNode::JsonType::DATA_INTEGER || value.Integer() < minValue || value.Integer() > maxValue)
	{
		errors.push_back(context + " must be an integer in [" + std::to_string(minValue) + ", " + std::to_string(maxValue) + "]");
		return false;
	}
	target = static_cast<int>(value.Integer());
	return true;
}

bool requireInt(const JsonNode & node, const std::string & field, int & target, int minValue, int maxValue, const std::string & context, std::vector<std::string> & errors)
{
	return requireIntValue(node[field], target, minValue, maxValue, context + ": field '" + field + "'", errors);
}

bool parseControlMap(const JsonNode & node, const std::string & field, std::map<std::string, int> & target, int maxIndex, const std::string & context, std::vector<std::string> & errors)
{
	const auto & controls = node[field];
	if(!controls.isStruct())
	{
		errors.push_back(context + ": field '" + field + "' must be an object");
		return false;
	}
	for(const auto & [name, indexNode] : controls.Struct())
	{
		int index = 0;
		if(!requireIntValue(indexNode, index, 0, maxIndex, context + ": control '" + name + "'", errors))
			continue;
		if(!target.emplace(name, index).second)
			errors.push_back(context + ": duplicate control '" + name + "'");
	}
	return true;
}

}

std::optional<VirtualControllerProfile> VirtualControllerProfile::parse(const JsonNode & node, std::vector<std::string> & errors)
{
	VirtualControllerProfile profile;
	const std::string context = "profile";

	if(!node.isStruct())
	{
		errors.push_back(context + ": document must be a JSON object");
		return std::nullopt;
	}

	const auto & schema = node["schema"];
	if(!schema.isString() || schema.String() != SCHEMA)
		errors.push_back(context + ": field 'schema' must be \"" + std::string(SCHEMA) + "\"");

	requireString(node, "id", profile.id, context, errors);
	requireString(node, "display_name", profile.displayName, context, errors);

	int vendorId = 0;
	int productId = 0;
	if(requireInt(node, "vendor_id", vendorId, 0, 65535, context, errors))
		profile.vendorId = static_cast<Uint16>(vendorId);
	if(requireInt(node, "product_id", productId, 0, 65535, context, errors))
		profile.productId = static_cast<Uint16>(productId);

	const auto & typeHint = node["sdl_type_hint"];
	if(typeHint.getType() != JsonNode::JsonType::DATA_NULL)
	{
		if(!typeHint.isString())
			errors.push_back(context + ": field 'sdl_type_hint' must be a string");
		else
			profile.sdlTypeHint = typeHint.String();
	}

	const auto & presentation = node["expected_presentation"];
	if(presentation.isString() && (presentation.String() == "playstation" || presentation.String() == "generic"))
		profile.expectedPresentation = presentation.String();
	else
		errors.push_back(context + ": field 'expected_presentation' must be \"playstation\" or \"generic\"");

	// Raw virtual joystick limits accepted by SDL_JoystickAttachVirtual
	parseControlMap(node, "buttons", profile.buttons, 255, context, errors);
	parseControlMap(node, "axes", profile.axes, 255, context, errors);

	const auto & axisKinds = node["axis_kinds"];
	if(!axisKinds.isStruct())
	{
		errors.push_back(context + ": field 'axis_kinds' must be an object");
	}
	else
	{
		for(const auto & [name, kindNode] : axisKinds.Struct())
		{
			if(!kindNode.isString() || (kindNode.String() != "stick" && kindNode.String() != "trigger"))
				errors.push_back(context + ": axis kind for '" + name + "' must be \"stick\" or \"trigger\"");
			else if(!profile.axes.count(name))
				errors.push_back(context + ": axis kind references unknown axis '" + name + "'");
			else
				profile.axisKinds[name] = kindNode.String();
		}
	}

	const auto & sdlBindings = node["sdl_bindings"];
	if(!sdlBindings.isStruct())
	{
		errors.push_back(context + ": field 'sdl_bindings' must be an object");
	}
	else
	{
		for(const auto & [token, controlNode] : sdlBindings.Struct())
		{
			if(!controlNode.isString())
			{
				errors.push_back(context + ": sdl binding '" + token + "' must reference a control name");
				continue;
			}
			const std::string & control = controlNode.String();
			const bool known = profile.buttons.count(control) > 0 || profile.axes.count(control) > 0;
			if(!known)
				errors.push_back(context + ": sdl binding '" + token + "' references unknown control '" + control + "'");
			else
				profile.sdlBindings[token] = control;
		}
	}

	for(const auto & [name, index] : profile.axes)
		if(!profile.axisKinds.count(name))
			errors.push_back(context + ": axis '" + name + "' has no entry in 'axis_kinds'");

	if(profile.buttons.empty() && profile.axes.empty())
		errors.push_back(context + ": profile declares no controls");

	if(!errors.empty())
		return std::nullopt;
	return profile;
}

std::optional<int> VirtualControllerProfile::buttonIndex(const std::string & control) const
{
	const auto found = buttons.find(control);
	if(found == buttons.end())
		return std::nullopt;
	return found->second;
}

std::optional<int> VirtualControllerProfile::axisIndex(const std::string & control) const
{
	const auto found = axes.find(control);
	if(found == axes.end())
		return std::nullopt;
	return found->second;
}

bool VirtualControllerProfile::isTrigger(const std::string & control) const
{
	const auto found = axisKinds.find(control);
	return found != axisKinds.end() && found->second == "trigger";
}

std::string VirtualControllerProfile::mappingBindings() const
{
	std::string result;
	for(const auto & [token, control] : sdlBindings)
	{
		const auto button = buttons.find(control);
		if(button != buttons.end())
		{
			result += token + ":b" + std::to_string(button->second) + ",";
			continue;
		}
		const auto axis = axes.find(control);
		if(axis != axes.end())
			result += token + ":a" + std::to_string(axis->second) + ",";
	}
	return result;
}

SDLVirtualController::SDLVirtualController(std::string alias, VirtualControllerProfile profile)
	: profile(std::move(profile))
	, alias(std::move(alias))
{
}

SDLVirtualController::~SDLVirtualController()
{
	std::string error;
	if(isAttached())
		detach(error);
}

SDLVirtualController::SDLVirtualController(SDLVirtualController && other) noexcept
	: profile(std::move(other.profile))
	, alias(std::move(other.alias))
	, deviceIndex(other.deviceIndex)
	, instanceId(other.instanceId)
	, controllerHandle(other.controllerHandle)
	, handle(other.handle)
{
	other.deviceIndex = -1;
	other.instanceId = -1;
	other.controllerHandle = nullptr;
	other.handle = nullptr;
}

SDLVirtualController & SDLVirtualController::operator=(SDLVirtualController && other) noexcept
{
	if(this != &other)
	{
		std::string error;
		if(isAttached())
			detach(error);
		profile = std::move(other.profile);
		alias = std::move(other.alias);
		deviceIndex = other.deviceIndex;
		instanceId = other.instanceId;
		controllerHandle = other.controllerHandle;
		handle = other.handle;
		other.deviceIndex = -1;
		other.instanceId = -1;
		other.controllerHandle = nullptr;
		other.handle = nullptr;
	}
	return *this;
}

std::string SDLVirtualController::buildMappingString() const
{
	char guid[33] = {};
	SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(deviceIndex), guid, sizeof(guid));

	std::string mapping = guid;
	mapping += "," + profile.displayName + ",";
	mapping += profile.mappingBindings();
	mapping += "platform:";
	mapping += SDL_GetPlatform();
	if(!profile.sdlTypeHint.empty())
		mapping += ",type:" + profile.sdlTypeHint;
	return mapping;
}

bool SDLVirtualController::attach(std::string & error)
{
	if(isAttached())
	{
		error = "device '" + alias + "' is already attached";
		return false;
	}

	if(!SDL_WasInit(SDL_INIT_JOYSTICK) || !SDL_WasInit(SDL_INIT_GAMECONTROLLER))
	{
		error = "SDL joystick/gamecontroller subsystems are not initialized";
		return false;
	}

#if SDL_VERSION_ATLEAST(2, 24, 0)
	SDL_VirtualJoystickDesc descriptor{};
	descriptor.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
	descriptor.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
	descriptor.nbuttons = static_cast<int>(profile.buttons.size());
	descriptor.naxes = static_cast<int>(profile.axes.size());
	descriptor.nhats = 0;
	descriptor.vendor_id = profile.vendorId;
	descriptor.product_id = profile.productId;
	descriptor.name = profile.displayName.c_str();
	deviceIndex = SDL_JoystickAttachVirtualEx(&descriptor);
#else
	// SDL < 2.24 cannot carry vendor/product identity; explicit GameController
	// mapping registered below keeps the device consumable by the production chain
	deviceIndex = SDL_JoystickAttachVirtual(
		SDL_JOYSTICK_TYPE_GAMECONTROLLER,
		static_cast<int>(profile.buttons.size()),
		static_cast<int>(profile.axes.size()),
		0);
#endif
	if(deviceIndex < 0)
	{
		error = "SDL_JoystickAttachVirtual failed: " + std::string(SDL_GetError());
		deviceIndex = -1;
		return false;
	}

	const std::string mapping = buildMappingString();
	if(SDL_GameControllerAddMapping(mapping.c_str()) < 0)
	{
		error = "SDL_GameControllerAddMapping failed: " + std::string(SDL_GetError());
		SDL_JoystickDetachVirtual(deviceIndex);
		deviceIndex = -1;
		return false;
	}

	// Open through the GameController API exactly like the production input
	// chain does; SDL refcounts opens so VCMI can open the same device too
	controllerHandle = SDL_GameControllerOpen(deviceIndex);
	if(!controllerHandle)
	{
		error = "SDL_GameControllerOpen failed: " + std::string(SDL_GetError());
		SDL_JoystickDetachVirtual(deviceIndex);
		deviceIndex = -1;
		return false;
	}

	handle = SDL_GameControllerGetJoystick(controllerHandle);
	if(!handle)
	{
		error = "SDL_GameControllerGetJoystick failed: " + std::string(SDL_GetError());
		SDL_GameControllerClose(controllerHandle);
		controllerHandle = nullptr;
		SDL_JoystickDetachVirtual(deviceIndex);
		deviceIndex = -1;
		return false;
	}

	instanceId = SDL_JoystickInstanceID(handle);
	if(instanceId < 0)
	{
		error = "SDL_JoystickInstanceID failed: " + std::string(SDL_GetError());
		SDL_GameControllerClose(controllerHandle);
		controllerHandle = nullptr;
		handle = nullptr;
		SDL_JoystickDetachVirtual(deviceIndex);
		deviceIndex = -1;
		return false;
	}

	return true;
}

bool SDLVirtualController::detach(std::string & error)
{
	if(!isAttached())
	{
		error = "device '" + alias + "' is not attached";
		return false;
	}

	bool success = true;

	std::string neutralizeError;
	if(!neutralize(neutralizeError))
	{
		error = "neutralize failed before detach: " + neutralizeError;
		success = false;
	}

	if(handle)
	{
		SDL_GameControllerClose(controllerHandle);
		controllerHandle = nullptr;
		handle = nullptr;
	}

	// Joystick device indices are not stable across attach/detach of other
	// devices; resolve the current index from the stable instance id
	int currentIndex = -1;
	for(int candidate = 0; candidate < SDL_NumJoysticks(); ++candidate)
	{
		if(SDL_JoystickGetDeviceInstanceID(candidate) == instanceId)
		{
			currentIndex = candidate;
			break;
		}
	}
	if(currentIndex < 0)
	{
		if(!error.empty())
			error += "; ";
		error += "device '" + alias + "' instance " + std::to_string(instanceId) + " no longer enumerated";
		success = false;
	}
	else if(SDL_JoystickDetachVirtual(currentIndex) != 0)
	{
		if(!error.empty())
			error += "; ";
		error += "SDL_JoystickDetachVirtual failed: " + std::string(SDL_GetError());
		success = false;
	}

	deviceIndex = -1;
	instanceId = -1;
	return success;
}

bool SDLVirtualController::verifyAttachment(std::string & error) const
{
	if(!isAttached())
	{
		error = "device '" + alias + "' is not attached";
		return false;
	}

	if(!SDL_JoystickIsVirtual(deviceIndex))
	{
		error = "device '" + alias + "' is not reported as virtual";
		return false;
	}

	if(!SDL_IsGameController(deviceIndex))
	{
		error = "device '" + alias + "' is not classified as a game controller";
		return false;
	}

	if(SDL_JoystickNumButtons(handle) != static_cast<int>(profile.buttons.size()))
	{
		error = "device '" + alias + "' reports " + std::to_string(SDL_JoystickNumButtons(handle))
			+ " buttons, expected " + std::to_string(profile.buttons.size());
		return false;
	}

	if(SDL_JoystickNumAxes(handle) != static_cast<int>(profile.axes.size()))
	{
		error = "device '" + alias + "' reports " + std::to_string(SDL_JoystickNumAxes(handle))
			+ " axes, expected " + std::to_string(profile.axes.size());
		return false;
	}

	SDL_GameController * controller = SDL_GameControllerFromInstanceID(instanceId);
	if(!controller)
	{
		error = "device '" + alias + "' has no open game controller for instance " + std::to_string(instanceId);
		return false;
	}

	char guid[33] = {};
	SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(handle), guid, sizeof(guid));
	if(SDL_GameControllerMappingForGUID(SDL_JoystickGetGUID(handle)) == nullptr)
	{
		error = "device '" + alias + "' (GUID " + guid + ") has no registered mapping";
		return false;
	}

	return true;
}

bool SDLVirtualController::setButton(const std::string & control, bool pressed, std::string & error)
{
	if(!isAttached())
	{
		error = "device '" + alias + "' is not attached";
		return false;
	}
	const auto index = profile.buttonIndex(control);
	if(!index)
	{
		error = "device '" + alias + "' has no button control '" + control + "'";
		return false;
	}
	if(SDL_JoystickSetVirtualButton(handle, *index, pressed ? 1 : 0) != 0)
	{
		error = "SDL_JoystickSetVirtualButton failed for '" + control + "': " + std::string(SDL_GetError());
		return false;
	}
	return true;
}

bool SDLVirtualController::setAxis(const std::string & control, double normalized, std::string & error)
{
	if(!isAttached())
	{
		error = "device '" + alias + "' is not attached";
		return false;
	}
	const auto index = profile.axisIndex(control);
	if(!index)
	{
		error = "device '" + alias + "' has no axis control '" + control + "'";
		return false;
	}

	double clamped = normalized;
	Sint16 value = 0;
	if(profile.isTrigger(control))
	{
		clamped = std::clamp(normalized, 0.0, 1.0);
		value = static_cast<Sint16>(clamped * SDL_JOYSTICK_AXIS_MAX);
	}
	else
	{
		clamped = std::clamp(normalized, -1.0, 1.0);
		value = static_cast<Sint16>(clamped * SDL_JOYSTICK_AXIS_MAX);
	}

	if(SDL_JoystickSetVirtualAxis(handle, *index, value) != 0)
	{
		error = "SDL_JoystickSetVirtualAxis failed for '" + control + "': " + std::string(SDL_GetError());
		return false;
	}
	return true;
}

bool SDLVirtualController::neutralize(std::string & error)
{
	if(!isAttached())
	{
		error = "device '" + alias + "' is not attached";
		return false;
	}

	bool success = true;
	for(const auto & [control, index] : profile.buttons)
	{
		if(SDL_JoystickSetVirtualButton(handle, index, 0) != 0)
		{
			error += (error.empty() ? "" : "; ") + std::string("failed to release button '") + control + "': " + SDL_GetError();
			success = false;
		}
	}
	for(const auto & [control, index] : profile.axes)
	{
		if(SDL_JoystickSetVirtualAxis(handle, index, 0) != 0)
		{
			error += (error.empty() ? "" : "; ") + std::string("failed to neutralize axis '") + control + "': " + SDL_GetError();
			success = false;
		}
	}
	return success;
}

std::string SDLVirtualController::describeIdentity() const
{
	std::string description = profile.id + "/" + alias;
	if(isAttached())
	{
		char guid[33] = {};
		SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(deviceIndex), guid, sizeof(guid));
		description += " index=" + std::to_string(deviceIndex) + " instance=" + std::to_string(instanceId) + " guid=" + guid;
	}
	else
		description += " detached";
	return description;
}

}
