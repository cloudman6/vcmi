/*
 * ControllerE2EExecutor.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "ControllerE2EExecutor.h"

#include "ControllerE2EArtifacts.h"
#include "ControllerE2EProbes.h"

#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../eventsSDL/InputHandler.h"
#include "../eventsSDL/InputSourceGameController.h"
#include "../gui/ShortcutHandler.h"
#include "../gui/WindowHandler.h"
#include "../render/IScreenHandler.h"
#include "../render/IRenderHandler.h"
#include "../render/IImage.h"
#include "../renderSDL/ScreenHandler.h"
#include "../windows/GUIClasses.h"
#include "../windows/CWindowObject.h"

#include "../../lib/GameLibrary.h"
#include "../../lib/CConfigHandler.h"
#include "../../lib/VCMIDirs.h"
#include "../../lib/json/JsonNode.h"
#include "../../lib/spells/CSpellHandler.h"
#include "../../lib/spells/SpellSchoolHandler.h"
#include "../../lib/texts/CGeneralTextHandler.h"
#include "../../lib/texts/TextOperations.h"
#include "../../lib/filesystem/ResourcePath.h"

#include <SDL.h>

#include <boost/filesystem.hpp>
#include <fstream>

#include <array>
#include <cstring>

namespace ControllerE2E
{

namespace
{

std::unique_ptr<ControllerE2EExecutor> globalExecutor;

/// Minimal SHA-256 used only for scenario/profile digests in the run manifest
struct Sha256
{
	std::array<uint32_t, 8> state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	std::array<uint32_t, 64> table = []()
	{
		std::array<uint32_t, 64> result{};
		uint32_t candidate = 2;
		for(size_t index = 0; index < 64; ++candidate)
		{
			bool prime = true;
			for(uint32_t divisor = 2; divisor * divisor <= candidate; ++divisor)
				if(candidate % divisor == 0)
					prime = false;
			if(!prime)
				continue;
			double root = index < 8 ? std::cbrt(static_cast<double>(candidate)) : std::sqrt(static_cast<double>(candidate));
			result[index++] = static_cast<uint32_t>((root - std::floor(root)) * 4294967296.0);
		}
		return result;
	}();
	uint64_t totalLength = 0;
	std::vector<uint8_t> pending;

	static uint32_t rotr(uint32_t value, int bits) { return (value >> bits) | (value << (32 - bits)); }

	void processBlock(const uint8_t * block)
	{
		std::array<uint32_t, 64> words{};
		for(int index = 0; index < 16; ++index)
			words[index] = (uint32_t(block[index * 4]) << 24) | (uint32_t(block[index * 4 + 1]) << 16) | (uint32_t(block[index * 4 + 2]) << 8) | uint32_t(block[index * 4 + 3]);
		for(int index = 16; index < 64; ++index)
		{
			const uint32_t s0 = rotr(words[index - 15], 7) ^ rotr(words[index - 15], 18) ^ (words[index - 15] >> 3);
			const uint32_t s1 = rotr(words[index - 2], 17) ^ rotr(words[index - 2], 19) ^ (words[index - 2] >> 10);
			words[index] = words[index - 16] + s0 + words[index - 7] + s1;
		}
		uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4], f = state[5], g = state[6], h = state[7];
		for(int index = 0; index < 64; ++index)
		{
			const uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
			const uint32_t ch = (e & f) ^ (~e & g);
			const uint32_t temp1 = h + s1 + ch + table[index] + words[index];
			const uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
			const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
			const uint32_t temp2 = s0 + maj;
			h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
		}
		state[0] += a; state[1] += b; state[2] += c; state[3] += d;
		state[4] += e; state[5] += f; state[6] += g; state[7] += h;
	}

	void update(const std::string & data)
	{
		totalLength += data.size();
		pending.insert(pending.end(), data.begin(), data.end());
		while(pending.size() >= 64)
		{
			processBlock(pending.data());
			pending.erase(pending.begin(), pending.begin() + 64);
		}
	}

	std::string digest()
	{
		const uint64_t bitLength = totalLength * 8;
		pending.push_back(0x80);
		while(pending.size() % 64 != 56)
			pending.push_back(0);
		for(int index = 7; index >= 0; --index)
			pending.push_back(static_cast<uint8_t>(bitLength >> (index * 8)));
		for(size_t offset = 0; offset < pending.size(); offset += 64)
			processBlock(pending.data() + offset);
		std::string result;
		char buffer[3];
		for(const uint32_t word : state)
			for(int byte = 3; byte >= 0; --byte)
			{
				std::snprintf(buffer, sizeof(buffer), "%02x", (word >> (byte * 8)) & 0xff);
				result += buffer;
			}
		return result;
	}
};

std::string sha256Hex(const std::string & data)
{
	Sha256 hasher;
	hasher.update(data);
	return hasher.digest();
}

std::string inputModeName(InputMode mode)
{
	switch(mode)
	{
	case InputMode::KEYBOARD_AND_MOUSE: return "keyboard_and_mouse";
	case InputMode::TOUCH: return "touch";
	case InputMode::CONTROLLER: return "controller";
	}
	return "unknown";
}

std::string presentationName(ControllerPresentation presentation)
{
	switch(presentation)
	{
	case ControllerPresentation::PLAYSTATION: return "playstation";
	case ControllerPresentation::UNKNOWN: return "unknown";
	}
	return "unknown";
}

struct AddSpellFixtureState
{
	std::shared_ptr<const std::vector<SpellID>> values;
	std::vector<SpellID> confirmedSpells;
	int confirmCount = 0;
	int cancelCount = 0;
	bool active = false;
};

AddSpellFixtureState addSpellFixture;

}

ControllerE2EExecutor * ControllerE2EExecutor::instance()
{
	return globalExecutor.get();
}

int ControllerE2EExecutor::earlyLoad(const std::string & scenarioPath, const std::string & outputDir)
{
	std::string text;
	try
	{
		std::ifstream stream(scenarioPath, std::ios::binary);
		if(!stream.is_open())
			throw std::runtime_error("cannot open scenario file");
		std::stringstream buffer;
		buffer << stream.rdbuf();
		text = buffer.str();
	}
	catch(const std::exception & e)
	{
		std::cerr << "controller-e2e: failed to read scenario '" << scenarioPath << "': " << e.what() << std::endl;
		return E2E_SCENARIO_ERROR;
	}

	const auto parseResult = parseScenarioText(text);
	if(!parseResult.ok())
	{
		for(const auto & error : parseResult.errors)
			std::cerr << "controller-e2e: " << error << std::endl;
		return E2E_SCENARIO_ERROR;
	}

	globalExecutor = std::make_unique<ControllerE2EExecutor>(
		*parseResult.scenario,
		boost::filesystem::path(outputDir),
		boost::filesystem::path(scenarioPath),
		sha256Hex(text));

	// profile references must resolve before any device or engine state exists
	std::vector<std::string> errors;
	if(!validateScenarioDevices(*parseResult.scenario, globalExecutor->profileIds, errors))
	{
		for(const auto & error : errors)
			std::cerr << "controller-e2e: " << error << std::endl;
		globalExecutor.reset();
		return E2E_SCENARIO_ERROR;
	}

	if(!globalExecutor->artifacts->prepareDirectory())
	{
		for(const auto & error : globalExecutor->artifacts->getWriteErrors())
			std::cerr << "controller-e2e: " << error << std::endl;
		globalExecutor.reset();
		return E2E_ARTIFACT_FAILURE;
	}

	globalExecutor->artifacts->writeJson("scenario.resolved.json", parseResult.scenario->resolved);
	return E2E_PASS;
}

ControllerE2EExecutor::ControllerE2EExecutor(ScenarioSpec scenario, boost::filesystem::path outputDir, boost::filesystem::path scenarioPath, std::string scenarioDigest)
	: scenario(std::move(scenario))
	, artifacts(std::make_unique<ArtifactWriter>(std::move(outputDir)))
	, scenarioPath(std::move(scenarioPath))
	, scenarioDigest(std::move(scenarioDigest))
{
	for(const auto & [id, document] : builtinProfileDocuments())
	{
		JsonNode node(document.c_str(), document.size(), "builtin-profile-" + id);
		std::vector<std::string> errors;
		auto profile = VirtualControllerProfile::parse(node, errors);
		if(!profile)
		{
			resultMessages.push_back("builtin profile '" + id + "' failed to parse");
			for(auto & error : errors)
				resultMessages.push_back("  " + error);
			continue;
		}
		profiles[id] = *profile;
		profileIds.push_back(id);
	}
}

std::string ControllerE2EExecutor::fixtureKind() const
{
	return scenario.fixture["setup"].isString() ? scenario.fixture["setup"].String() : "";
}

void ControllerE2EExecutor::registerProfiles()
{
	// profiles are registered during construction; kept for symmetry
}

void ControllerE2EExecutor::registerBuiltinProbes()
{
	auto & registry = ProbeRegistry::instance();

	registry.registerProbe("runtime", []()
	{
		JsonNode snapshot;
		auto * executor = ControllerE2EExecutor::instance();
		snapshot["frame"].Integer() = executor ? static_cast<si64>(executor->getFrame()) : 0;
		if(ENGINE)
		{
			snapshot["input_mode"].String() = inputModeName(ENGINE->input().getCurrentInputMode());
			snapshot["presentation"].String() = presentationName(ENGINE->input().getControllerPresentation());
			snapshot["window_count"].Integer() = static_cast<si64>(ENGINE->windows().count());
			const auto top = ENGINE->windows().topWindow<CWindowObject>();
			const CWindowObject * topRaw = top.get();
			snapshot["top_window"].String() = topRaw ? typeid(*topRaw).name() : "";
		}
		if(executor)
		{
			int attached = 0;
			for(const auto & [alias, state] : executor->devices)
				if(state.device && state.device->isAttached())
					++attached;
			snapshot["scenario_devices_attached"].Integer() = attached;
			auto & controllers = snapshot["controllers"].Vector();
			for(const auto & [alias, state] : executor->devices)
			{
				JsonNode entry;
				entry["alias"].String() = alias;
				entry["profile"].String() = state.device ? state.device->getProfile().id : "";
				entry["attached"].Bool() = state.device && state.device->isAttached();
				entry["instance_id"].Integer() = state.device ? state.device->getInstanceId() : -1;
				controllers.push_back(std::move(entry));
			}
			snapshot["selected_device"].String() = executor->selectedDeviceAlias;
		}
		return snapshot;
	});

	registry.registerProbe("input", []()
	{
		JsonNode snapshot;
		if(ENGINE)
		{
			snapshot["mode"].String() = inputModeName(ENGINE->input().getCurrentInputMode());
			snapshot["presentation"].String() = presentationName(ENGINE->input().getControllerPresentation());
			snapshot["accept_glyph_bindings"].Vector();
			for(const auto & binding : ENGINE->shortcuts().getJoystickBindings(EShortcut::GLOBAL_ACCEPT))
				snapshot["accept_glyph_bindings"].Vector().push_back(JsonNode(binding));
			const auto token = ENGINE->input().getControllerGlyphToken(ENGINE->shortcuts().getJoystickBindings(EShortcut::GLOBAL_ACCEPT));
			snapshot["accept_glyph_token"].String() = token.value_or("");
		}
		if(auto * executor = ControllerE2EExecutor::instance())
		{
			snapshot["last_event_kind"].String() = executor->lastEventKind;
			snapshot["mode_change_frame"].Integer() = static_cast<si64>(executor->modeChangeFrame);
			snapshot["takeover_reason"].String() = executor->takeoverReason;
		}
		return snapshot;
	});

	registry.registerProbe("navigation", []()
	{
		JsonNode snapshot;
		if(ENGINE)
		{
			snapshot["window_count"].Integer() = static_cast<si64>(ENGINE->windows().count());
			snapshot["top_window_is_popup"].Bool() = ENGINE->windows().isTopWindowPopup();
		}
		return snapshot;
	});

	registry.registerProbe("events", [this]()
	{
		JsonNode snapshot;
		auto & counts = snapshot["counts"].Struct();
		for(const auto & [kind, count] : eventCounts)
			counts[kind].Integer() = count;
		auto & recent = snapshot["recent"].Vector();
		for(const auto & event : recordedEvents)
			recent.push_back(event);
		return snapshot;
	});

	/// Fixture-owned business outcome. It survives the consumer window closing,
	/// which is exactly when confirm/cancel assertions need to read it.
	registry.registerProbe("domain", []()
	{
		JsonNode snapshot;
		snapshot["active"].Bool() = addSpellFixture.active;
		snapshot["confirm_count"].Integer() = static_cast<si64>(addSpellFixture.confirmedSpells.size());
		snapshot["cancel_count"].Integer() = addSpellFixture.cancelCount;
		return snapshot;
	});
}

bool ControllerE2EExecutor::runPrelude(std::string & error)
{
	for(const auto & step : scenario.prelude)
	{
		const auto result = applyPrePollStep(step, error);
		if(result == StepApplyResult::FAILED)
			return false;
	}
	return true;
}

void ControllerE2EExecutor::activate()
{
	if(activated || finished)
		return;
	activated = true;
	startTime = std::chrono::steady_clock::now();
	registerBuiltinProbes();

	std::string error;
	if(!runPrelude(error))
	{
		fail(E2E_SCENARIO_ERROR, "prelude failed: " + error);
		return;
	}
}

ControllerE2EExecutor::StepApplyResult ControllerE2EExecutor::applyPrePollStep(const ScenarioStep & step, std::string & error)
{
	auto deviceFor = [&](const std::string & alias) -> SDLVirtualController *
	{
		const auto found = devices.find(alias);
		if(found == devices.end() || !found->second.device)
		{
			error = "unknown device '" + alias + "'";
			return nullptr;
		}
		return found->second.device.get();
	};

	switch(step.kind)
	{
	case ScenarioStep::Kind::ATTACH:
	{
		const auto profileIterator = profiles.find(step.profileId);
		if(profileIterator == profiles.end())
		{
			error = "unknown profile '" + step.profileId + "'";
			return StepApplyResult::FAILED;
		}
		DeviceState & state = devices[step.device];
		state.device = std::make_unique<SDLVirtualController>(step.device, profileIterator->second);
		if(!state.device->attach(error))
			return StepApplyResult::FAILED;
		if(!state.device->verifyAttachment(error))
			return StepApplyResult::FAILED;
		state.detachedByScenario = false;
		JsonNode record;
		record["device"].String() = step.device;
		record["identity"].String() = state.device->describeIdentity();
		recordSdlEventIdentity(record);
		return StepApplyResult::APPLIED;
	}
	case ScenarioStep::Kind::DETACH:
	{
		auto * device = deviceFor(step.device);
		if(!device)
			return StepApplyResult::FAILED;
		if(!device->detach(error))
			return StepApplyResult::FAILED;
		devices[step.device].detachedByScenario = true;
		return StepApplyResult::APPLIED;
	}
	case ScenarioStep::Kind::RECONNECT:
	{
		auto & state = devices[step.device];
		if(!state.device)
		{
			error = "unknown device '" + step.device + "'";
			return StepApplyResult::FAILED;
		}
		const VirtualControllerProfile profile = state.device->getProfile();
		state.device = std::make_unique<SDLVirtualController>(step.device, profile);
		if(!state.device->attach(error))
			return StepApplyResult::FAILED;
		if(!state.device->verifyAttachment(error))
			return StepApplyResult::FAILED;
		state.detachedByScenario = false;
		return StepApplyResult::APPLIED;
	}
	case ScenarioStep::Kind::SELECT_DEVICE:
	{
		if(!deviceFor(step.device))
			return StepApplyResult::FAILED;
		selectedDeviceAlias = step.device;
		return StepApplyResult::APPLIED;
	}
	case ScenarioStep::Kind::PRESS:
	case ScenarioStep::Kind::RELEASE:
	case ScenarioStep::Kind::TAP:
	{
		auto * device = deviceFor(step.device);
		if(!device)
			return StepApplyResult::FAILED;
		const std::string blockKey = step.device + "/" + step.control;
		const bool pressed = step.kind != ScenarioStep::Kind::RELEASE;
		if(pressed)
		{
			// SDL collapses a release and the following press into zero events
			// when both land between the same two polls; defer the press until
			// the poll after the release has been observed
			const auto blocked = pressBlockedUntilPoll.find(blockKey);
			if(blocked != pressBlockedUntilPoll.end() && frame <= blocked->second)
				return StepApplyResult::PENDING;
		}
		if(!device->setButton(step.control, pressed, error))
			return StepApplyResult::FAILED;
		if(pressed)
			pressBlockedUntilPoll.erase(blockKey);
		else
			pressBlockedUntilPoll[blockKey] = frame;
		if(step.kind == ScenarioStep::Kind::TAP)
		{
			pendingTap.device = step.device;
			pendingTap.control = step.control;
			pendingTap.releaseAtFrame = static_cast<int>(frame) + step.holdFrames;
		}
		return StepApplyResult::APPLIED;
	}
	case ScenarioStep::Kind::SET_AXIS:
	{
		auto * device = deviceFor(step.device);
		if(!device)
			return StepApplyResult::FAILED;
		if(!device->setAxis(step.control, step.axisValue, error))
			return StepApplyResult::FAILED;
		return StepApplyResult::APPLIED;
	}
	case ScenarioStep::Kind::RAMP_AXIS:
	{
		auto * device = deviceFor(step.device);
		if(!device)
			return StepApplyResult::FAILED;
		if(!device->setAxis(step.control, step.axisFrom, error))
			return StepApplyResult::FAILED;
		PendingRamp ramp;
		ramp.device = step.device;
		ramp.control = step.control;
		ramp.from = step.axisFrom;
		ramp.to = step.axisTo;
		ramp.startFrame = static_cast<int>(frame);
		ramp.frames = step.rampFrames;
		pendingRamps.push_back(ramp);
		return StepApplyResult::APPLIED;
	}
	case ScenarioStep::Kind::NEUTRALIZE:
	{
		auto * device = deviceFor(step.device);
		if(!device)
			return StepApplyResult::FAILED;
		if(!device->neutralize(error))
			return StepApplyResult::FAILED;
		for(const auto & [control, index] : device->getProfile().buttons)
			pressBlockedUntilPoll[step.device + "/" + control] = frame;
		return StepApplyResult::APPLIED;
	}
	case ScenarioStep::Kind::PRESS_KEY:
	case ScenarioStep::Kind::RELEASE_KEY:
	{
		const SDL_Keycode keycode = SDL_GetKeyFromName(step.keyName.c_str());
		if(keycode == SDLK_UNKNOWN)
		{
			error = "unknown key name '" + step.keyName + "'";
			return StepApplyResult::FAILED;
		}
		SDL_Event event{};
		event.type = step.kind == ScenarioStep::Kind::PRESS_KEY ? SDL_KEYDOWN : SDL_KEYUP;
		event.key.type = event.type;
		event.key.state = step.kind == ScenarioStep::Kind::PRESS_KEY ? SDL_PRESSED : SDL_RELEASED;
		event.key.keysym.sym = keycode;
		event.key.keysym.scancode = SDL_GetScancodeFromKey(keycode);
		if(SDL_PushEvent(&event) != 1)
		{
			error = "SDL_PushEvent failed: " + std::string(SDL_GetError());
			return StepApplyResult::FAILED;
		}
		return StepApplyResult::APPLIED;
	}
	case ScenarioStep::Kind::MOVE_MOUSE:
	case ScenarioStep::Kind::CLICK_MOUSE:
	{
		const int scaling = ENGINE ? ENGINE->screenHandler().getScalingFactor() : 1;
		SDL_Event motion{};
		motion.type = SDL_MOUSEMOTION;
		motion.motion.type = SDL_MOUSEMOTION;
		motion.motion.x = step.mouseX * scaling;
		motion.motion.y = step.mouseY * scaling;
		if(SDL_PushEvent(&motion) != 1)
		{
			error = "SDL_PushEvent failed: " + std::string(SDL_GetError());
			return StepApplyResult::FAILED;
		}
		if(step.kind == ScenarioStep::Kind::MOVE_MOUSE)
			return StepApplyResult::APPLIED;

		Uint8 button = SDL_BUTTON_LEFT;
		if(step.mouseButton == "right")
			button = SDL_BUTTON_RIGHT;
		else if(step.mouseButton == "middle")
			button = SDL_BUTTON_MIDDLE;
		else if(step.mouseButton != "left")
		{
			error = "unknown mouse button '" + step.mouseButton + "'";
			return StepApplyResult::FAILED;
		}

		SDL_Event down{};
		down.type = SDL_MOUSEBUTTONDOWN;
		down.button.type = SDL_MOUSEBUTTONDOWN;
		down.button.button = button;
		down.button.state = SDL_PRESSED;
		down.button.x = step.mouseX * scaling;
		down.button.y = step.mouseY * scaling;
		if(SDL_PushEvent(&down) != 1)
		{
			error = "SDL_PushEvent failed: " + std::string(SDL_GetError());
			return StepApplyResult::FAILED;
		}
		SDL_Event up = down;
		up.type = SDL_MOUSEBUTTONUP;
		up.button.type = SDL_MOUSEBUTTONUP;
		up.button.state = SDL_RELEASED;
		if(SDL_PushEvent(&up) != 1)
		{
			error = "SDL_PushEvent failed: " + std::string(SDL_GetError());
			return StepApplyResult::FAILED;
		}
		return StepApplyResult::APPLIED;
	}
	default:
		error = "operation '" + step.kindName() + "' cannot run before poll";
		return StepApplyResult::FAILED;
	}
}

void ControllerE2EExecutor::applyScheduledState()
{
	if(!pendingTap.device.empty() && static_cast<int>(frame) >= pendingTap.releaseAtFrame)
	{
		const auto found = devices.find(pendingTap.device);
		if(found != devices.end() && found->second.device)
		{
			std::string error;
			found->second.device->setButton(pendingTap.control, false, error);
			if(!error.empty())
				fail(E2E_DRIVER_ERROR, "scheduled tap release failed: " + error);
			else
				pressBlockedUntilPoll[pendingTap.device + "/" + pendingTap.control] = frame;
		}
		pendingTap = {};
	}

	for(auto ramp = pendingRamps.begin(); ramp != pendingRamps.end();)
	{
		const int elapsed = static_cast<int>(frame) - ramp->startFrame;
		const double progress = std::clamp(static_cast<double>(elapsed) / ramp->frames, 0.0, 1.0);
		const double value = ramp->from + (ramp->to - ramp->from) * progress;
		const auto found = devices.find(ramp->device);
		if(found != devices.end() && found->second.device)
		{
			std::string error;
			found->second.device->setAxis(ramp->control, value, error);
			if(!error.empty())
			{
				fail(E2E_DRIVER_ERROR, "ramp update failed: " + error);
				return;
			}
		}
		if(progress >= 1.0)
			ramp = pendingRamps.erase(ramp);
		else
			++ramp;
	}
}

void ControllerE2EExecutor::advanceStepPointer()
{
	++stepCursor;
	waitState = {};
	unchangedState = {};
}

const JsonNode ControllerE2EExecutor::readProbeField(const std::string & probe, const std::string & field, std::string & error)
{
	const JsonNode snapshot = ProbeRegistry::instance().read(probe);
	const auto * failure = findProbeField(snapshot, "error");
	if(failure)
	{
		error = failure->String();
		return JsonNode();
	}
	const JsonNode * value = findProbeField(snapshot, field);
	if(!value)
	{
		error = "probe '" + probe + "' has no field '" + field + "'";
		return JsonNode();
	}
	return *value;
}

bool ControllerE2EExecutor::evaluateCondition(bool & satisfied, std::string & error)
{
	const ScenarioStep & step = scenario.steps[stepCursor];
	std::string readError;
	const JsonNode current = readProbeField(step.probe, step.field, readError);
	if(!readError.empty())
	{
		error = readError;
		return false;
	}

	if(step.expectChange)
	{
		if(!waitState.sawInitialValue)
		{
			waitState.baseline = current;
			waitState.sawInitialValue = true;
			satisfied = false;
			return true;
		}
		satisfied = !(current == waitState.baseline);
		return true;
	}

	satisfied = current == step.expected;
	return true;
}

void ControllerE2EExecutor::onBeforePoll()
{
	if(!activated)
		return;
	allowShutdownThrow = true;
	if(finished)
		throwShutdownIfAllowed();

	applyScheduledState();
	if(finished)
		return;

	while(stepCursor < scenario.steps.size())
	{
		const ScenarioStep & step = scenario.steps[stepCursor];
		switch(step.kind)
		{
		case ScenarioStep::Kind::ATTACH:
		case ScenarioStep::Kind::DETACH:
		case ScenarioStep::Kind::RECONNECT:
		case ScenarioStep::Kind::SELECT_DEVICE:
		case ScenarioStep::Kind::PRESS:
		case ScenarioStep::Kind::RELEASE:
		case ScenarioStep::Kind::TAP:
		case ScenarioStep::Kind::SET_AXIS:
		case ScenarioStep::Kind::RAMP_AXIS:
		case ScenarioStep::Kind::NEUTRALIZE:
		case ScenarioStep::Kind::PRESS_KEY:
		case ScenarioStep::Kind::RELEASE_KEY:
		case ScenarioStep::Kind::MOVE_MOUSE:
		case ScenarioStep::Kind::CLICK_MOUSE:
		{
			std::string error;
			const auto result = applyPrePollStep(step, error);
			if(result == StepApplyResult::PENDING)
				return; // deferred until the previous release has been polled
			JsonNode record;
			record["index"].Integer() = step.index;
			record["op"].String() = step.kindName();
			record["frame"].Integer() = static_cast<si64>(frame);
			record["result"].String() = result == StepApplyResult::APPLIED ? "applied" : "error";
			if(result != StepApplyResult::APPLIED)
				record["message"].String() = error;
			writeStepRecord(record);
			if(result != StepApplyResult::APPLIED)
			{
				fail(E2E_SCENARIO_ERROR, "step " + std::to_string(step.index) + " (" + step.kindName() + ") failed: " + error);
				return;
			}
			advanceStepPointer();
			continue;
		}
		default:
			return; // post-present step: handled in onAfterPresent
		}
	}
}

void ControllerE2EExecutor::onAfterPresent()
{
	if(!activated)
		return;
	allowShutdownThrow = true;
	if(finished)
		throwShutdownIfAllowed();

	++frame;

	if(ENGINE)
	{
		const InputMode current = ENGINE->input().getCurrentInputMode();
		if(current != lastInputMode)
		{
			modeChangeFrame = frame;
			takeoverReason = inputModeName(current) + "_after_" + (lastEventKind.empty() ? std::string("unknown") : lastEventKind);
			lastInputMode = current;
		}
	}

	const auto now = std::chrono::steady_clock::now();
	if(std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() > scenario.timeoutMs)
	{
		fail(E2E_TIMEOUT, "scenario timeout of " + std::to_string(scenario.timeoutMs) + " ms exceeded at frame " + std::to_string(frame));
		return;
	}

	if(stepCursor >= scenario.steps.size())
	{
		finish(E2E_PASS, "scenario completed");
		return;
	}

	const ScenarioStep & step = scenario.steps[stepCursor];
	switch(step.kind)
	{
	// Pre-poll step still pending (e.g. a press deferred until the poll after
	// the previous release); it is retried in the next onBeforePoll
	case ScenarioStep::Kind::ATTACH:
	case ScenarioStep::Kind::DETACH:
	case ScenarioStep::Kind::RECONNECT:
	case ScenarioStep::Kind::SELECT_DEVICE:
	case ScenarioStep::Kind::PRESS:
	case ScenarioStep::Kind::RELEASE:
	case ScenarioStep::Kind::TAP:
	case ScenarioStep::Kind::SET_AXIS:
	case ScenarioStep::Kind::RAMP_AXIS:
	case ScenarioStep::Kind::NEUTRALIZE:
	case ScenarioStep::Kind::PRESS_KEY:
	case ScenarioStep::Kind::RELEASE_KEY:
	case ScenarioStep::Kind::MOVE_MOUSE:
	case ScenarioStep::Kind::CLICK_MOUSE:
		return;
	case ScenarioStep::Kind::WAIT_FRAMES:
	{
		if(!waitState.active)
		{
			waitState.active = true;
			waitState.stableFrames = step.frames;
		}
		if(--waitState.stableFrames <= 0)
		{
			JsonNode record;
			record["index"].Integer() = step.index;
			record["op"].String() = step.kindName();
			record["frame"].Integer() = static_cast<si64>(frame);
			record["result"].String() = "ok";
			writeStepRecord(record);
			advanceStepPointer();
		}
		return;
	}
	case ScenarioStep::Kind::WAIT_UNTIL:
	{
		if(!waitState.active)
		{
			waitState.active = true;
			waitState.deadline = now + std::chrono::milliseconds(step.timeoutMs);
		}
		std::string error;
		bool satisfied = false;
		if(!evaluateCondition(satisfied, error))
		{
			fail(E2E_SCENARIO_ERROR, "step " + std::to_string(step.index) + " wait_until failed: " + error);
			return;
		}
		if(satisfied)
		{
			if(++waitState.stableFrames >= step.stableForFrames)
			{
				JsonNode record;
				record["index"].Integer() = step.index;
				record["op"].String() = step.kindName();
				record["frame"].Integer() = static_cast<si64>(frame);
				record["result"].String() = "satisfied";
				writeStepRecord(record);
				advanceStepPointer();
			}
			return;
		}
		waitState.stableFrames = 0;
		if(now >= waitState.deadline)
		{
			std::string readError;
			const JsonNode observed = readProbeField(step.probe, step.field, readError);
			fail(E2E_TIMEOUT, "step " + std::to_string(step.index) + " wait_until timed out after " + std::to_string(step.timeoutMs)
				+ " ms; last observed value: " + (readError.empty() ? observed.toString() : readError));
		}
		return;
	}
	case ScenarioStep::Kind::ASSERT:
	{
		std::string error;
		const JsonNode current = readProbeField(step.probe, step.field, error);
		JsonNode record;
		record["index"].Integer() = step.index;
		record["op"].String() = step.kindName();
		record["frame"].Integer() = static_cast<si64>(frame);
		if(!error.empty())
		{
			record["result"].String() = "error";
			record["message"].String() = error;
			writeStepRecord(record);
			fail(E2E_ASSERTION_FAILURE, "step " + std::to_string(step.index) + " assert failed: " + error);
			return;
		}
		if(!(current == step.expected))
		{
			record["result"].String() = "failed";
			record["observed"].String() = current.toString();
			record["expected"].String() = step.expected.toString();
			writeStepRecord(record);
			fail(E2E_ASSERTION_FAILURE, "step " + std::to_string(step.index) + " assert failed: expected "
				+ step.expected.toString() + " but observed " + current.toString());
			return;
		}
		record["result"].String() = "ok";
		writeStepRecord(record);
		advanceStepPointer();
		return;
	}
	case ScenarioStep::Kind::ASSERT_UNCHANGED:
	{
		if(!unchangedState.active)
		{
			std::string error;
			unchangedState.baseline = readProbeField(step.probe, step.field, error);
			if(!error.empty())
			{
				fail(E2E_SCENARIO_ERROR, "step " + std::to_string(step.index) + " assert_unchanged failed: " + error);
				return;
			}
			unchangedState.active = true;
			unchangedState.framesLeft = step.frames;
		}
		std::string error;
		const JsonNode current = readProbeField(step.probe, step.field, error);
		if(!error.empty())
		{
			fail(E2E_SCENARIO_ERROR, "step " + std::to_string(step.index) + " assert_unchanged failed: " + error);
			return;
		}
		if(!(current == unchangedState.baseline))
		{
			fail(E2E_ASSERTION_FAILURE, "step " + std::to_string(step.index) + " assert_unchanged failed: value changed to "
				+ current.toString() + " after " + std::to_string(step.frames - unchangedState.framesLeft) + " frames");
			return;
		}
		if(--unchangedState.framesLeft <= 0)
		{
			JsonNode record;
			record["index"].Integer() = step.index;
			record["op"].String() = step.kindName();
			record["frame"].Integer() = static_cast<si64>(frame);
			record["result"].String() = "ok";
			writeStepRecord(record);
			advanceStepPointer();
		}
		return;
	}
	case ScenarioStep::Kind::ASSERT_EVENT_COUNT:
	{
		const auto found = eventCounts.find(step.eventKind);
		const int actual = found == eventCounts.end() ? 0 : found->second;
		JsonNode record;
		record["index"].Integer() = step.index;
		record["op"].String() = step.kindName();
		record["frame"].Integer() = static_cast<si64>(frame);
		if(actual != step.eventCount)
		{
			record["result"].String() = "failed";
			record["observed"].Integer() = actual;
			record["expected"].Integer() = step.eventCount;
			writeStepRecord(record);
			fail(E2E_ASSERTION_FAILURE, "step " + std::to_string(step.index) + " assert_event_count failed: expected "
				+ std::to_string(step.eventCount) + " '" + step.eventKind + "' events but observed " + std::to_string(actual));
			return;
		}
		record["result"].String() = "ok";
		writeStepRecord(record);
		advanceStepPointer();
		return;
	}
	case ScenarioStep::Kind::CHECKPOINT:
	{
		JsonNode record;
		record["index"].Integer() = step.index;
		record["op"].String() = step.kindName();
		record["frame"].Integer() = static_cast<si64>(frame);
		record["name"].String() = step.name;
		record["result"].String() = "ok";
		writeStepRecord(record);
		advanceStepPointer();
		return;
	}
	case ScenarioStep::Kind::CAPTURE_FRAME:
	case ScenarioStep::Kind::CAPTURE_REGION:
	{
		std::string savedName;
		SDL_Surface * surface = nullptr;
		if(ENGINE)
			surface = static_cast<ScreenHandler &>(ENGINE->screenHandler()).getE2ECaptureSurface();
		const bool ok = artifacts->savePng(surface, step.name, step.hasRegion, step.regionX, step.regionY, step.regionW, step.regionH, savedName);
		JsonNode record;
		record["index"].Integer() = step.index;
		record["op"].String() = step.kindName();
		record["frame"].Integer() = static_cast<si64>(frame);
		record["name"].String() = step.name;
		record["result"].String() = ok ? "ok" : "error";
		if(ok)
			record["screenshot"].String() = "screenshots/" + savedName;
		writeStepRecord(record);
		if(!ok)
		{
			fail(E2E_ARTIFACT_FAILURE, "step " + std::to_string(step.index) + " capture failed: " + step.name);
			return;
		}
		advanceStepPointer();
		return;
	}
	case ScenarioStep::Kind::REQUEST_SHUTDOWN:
	{
		finish(E2E_PASS, "scenario requested shutdown");
		return;
	}
	default:
		fail(E2E_DRIVER_ERROR, "step " + std::to_string(step.index) + " cannot run after present");
		return;
	}
}

void ControllerE2EExecutor::recordSdlEvent(const SDL_Event & event)
{
	if(finished)
		return;

	JsonNode entry;
	entry["frame"].Integer() = static_cast<si64>(frame);
	std::string kind;

	switch(event.type)
	{
	case SDL_CONTROLLERBUTTONDOWN:
		kind = "controller_button_down";
		entry["instance_id"].Integer() = event.cbutton.which;
		entry["button"].Integer() = event.cbutton.button;
		break;
	case SDL_CONTROLLERBUTTONUP:
		kind = "controller_button_up";
		entry["instance_id"].Integer() = event.cbutton.which;
		entry["button"].Integer() = event.cbutton.button;
		break;
	case SDL_CONTROLLERAXISMOTION:
		kind = "controller_axis";
		entry["instance_id"].Integer() = event.caxis.which;
		entry["axis"].Integer() = event.caxis.axis;
		entry["value"].Integer() = event.caxis.value;
		break;
	case SDL_CONTROLLERDEVICEADDED:
		kind = "device_added";
		entry["which"].Integer() = event.cdevice.which;
		break;
	case SDL_CONTROLLERDEVICEREMOVED:
		kind = "device_removed";
		entry["which"].Integer() = event.cdevice.which;
		break;
	case SDL_CONTROLLERDEVICEREMAPPED:
		kind = "device_remapped";
		entry["which"].Integer() = event.cdevice.which;
		break;
	case SDL_KEYDOWN:
		kind = "key_down";
		entry["key"].String() = SDL_GetKeyName(event.key.keysym.sym);
		break;
	case SDL_KEYUP:
		kind = "key_up";
		entry["key"].String() = SDL_GetKeyName(event.key.keysym.sym);
		break;
	case SDL_MOUSEBUTTONDOWN:
		kind = "mouse_button_down";
		entry["button"].Integer() = event.button.button;
		break;
	case SDL_MOUSEBUTTONUP:
		kind = "mouse_button_up";
		entry["button"].Integer() = event.button.button;
		break;
	case SDL_MOUSEMOTION:
		kind = "mouse_motion";
		entry["x"].Integer() = event.motion.x;
		entry["y"].Integer() = event.motion.y;
		break;
	default:
		return;
	}

	entry["kind"].String() = kind;
	lastEventKind = kind;
	eventCounts[kind]++;
	recordedEvents.push_back(std::move(entry));
	while(recordedEvents.size() > 512)
		recordedEvents.pop_front();

	if(recordedEvents.size() % 64 == 0)
		flushEvents();
}

void ControllerE2EExecutor::recordSdlEventIdentity(const JsonNode & record)
{
	if(artifacts && artifacts->isReady())
		artifacts->appendJsonLine("steps.jsonl", record);
}

void ControllerE2EExecutor::writeStepRecord(const JsonNode & record)
{
	if(artifacts && artifacts->isReady())
		artifacts->appendJsonLine("steps.jsonl", record);
}

void ControllerE2EExecutor::flushEvents()
{
	if(!artifacts || !artifacts->isReady())
		return;
	for(const auto & event : recordedEvents)
		artifacts->appendJsonLine("events.jsonl", event);
	recordedEvents.clear();
}

void ControllerE2EExecutor::cleanupDevices()
{
	for(auto & [alias, state] : devices)
	{
		if(!state.device || !state.device->isAttached())
			continue;
		std::string error;
		if(!state.device->detach(error))
			resultMessages.push_back("cleanup of device '" + alias + "' failed: " + error);
	}
}

void ControllerE2EExecutor::fail(int code, const std::string & message)
{
	if(finished)
		return;
	resultMessages.push_back(message);
	finish(code, message);
}

void ControllerE2EExecutor::finish(int code, const std::string & message)
{
	if(finished)
		return;
	finished = true;
	exitCode = code;
	resultMessages.push_back("result: " + message);
	cleanupDevices();
	flushEvents();
	throwShutdownIfAllowed();
}

void ControllerE2EExecutor::throwShutdownIfAllowed()
{
	// Only the main loop may receive the shutdown exception; failures during
	// engine construction are deferred until the first loop hook runs
	if(allowShutdownThrow)
		throw GameShutdownException();
}

int ControllerE2EExecutor::finalize()
{
	if(!finished)
	{
		// reached when the main loop ended without a scenario result, e.g. a crash
		exitCode = E2E_ABNORMAL;
		resultMessages.push_back("process exited without scenario completion");
		cleanupDevices();
		flushEvents();
		finished = true;
	}

	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();

	JsonNode run;
	run["schema"].String() = "vcmi.controller-e2e/run/v1";
	run["scenario_id"].String() = scenario.id;
	run["scenario_path"].String() = scenarioPath.string();
	run["scenario_digest_sha256"].String() = scenarioDigest;
	run["result"].String() = exitCode == E2E_PASS && !artifacts->hasWriteErrors() ? "PASS" : "FAIL";
	run["exit_code"].Integer() = exitCode;
	run["frames"].Integer() = static_cast<si64>(frame);
	run["duration_ms"].Integer() = elapsed;
	run["pid"].Integer() = static_cast<si64>(::getpid());
	run["source_sha"].String() = GameConstants::GIT_SHA1;
	run["version"].String() = GameConstants::VCMI_VERSION;

	SDL_version linked{};
	SDL_GetVersion(&linked);
	run["sdl_compile_version"].String() = std::to_string(SDL_MAJOR_VERSION) + "." + std::to_string(SDL_MINOR_VERSION) + "." + std::to_string(SDL_PATCHLEVEL);
	run["sdl_linked_version"].String() = std::to_string(linked.major) + "." + std::to_string(linked.minor) + "." + std::to_string(linked.patch);
	run["sdl_revision"].String() = SDL_GetRevision();
	run["platform"].String() = SDL_GetPlatform();
	run["locale"].String() = settings["general"]["language"].String();

	if(ENGINE)
	{
		const Point dimensions = ENGINE->screenDimensions();
		run["resolution"]["x"].Integer() = dimensions.x;
		run["resolution"]["y"].Integer() = dimensions.y;
		run["scaling_factor"].Integer() = ENGINE->screenHandler().getScalingFactor();
	}

	auto & profileDigests = run["profiles"].Struct();
	for(const auto & [id, document] : builtinProfileDocuments())
		profileDigests[id].String() = sha256Hex(document);

	auto & deviceList = run["devices"].Vector();
	for(const auto & [alias, state] : devices)
	{
		JsonNode entry;
		entry["alias"].String() = alias;
		entry["profile"].String() = state.device ? state.device->getProfile().id : "";
		entry["identity"].String() = state.device ? state.device->describeIdentity() : "missing";
		deviceList.push_back(std::move(entry));
	}

	auto & messages = run["messages"].Vector();
	for(const auto & message : resultMessages)
		messages.push_back(JsonNode(message));

	auto & artifactErrors = run["artifact_errors"].Vector();
	for(const auto & error : artifacts->getWriteErrors())
		artifactErrors.push_back(JsonNode(error));

	artifacts->writeJson("run.json", run);

	if(exitCode == E2E_PASS && artifacts->hasWriteErrors())
		return E2E_ARTIFACT_FAILURE;
	return exitCode;
}

void ControllerE2EExecutor::pushAddSpellFixture()
{
	if(addSpellFixture.active || !ENGINE || !LIBRARY || !LIBRARY->spellh)
		return;

	auto allowedSet = LIBRARY->spellh->getDefaultAllowed();
	std::vector<SpellID> allSpells(allowedSet.begin(), allowedSet.end());
	allSpells.erase(std::remove_if(allSpells.begin(), allSpells.end(), [](const SpellID & spell)
	{
		return !spell.toSpell()->isCombat();
	}), allSpells.end());
	std::sort(allSpells.begin(), allSpells.end(), [](const SpellID & left, const SpellID & right)
	{
		const auto leftSpell = left.toSpell();
		const auto rightSpell = right.toSpell();
		if(leftSpell->getLevel() != rightSpell->getLevel())
			return leftSpell->getLevel() < rightSpell->getLevel();
		for(const auto schoolId : LIBRARY->spellSchoolHandler->getAllObjects())
		{
			if(leftSpell->schools.count(schoolId) && !rightSpell->schools.count(schoolId))
				return true;
			if(!leftSpell->schools.count(schoolId) && rightSpell->schools.count(schoolId))
				return false;
		}
		return TextOperations::compareLocalizedStrings(leftSpell->getNameTranslated(), rightSpell->getNameTranslated());
	});

	auto values = std::make_shared<const std::vector<SpellID>>(allSpells);
	addSpellFixture.values = values;

	std::vector<size_t> preselected;
	const auto & preselectedNode = scenario.fixture["preselected"];
	if(preselectedNode.isVector())
	{
		for(const auto & entry : preselectedNode.Vector())
		{
			if(entry.getType() != JsonNode::JsonType::DATA_INTEGER || entry.Integer() < 0 || entry.Integer() >= static_cast<si64>(values->size()))
			{
				fail(E2E_SCENARIO_ERROR, "fixture preselected index out of range");
				return;
			}
			preselected.push_back(static_cast<size_t>(entry.Integer()));
		}
	}

	const std::string disabledReason = LIBRARY->generaltexth->translate("vcmi.lobby.battleOnlySpellAlreadySelected");
	std::vector<CObjectListWindow::ListItem> items;
	std::vector<std::shared_ptr<IImage>> images;
	items.reserve(values->size());
	images.reserve(values->size());
	for(size_t index = 0; index < values->size(); ++index)
	{
		const auto & spell = (*values)[index];
		const bool alreadySelected = std::find(preselected.begin(), preselected.end(), index) != preselected.end();
		items.push_back({spell.toSpell()->getNameTranslated(), !alreadySelected, alreadySelected ? disabledReason : ""});
		auto image = ENGINE->renderHandler().loadImage(
			AnimationPath::builtin("SpellInt"), spell.toSpell()->getIconIndex() + 1, 0, EImageBlitMode::OPAQUE);
		image->scaleTo(Point(35, 23), EScalingAlgorithm::NEAREST);
		images.push_back(image);
	}

	const std::string title = LIBRARY->generaltexth->translate("vcmi.lobby.battleOnlySpellAdd");
	auto window = std::make_shared<CObjectListWindow>(items, nullptr, title, title, [values](int index)
	{
		if(index < 0 || index >= static_cast<int>(values->size()))
			return;
		addSpellFixture.confirmedSpells.push_back(values->at(index));
	}, 0, images, true, true);
	window->onExit = []()
	{
		++addSpellFixture.cancelCount;
	};
	window->setBattleOnlySpellActionPrompts();
	addSpellFixture.active = true;
	ENGINE->windows().pushWindow(window);
}

namespace Hooks
{

void onBeforeInputHandler()
{
	if(auto * executor = ControllerE2EExecutor::instance())
		executor->activate();
}

void onBeforePoll()
{
	if(auto * executor = ControllerE2EExecutor::instance())
		executor->onBeforePoll();
}

void onAfterPresent()
{
	if(auto * executor = ControllerE2EExecutor::instance())
		executor->onAfterPresent();
}

void recordSdlEvent(const SDL_Event & event)
{
	if(auto * executor = ControllerE2EExecutor::instance())
		executor->recordSdlEvent(event);
}

}

}
