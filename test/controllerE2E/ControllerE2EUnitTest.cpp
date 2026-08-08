/*
 * ControllerE2EUnitTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../client/StdInc.h"

#include "../client/controllerE2E/ControllerE2EScenario.h"
#include "../client/controllerE2E/ControllerE2EVirtualController.h"

#include "../lib/json/JsonNode.h"

#include <gtest/gtest.h>

#include <SDL.h>
#include <SDL_gamecontroller.h>
#include <SDL_joystick.h>

using namespace ControllerE2E;

namespace
{

const char * VALID_SCENARIO = R"(
{
	"schema": "vcmi.controller-e2e/v1",
	"id": "unit.valid",
	"profile": "dualsense",
	"timeout_ms": 5000,
	"fixture": {},
	"prelude": [
		{"op": "attach", "device": "pad0", "profile": "dualsense"}
	],
	"steps": [
		{"op": "press", "device": "pad0", "control": "south"},
		{"op": "wait_frames", "frames": 2},
		{"op": "release", "device": "pad0", "control": "south"},
		{"op": "request_shutdown"}
	]
}
)";

std::string replaceToken(const std::string & source, const std::string & token, const std::string & replacement)
{
	std::string result = source;
	const size_t position = result.find(token);
	EXPECT_NE(position, std::string::npos) << "token not found: " << token;
	result.replace(position, token.size(), replacement);
	return result;
}

}

class ScenarioProtocolTest : public testing::Test
{
};

TEST_F(ScenarioProtocolTest, AcceptsValidScenarioWithPreludeAndSteps)
{
	const auto result = parseScenarioText(VALID_SCENARIO);
	EXPECT_TRUE(result.errors.empty());
	ASSERT_TRUE(result.scenario.has_value());
	EXPECT_EQ(result.scenario->id, "unit.valid");
	EXPECT_EQ(result.scenario->profileId, "dualsense");
	EXPECT_EQ(result.scenario->timeoutMs, 5000);
	EXPECT_EQ(result.scenario->prelude.size(), 1);
	EXPECT_EQ(result.scenario->steps.size(), 4);
	EXPECT_EQ(result.scenario->prelude.at(0).kind, ScenarioStep::Kind::ATTACH);
	EXPECT_EQ(result.scenario->steps.at(3).kind, ScenarioStep::Kind::REQUEST_SHUTDOWN);
}

TEST_F(ScenarioProtocolTest, RejectsWrongSchemaVersion)
{
	const auto result = parseScenarioText(replaceToken(VALID_SCENARIO, "vcmi.controller-e2e/v1", "vcmi.controller-e2e/v9"));
	EXPECT_FALSE(result.ok());
}

TEST_F(ScenarioProtocolTest, RejectsUnknownTopLevelField)
{
	const auto result = parseScenarioText(replaceToken(VALID_SCENARIO, "\"fixture\": {}", "\"fixture\": {}, \"harness_hook\": true"));
	EXPECT_FALSE(result.ok());
}

TEST_F(ScenarioProtocolTest, RejectsUnknownOperation)
{
	const auto result = parseScenarioText(replaceToken(VALID_SCENARIO, "\"op\": \"request_shutdown\"", "\"op\": \"call_cpp_function\""));
	EXPECT_FALSE(result.ok());
}

TEST_F(ScenarioProtocolTest, RejectsUnknownStepField)
{
	const auto result = parseScenarioText(replaceToken(VALID_SCENARIO, "\"op\": \"press\", \"device\": \"pad0\", \"control\": \"south\"",
		"\"op\": \"press\", \"device\": \"pad0\", \"control\": \"south\", \"shortcut\": \"globalAccept\""));
	EXPECT_FALSE(result.ok());
}

TEST_F(ScenarioProtocolTest, RejectsMissingRequiredField)
{
	const auto result = parseScenarioText(replaceToken(VALID_SCENARIO, "\"op\": \"press\", \"device\": \"pad0\", \"control\": \"south\"", "\"op\": \"press\", \"device\": \"pad0\""));
	EXPECT_FALSE(result.ok());
}

TEST_F(ScenarioProtocolTest, RejectsWaitUntilWithoutTimeout)
{
	const std::string scenario = replaceToken(VALID_SCENARIO,
		"{\"op\": \"wait_frames\", \"frames\": 2}",
		"{\"op\": \"wait_until\", \"probe\": \"runtime\", \"field\": \"input_mode\", \"equals\": \"controller\"}");
	const auto result = parseScenarioText(scenario);
	EXPECT_FALSE(result.ok());
}

TEST_F(ScenarioProtocolTest, RejectsAxisValueOutsideNormalizedRange)
{
	const std::string scenario = replaceToken(VALID_SCENARIO,
		"{\"op\": \"wait_frames\", \"frames\": 2}",
		"{\"op\": \"set_axis\", \"device\": \"pad0\", \"control\": \"left_stick_x\", \"value\": 1.5}");
	const auto result = parseScenarioText(scenario);
	EXPECT_FALSE(result.ok());
}

TEST_F(ScenarioProtocolTest, RejectsReferenceToDeviceThatIsNeverAttached)
{
	const std::string scenario = replaceToken(VALID_SCENARIO, "\"op\": \"press\", \"device\": \"pad0\"", "\"op\": \"press\", \"device\": \"ghost\"");
	const auto result = parseScenarioText(scenario);
	EXPECT_FALSE(result.ok());
}

TEST_F(ScenarioProtocolTest, RejectsDuplicateAttachAlias)
{
	const std::string scenario = replaceToken(VALID_SCENARIO,
		"[",
		"[{\"op\": \"attach\", \"device\": \"pad0\", \"profile\": \"dualsense\"}, ") ;
	const auto result = parseScenarioText(scenario);
	EXPECT_FALSE(result.ok());
}

TEST_F(ScenarioProtocolTest, RejectsFrameOperationInsidePrelude)
{
	const std::string scenario = replaceToken(VALID_SCENARIO,
		"\"prelude\": [\n\t\t{\"op\": \"attach\", \"device\": \"pad0\", \"profile\": \"dualsense\"}\n\t]",
		"\"prelude\": [\n\t\t{\"op\": \"attach\", \"device\": \"pad0\", \"profile\": \"dualsense\"},\n\t\t{\"op\": \"press\", \"device\": \"pad0\", \"control\": \"south\"}\n\t]");
	const auto result = parseScenarioText(scenario);
	EXPECT_FALSE(result.ok());
}

TEST_F(ScenarioProtocolTest, RejectsEmptyStepList)
{
	const std::string scenario = replaceToken(VALID_SCENARIO,
		"\"steps\": [\n\t\t{\"op\": \"press\", \"device\": \"pad0\", \"control\": \"south\"},\n\t\t{\"op\": \"wait_frames\", \"frames\": 2},\n\t\t{\"op\": \"release\", \"device\": \"pad0\", \"control\": \"south\"},\n\t\t{\"op\": \"request_shutdown\"}\n\t]",
		"\"steps\": []");
	const auto result = parseScenarioText(scenario);
	EXPECT_FALSE(result.ok());
}

TEST_F(ScenarioProtocolTest, RejectsJsoncCommentsBecauseProtocolIsStrictJson)
{
	const std::string scenario = replaceToken(VALID_SCENARIO, "\"id\": \"unit.valid\"", "\"id\": \"unit.valid\" // escaped action name");
	const auto result = parseScenarioText(scenario);
	EXPECT_FALSE(result.ok());
}

TEST_F(ScenarioProtocolTest, RejectsMalformedJson)
{
	const auto result = parseScenarioText("{\"schema\": \"vcmi.controller-e2e/v1\", ");
	EXPECT_FALSE(result.ok());
}

TEST_F(ScenarioProtocolTest, DeviceValidationRejectsUnknownProfile)
{
	const auto parseResult = parseScenarioText(VALID_SCENARIO);
	ASSERT_TRUE(parseResult.scenario.has_value());
	std::vector<std::string> errors;
	EXPECT_FALSE(validateScenarioDevices(*parseResult.scenario, {"synthetic-generic"}, errors));
	EXPECT_FALSE(errors.empty());
}

TEST_F(ScenarioProtocolTest, DeviceValidationAcceptsKnownProfiles)
{
	const auto parseResult = parseScenarioText(VALID_SCENARIO);
	ASSERT_TRUE(parseResult.scenario.has_value());
	std::vector<std::string> errors;
	EXPECT_TRUE(validateScenarioDevices(*parseResult.scenario, {"dualsense", "synthetic-generic"}, errors));
	EXPECT_TRUE(errors.empty());
}

class ProfileContractTest : public testing::Test
{
};

TEST_F(ProfileContractTest, AllBuiltinProfilesParseThroughTheSameDataContract)
{
	const auto documents = builtinProfileDocuments();
	ASSERT_GE(documents.size(), 3u);
	for(const auto & [id, document] : documents)
	{
		JsonNode node(document.c_str(), document.size(), "profile-" + id);
		std::vector<std::string> errors;
		const auto profile = VirtualControllerProfile::parse(node, errors);
		ASSERT_TRUE(profile.has_value()) << "profile " << id << " failed to parse";
		EXPECT_TRUE(errors.empty());
		EXPECT_EQ(profile->id, id);
		EXPECT_FALSE(profile->buttons.empty());
		EXPECT_FALSE(profile->axes.empty());
		EXPECT_FALSE(profile->sdlBindings.empty());
	}
}

TEST_F(ProfileContractTest, DualSenseProfileCoversRequiredControlSet)
{
	const auto documents = builtinProfileDocuments();
	JsonNode node(documents.at("dualsense").c_str(), documents.at("dualsense").size(), "dualsense");
	std::vector<std::string> errors;
	const auto profile = VirtualControllerProfile::parse(node, errors);
	ASSERT_TRUE(profile.has_value());

	for(const char * control : {"south", "east", "west", "north", "l1", "r1",
		"create", "options", "ps", "touchpad_click", "dpad_up", "dpad_down", "dpad_left", "dpad_right",
		"left_stick_click", "right_stick_click"})
		EXPECT_TRUE(profile->buttonIndex(control).has_value()) << "missing DualSense control " << control;

	for(const char * control : {"left_stick_x", "left_stick_y", "right_stick_x", "right_stick_y", "l2", "r2"})
		EXPECT_TRUE(profile->axisIndex(control).has_value()) << "missing DualSense axis " << control;

	EXPECT_TRUE(profile->isTrigger("l2"));
	EXPECT_TRUE(profile->isTrigger("r2"));
	EXPECT_FALSE(profile->isTrigger("left_stick_x"));
	EXPECT_EQ(profile->expectedPresentation, "playstation");
}

TEST_F(ProfileContractTest, RejectsAxisKindForUnknownAxis)
{
	const std::string document = R"({
		"schema": "vcmi.controller-e2e/profile/v1",
		"id": "broken", "display_name": "broken", "vendor_id": 1, "product_id": 1,
		"buttons": {"south": 0},
		"axes": {"left_stick_x": 0},
		"axis_kinds": {"ghost_axis": "stick", "left_stick_x": "stick"},
		"sdl_bindings": {"a": "south", "leftx": "left_stick_x"}
	})";
	JsonNode node(document.c_str(), document.size(), "broken");
	std::vector<std::string> errors;
	EXPECT_FALSE(VirtualControllerProfile::parse(node, errors).has_value());
}

TEST_F(ProfileContractTest, RejectsBindingForUnknownControl)
{
	const std::string document = R"({
		"schema": "vcmi.controller-e2e/profile/v1",
		"id": "broken", "display_name": "broken", "vendor_id": 1, "product_id": 1,
		"buttons": {"south": 0},
		"axes": {"left_stick_x": 0},
		"axis_kinds": {"left_stick_x": "stick"},
		"sdl_bindings": {"a": "ghost"}
	})";
	JsonNode node(document.c_str(), document.size(), "broken");
	std::vector<std::string> errors;
	EXPECT_FALSE(VirtualControllerProfile::parse(node, errors).has_value());
}

class VirtualDeviceTest : public testing::Test
{
protected:
	void SetUp() override
	{
		ASSERT_EQ(SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER), 0) << SDL_GetError();
	}

	void TearDown() override
	{
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
	}

	VirtualControllerProfile profileById(const std::string & id)
	{
		const auto documents = builtinProfileDocuments();
		JsonNode node(documents.at(id).c_str(), documents.at(id).size(), id);
		std::vector<std::string> errors;
		auto profile = VirtualControllerProfile::parse(node, errors);
		EXPECT_TRUE(profile.has_value()) << id;
		return *profile;
	}

	int drainEvents(std::vector<SDL_Event> & collected)
	{
		SDL_PumpEvents();
		SDL_Event event;
		int count = 0;
		while(SDL_PollEvent(&event) == 1)
		{
			collected.push_back(event);
			++count;
		}
		return count;
	}
};

TEST_F(VirtualDeviceTest, AttachCreatesVirtualGameControllerVisibleToSdlEnumeration)
{
	const int joysticksBefore = SDL_NumJoysticks();
	SDLVirtualController device("pad0", profileById("dualsense"));
	std::string error;
	ASSERT_TRUE(device.attach(error)) << error;
	EXPECT_TRUE(device.verifyAttachment(error)) << error;
	EXPECT_EQ(SDL_NumJoysticks(), joysticksBefore + 1);
	EXPECT_TRUE(SDL_JoystickIsVirtual(device.getDeviceIndex()));
	EXPECT_TRUE(SDL_IsGameController(device.getDeviceIndex()));
	EXPECT_GE(device.getInstanceId(), 0);
	EXPECT_TRUE(device.detach(error)) << error;
	EXPECT_EQ(SDL_NumJoysticks(), joysticksBefore);
}

TEST_F(VirtualDeviceTest, VirtualButtonStateGeneratesControllerEventThroughNormalSdlPump)
{
	SDLVirtualController device("pad0", profileById("dualsense"));
	std::string error;
	ASSERT_TRUE(device.attach(error)) << error;

	std::vector<SDL_Event> events;
	drainEvents(events); // discard attach events

	ASSERT_TRUE(device.setButton("south", true, error)) << error;

	bool sawButtonDown = false;
	drainEvents(events);
	for(const auto & event : events)
	{
		if(event.type == SDL_CONTROLLERBUTTONDOWN && event.cbutton.button == SDL_CONTROLLER_BUTTON_A)
			sawButtonDown = true;
	}
	EXPECT_TRUE(sawButtonDown) << "virtual button state must reach VCMI through SDL event generation";

	ASSERT_TRUE(device.setButton("south", false, error)) << error;
	bool sawButtonUp = false;
	events.clear();
	drainEvents(events);
	for(const auto & event : events)
	{
		if(event.type == SDL_CONTROLLERBUTTONUP && event.cbutton.button == SDL_CONTROLLER_BUTTON_A)
			sawButtonUp = true;
	}
	EXPECT_TRUE(sawButtonUp);
	EXPECT_TRUE(device.detach(error));
}

TEST_F(VirtualDeviceTest, TriggerAxisUsesNonNegativeRangeAndStickUsesSignedRange)
{
	SDLVirtualController device("pad0", profileById("dualsense"));
	std::string error;
	ASSERT_TRUE(device.attach(error)) << error;

	ASSERT_TRUE(device.setAxis("r2", 1.0, error)) << error;
	ASSERT_TRUE(device.setAxis("left_stick_x", -1.0, error)) << error;

	std::vector<SDL_Event> events;
	drainEvents(events);
	bool sawTrigger = false;
	bool sawStickNegative = false;
	for(const auto & event : events)
	{
		if(event.type != SDL_CONTROLLERAXISMOTION)
			continue;
		if(event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT && event.caxis.value > SDL_JOYSTICK_AXIS_MAX / 2)
			sawTrigger = true;
		if(event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX && event.caxis.value < -SDL_JOYSTICK_AXIS_MAX / 2)
			sawStickNegative = true;
	}
	EXPECT_TRUE(sawTrigger);
	EXPECT_TRUE(sawStickNegative);

	ASSERT_TRUE(device.neutralize(error)) << error;
	EXPECT_TRUE(device.detach(error));
}

TEST_F(VirtualDeviceTest, UnknownControlFailsClosed)
{
	SDLVirtualController device("pad0", profileById("dualsense"));
	std::string error;
	ASSERT_TRUE(device.attach(error)) << error;
	EXPECT_FALSE(device.setButton("globalAccept", true, error));
	EXPECT_FALSE(error.empty());
	error.clear();
	EXPECT_FALSE(device.setAxis("left_stick_z", 0.5, error));
	EXPECT_FALSE(error.empty());
	EXPECT_TRUE(device.detach(error));
}

TEST_F(VirtualDeviceTest, DetachWithoutAttachFailsClosed)
{
	SDLVirtualController device("pad0", profileById("dualsense"));
	std::string error;
	EXPECT_FALSE(device.detach(error));
	EXPECT_FALSE(error.empty());
}

TEST_F(VirtualDeviceTest, TwoDevicesCoexistWithDistinctInstanceIds)
{
	SDLVirtualController first("pad0", profileById("dualsense"));
	SDLVirtualController second("pad1", profileById("synthetic-generic"));
	std::string error;
	ASSERT_TRUE(first.attach(error)) << error;
	ASSERT_TRUE(second.attach(error)) << error;
	EXPECT_NE(first.getInstanceId(), second.getInstanceId());
	EXPECT_TRUE(first.verifyAttachment(error)) << error;
	EXPECT_TRUE(second.verifyAttachment(error)) << error;
	EXPECT_TRUE(first.detach(error));
	EXPECT_TRUE(second.detach(error));
}

TEST_F(VirtualDeviceTest, ReconnectAfterDetachProducesFreshAttachment)
{
	SDLVirtualController device("pad0", profileById("dualsense"));
	std::string error;
	ASSERT_TRUE(device.attach(error)) << error;
	const SDL_JoystickID firstInstance = device.getInstanceId();
	ASSERT_TRUE(device.detach(error)) << error;
	ASSERT_TRUE(device.attach(error)) << error;
	EXPECT_TRUE(device.isAttached());
	EXPECT_GE(device.getInstanceId(), 0);
	EXPECT_NE(device.getInstanceId(), firstInstance);
	EXPECT_TRUE(device.detach(error));
}

/// Negative mutation: a constructed controller event without any attached
/// virtual device must not fabricate device enumeration. If this oracle ever
/// turns GREEN for the mutation, the E2E layer cannot distinguish real device
/// state from injected events.
TEST_F(VirtualDeviceTest, MutationDirectControllerEventWithoutDeviceDoesNotCreateEnumerationEvidence)
{
	// Baseline may include real physical controllers attached to the host;
	// the oracle is that injection adds nothing, not that the host is empty
	const int joysticksBefore = SDL_NumJoysticks();
	int virtualBefore = 0;
	for(int index = 0; index < joysticksBefore; ++index)
		virtualBefore += SDL_JoystickIsVirtual(index) ? 1 : 0;

	SDL_Event event{};
	event.type = SDL_CONTROLLERBUTTONDOWN;
	event.cbutton.type = SDL_CONTROLLERBUTTONDOWN;
	event.cbutton.which = 42;
	event.cbutton.button = SDL_CONTROLLER_BUTTON_A;
	event.cbutton.state = SDL_PRESSED;
	SDL_PushEvent(&event);

	std::vector<SDL_Event> events;
	drainEvents(events);

	EXPECT_EQ(SDL_NumJoysticks(), joysticksBefore) << "injected controller event must not enumerate a device";
	int virtualAfter = 0;
	for(int index = 0; index < SDL_NumJoysticks(); ++index)
		virtualAfter += SDL_JoystickIsVirtual(index) ? 1 : 0;
	EXPECT_EQ(virtualAfter, virtualBefore) << "injected controller event must not fabricate a virtual device";
}

/// Negative mutation: pushing controller events for an attached device must
/// not change the virtual joystick raw state. Raw state may only change via
/// SDL_JoystickSetVirtual*; if this fails, the harness could bypass the
/// production event generation path.
TEST_F(VirtualDeviceTest, MutationInjectedEventDoesNotChangeVirtualRawState)
{
	SDLVirtualController device("pad0", profileById("dualsense"));
	std::string error;
	ASSERT_TRUE(device.attach(error)) << error;
	std::vector<SDL_Event> events;
	drainEvents(events);

	SDL_Event injected{};
	injected.type = SDL_CONTROLLERBUTTONDOWN;
	injected.cbutton.type = SDL_CONTROLLERBUTTONDOWN;
	injected.cbutton.which = device.getInstanceId();
	injected.cbutton.button = SDL_CONTROLLER_BUTTON_A;
	injected.cbutton.state = SDL_PRESSED;
	SDL_PushEvent(&injected);
	drainEvents(events);

	SDL_PumpEvents();
	SDL_Joystick * opened = SDL_JoystickOpen(SDL_NumJoysticks() - 1);
	ASSERT_NE(opened, nullptr) << SDL_GetError();
	// The already-opened driver handle owns the device; SDL returns the same
	// instance. Reading state through it reflects raw virtual state only.
	EXPECT_EQ(SDL_JoystickGetButton(opened, 0), 0)
		<< "injected SDL_CONTROLLER* events must not mutate virtual joystick state";
	SDL_JoystickClose(opened);
	EXPECT_TRUE(device.detach(error));
}
