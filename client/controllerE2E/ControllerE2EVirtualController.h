/*
 * ControllerE2EVirtualController.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include <SDL_version.h>
#include <SDL_joystick.h>
#include <SDL_gamecontroller.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

class JsonNode;

namespace ControllerE2E
{

/// Data-only description of a controller family. Contains no VCMI action,
/// binding or UI rules: scenarios use physical-position control names and the
/// production input chain resolves everything else.
struct VirtualControllerProfile
{
	static constexpr const char * SCHEMA = "vcmi.controller-e2e/profile/v1";

	std::string id;
	std::string displayName;
	Uint16 vendorId = 0;
	Uint16 productId = 0;
	/// SDL GameController mapping hint such as "ps5" or "xboxone"
	std::string sdlTypeHint;

	/// physical control name -> raw virtual joystick button index
	std::map<std::string, int> buttons;
	/// physical control name -> raw virtual joystick axis index
	std::map<std::string, int> axes;
	/// axis control name -> "stick" (-1..1) or "trigger" (0..1)
	std::map<std::string, std::string> axisKinds;
	/// SDL GameController mapping token ("a", "dpup", "leftx", ...) -> physical control name
	std::map<std::string, std::string> sdlBindings;

	/// Parse and validate a profile document. All validation problems are
	/// appended to errors; the returned profile is usable only when errors is empty.
	static std::optional<VirtualControllerProfile> parse(const JsonNode & node, std::vector<std::string> & errors);

	std::optional<int> buttonIndex(const std::string & control) const;
	std::optional<int> axisIndex(const std::string & control) const;
	bool isTrigger(const std::string & control) const;
	int buttonCount() const { return static_cast<int>(buttons.size()); }
	int axisCount() const { return static_cast<int>(axes.size()); }

	/// Builds the button/axis section of an SDL GameController mapping string
	std::string mappingBindings() const;
};

/// RAII owner of one SDL virtual joystick. State changes go through
/// SDL_JoystickSetVirtualButton/Axis only; the normal SDL event pump then
/// turns them into device events consumed by the production input chain.
class SDLVirtualController
{
	VirtualControllerProfile profile;
	std::string alias;
	int deviceIndex = -1;
	SDL_JoystickID instanceId = -1;
	/// Opened through the GameController API to mirror the production chain;
	/// the joystick handle used for SDL_JoystickSetVirtual* comes from it
	SDL_GameController * controllerHandle = nullptr;
	SDL_Joystick * handle = nullptr;

	std::string buildMappingString() const;

public:
	SDLVirtualController(std::string alias, VirtualControllerProfile profile);
	~SDLVirtualController();

	SDLVirtualController(const SDLVirtualController &) = delete;
	SDLVirtualController & operator=(const SDLVirtualController &) = delete;
	SDLVirtualController(SDLVirtualController && other) noexcept;
	SDLVirtualController & operator=(SDLVirtualController && other) noexcept;

	/// Attaches the virtual device and registers its GameController mapping.
	/// Requires SDL joystick + gamecontroller subsystems to be initialized.
	bool attach(std::string & error);
	/// Neutralizes all state, closes the handle and detaches the device.
	/// Failures are reported through error and must reach the run manifest.
	bool detach(std::string & error);

	/// Verifies post-attach identity: virtual flag, GUID, mapping, instance id,
	/// control counts and GameController classification.
	bool verifyAttachment(std::string & error) const;

	bool isAttached() const { return deviceIndex >= 0; }
	int getDeviceIndex() const { return deviceIndex; }
	SDL_JoystickID getInstanceId() const { return instanceId; }
	const std::string & getAlias() const { return alias; }
	const VirtualControllerProfile & getProfile() const { return profile; }

	bool setButton(const std::string & control, bool pressed, std::string & error);
	/// Models buttons already held when a device is enumerated. Raw state changes
	/// through SDL's virtual-device API, but their pre-existing press edges are filtered.
	bool primeButtonsPressed(const std::vector<std::string> & controls, std::string & error);
	/// normalized value: -1..1 for sticks, 0..1 for triggers
	bool setAxis(const std::string & control, double normalized, std::string & error);
	/// Returns every control to its neutral state
	bool neutralize(std::string & error);

	/// Identity description for the run manifest
	std::string describeIdentity() const;
};

/// Embedded profile documents. These are pure data parsed through the same
/// strict profile contract; adding a family only adds a document.
std::map<std::string, std::string> builtinProfileDocuments();

}
