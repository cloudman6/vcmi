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
#include "../CServerHandler.h"
#include "../eventsSDL/InputHandler.h"
#include "../eventsSDL/InputSourceGameController.h"
#include "../gui/CursorHandler.h"
#include "../gui/ShortcutHandler.h"
#include "../gui/WindowHandler.h"
#include "../lobby/BattleOnlyModeTab.h"
#include "../lobby/CLobbyScreen.h"
#include "../mainmenu/CMainMenu.h"
#include "../render/Canvas.h"
#include "../render/IScreenHandler.h"
#include "../render/IRenderHandler.h"
#include "../render/IImage.h"
#include "../renderSDL/ScreenHandler.h"
#include "../widgets/Buttons.h"
#include "../windows/GUIClasses.h"
#include "../windows/CWindowObject.h"
#include "../windows/InfoWindows.h"

#include "../../lib/GameLibrary.h"
#include "../../lib/CCreatureHandler.h"
#include "../../lib/CConfigHandler.h"
#include "../../lib/StartInfo.h"
#include "../../lib/VCMIDirs.h"
#include "../../lib/constants/EntityIdentifiers.h"
#include "../../lib/constants/StringConstants.h"
#include "../../lib/json/JsonNode.h"
#include "../../lib/mapping/CMapInfo.h"
#include "../../lib/networkPacks/PacksForLobby.h"
#include "../../lib/spells/CSpellHandler.h"
#include "../../lib/spells/SpellSchoolHandler.h"
#include "../../lib/texts/CGeneralTextHandler.h"
#include "../../lib/texts/TextOperations.h"
#include "../../lib/filesystem/ResourcePath.h"

#include <SDL.h>

#include <boost/filesystem.hpp>
#include <fstream>

#ifdef VCMI_WINDOWS
#include <process.h>
#else
#include <unistd.h>
#endif

#include <array>
#include <atomic>
#include <cstring>
#include <thread>

namespace ControllerE2E
{

namespace
{

std::unique_ptr<ControllerE2EExecutor> globalExecutor;

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

std::string presentationName(ControllerPrompt::Family presentation)
{
	switch(presentation)
	{
	case ControllerPrompt::Family::GENERIC: return "generic";
	case ControllerPrompt::Family::NINTENDO: return "nintendo";
	case ControllerPrompt::Family::PLAYSTATION: return "playstation";
	case ControllerPrompt::Family::XBOX: return "xbox";
	case ControllerPrompt::Family::UNKNOWN: return "unknown";
	}
	return "unknown";
}

std::vector<SpellID> sortedCombatSpellsForFixture()
{
	const auto allowedSet = LIBRARY->spellh->getDefaultAllowed();
	std::vector<SpellID> result(allowedSet.begin(), allowedSet.end());
	result.erase(std::remove_if(result.begin(), result.end(), [](const SpellID & spell)
	{
		return !spell.toSpell()->isCombat();
	}), result.end());
	std::sort(result.begin(), result.end(), [](const SpellID & left, const SpellID & right)
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
		return TextOperations::compareLocalizedStrings(
			leftSpell->getNameTranslated(), rightSpell->getNameTranslated());
	});
	return result;
}

struct SpellFixtureState
{
	std::vector<SpellID> selectedSpells;
	std::optional<SpellID> targetSpell;
	std::optional<size_t> pendingChoice;
	std::atomic_bool startInfoApplied = false;
	bool add = false;
	bool startInfoRequested = false;
	bool pendingSetup = false;
	bool active = false;
};

SpellFixtureState spellFixture;

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
	ScenarioProfileButtons profileButtons;
	for(const auto & [id, profile] : globalExecutor->profiles)
	{
		auto & buttons = profileButtons[id];
		for(const auto & button : profile.buttons)
			buttons.insert(button.first);
	}
	if(!validateScenarioDevices(*parseResult.scenario, profileButtons, errors))
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

ControllerE2EExecutor::ControllerE2EExecutor(
	ScenarioSpec scenario,
	boost::filesystem::path outputDir,
	boost::filesystem::path scenarioPath,
	std::string scenarioDigest
)
	: scenario(std::move(scenario))
	, artifacts(std::make_unique<ArtifactWriter>(std::move(outputDir)))
	, scenarioPath(std::move(scenarioPath))
	, scenarioDigest(std::move(scenarioDigest))
{
	for(const auto * eventKind : {
		"controller_button_down",
		"controller_button_up",
		"controller_axis",
		"device_added",
		"device_removed",
		"device_remapped",
		"key_down",
		"key_up",
		"mouse_button_down",
		"mouse_button_up",
		"mouse_motion",
	})
		eventCounts[eventKind] = 0;

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

bool ControllerE2EExecutor::applyJoystickAxisBindingFixture(std::string & error)
{
	const JsonNode & overrides = scenario.fixture["joystickAxisBindings"];
	if(overrides.getType() == JsonNode::JsonType::DATA_NULL)
		return true;
	if(!overrides.isStruct())
	{
		error = "fixture.joystickAxisBindings must be an object";
		return false;
	}

	for(const auto & [action, bindings] : overrides.Struct())
	{
		if(keyBindingsConfig["joystickAxes"].Struct().count(action) == 0)
		{
			error = "fixture.joystickAxisBindings references unknown action '" + action + "'";
			return false;
		}
		if(!bindings.isString() && !bindings.isVector())
		{
			error = "fixture.joystickAxisBindings entry '" + action + "' must be a string or array";
			return false;
		}
		if(bindings.isVector()
			&& std::any_of(bindings.Vector().begin(), bindings.Vector().end(), [](const JsonNode & binding)
			{
				return !binding.isString();
			}))
		{
			error = "fixture.joystickAxisBindings entry '" + action + "' contains a non-string binding";
			return false;
		}
	}

	{
		Settings axisBindings = keyBindingsConfig.write["joystickAxes"];
		for(const auto & [action, bindings] : overrides.Struct())
			axisBindings[action] = bindings;
	}
	return true;
}

void ControllerE2EExecutor::registerBuiltinProbes()
{
	auto & registry = ProbeRegistry::instance();

	registry.registerProbe("runtime", []()
	{
		JsonNode snapshot;
		auto * executor = ControllerE2EExecutor::instance();
		snapshot["frame"].Integer() = executor ? static_cast<si64>(executor->getFrame()) : 0;
		auto & probes = ProbeRegistry::instance();
		const JsonNode battle = probes.has("battle") ? probes.read("battle") : JsonNode();
		const JsonNode * battleOpen = findProbeField(battle, "open");
		snapshot["battle_open"].Bool() = battleOpen && battleOpen->isBool() && battleOpen->Bool();
		if(ENGINE)
		{
			snapshot["input_mode"].String() = inputModeName(ENGINE->input().getCurrentInputMode());
			snapshot["presentation"].String() = presentationName(ENGINE->input().getActiveControllerPromptFamily());
			snapshot["window_count"].Integer() = static_cast<si64>(ENGINE->windows().count());
			const auto top = ENGINE->windows().topWindow<CWindowObject>();
			const CWindowObject * topRaw = top.get();
			snapshot["top_window"].String() = topRaw ? typeid(*topRaw).name() : "";
			snapshot["cursor_position_x"].Integer() = ENGINE->getCursorPosition().x;
			snapshot["cursor_position_y"].Integer() = ENGINE->getCursorPosition().y;
			snapshot["cursor_visible"].Bool() = ENGINE->cursor().controllerE2EVisible();
			snapshot["cursor_backend_visible"].Bool() = SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE;
			snapshot["cursor_id"].String() = ENGINE->cursor().getCursorIDForE2E();
			snapshot["cursor_image_pending"].Bool() = ENGINE->cursor().hasPendingImageForE2E();
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
			snapshot["presentation"].String() = presentationName(ENGINE->input().getActiveControllerPromptFamily());
			snapshot["accept_glyph_bindings"].Vector();
			for(const auto & binding : ENGINE->shortcuts().getJoystickButtonBindings(EShortcut::GLOBAL_ACCEPT))
				snapshot["accept_glyph_bindings"].Vector().push_back(JsonNode(binding));
			const auto bindings = ENGINE->shortcuts().getJoystickButtonBindings(EShortcut::GLOBAL_ACCEPT);
			const bool supported = ENGINE->input().getActiveControllerPromptFamily() != ControllerPrompt::Family::UNKNOWN
				&& bindings.size() == 1 && (bindings.front() == "a" || bindings.front() == "b");
			snapshot["accept_glyph_token"].String() = supported ? bindings.front() : "";
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

	/// The production lobby writes server-round-tripped Battle Only state here,
	/// allowing scenarios to assert the actual spellbook result.
	registry.registerProbe("domain", []()
	{
		JsonNode snapshot;
		snapshot["active"].Bool() = spellFixture.active;
		const JsonNode saved = persistentStorage["battleModeSettings"];
		const JsonNode & selectedSpells = saved["slots"][0]["spells"];
		if(selectedSpells.isVector())
		{
			snapshot["spell_count"].Integer() = static_cast<si64>(selectedSpells.Vector().size());
			const std::string targetSpell = spellFixture.targetSpell
				? SpellID::encode(spellFixture.targetSpell->getNum())
				: "";
			snapshot["target_spell_present"].Bool() = spellFixture.targetSpell
				&& std::any_of(selectedSpells.Vector().begin(), selectedSpells.Vector().end(),
					[&targetSpell](const JsonNode & entry)
				{
					return entry.isString() && entry.String() == targetSpell;
				});
		}
		else
		{
			snapshot["spell_count"].Integer() = 0;
			snapshot["target_spell_present"].Bool() = false;
		}
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
	if(!applyJoystickAxisBindingFixture(error))
	{
		fail(E2E_SCENARIO_ERROR, error);
		return;
	}
	if(!runPrelude(error))
	{
		fail(E2E_SCENARIO_ERROR, "prelude failed: " + error);
		return;
	}
}

ControllerE2EExecutor::StepApplyResult
ControllerE2EExecutor::applyPrePollStep(const ScenarioStep & step, std::string & error)
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
		if(!state.device->primeButtonsPressed(step.heldControls, error))
			return StepApplyResult::FAILED;
		state.detachedByScenario = false;
		return StepApplyResult::APPLIED;
	}
	case ScenarioStep::Kind::REMAP:
	{
		auto * device = deviceFor(step.device);
		if(!device)
			return StepApplyResult::FAILED;
		SDL_Event event{};
		event.type = SDL_CONTROLLERDEVICEREMAPPED;
		event.cdevice.type = event.type;
		event.cdevice.which = device->getInstanceId();
		if(SDL_PushEvent(&event) != 1)
		{
			error = "SDL_PushEvent failed: " + std::string(SDL_GetError());
			return StepApplyResult::FAILED;
		}
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
			PendingTap tap;
			tap.device = step.device;
			tap.control = step.control;
			tap.releaseAtFrame = static_cast<int>(frame) + step.holdFrames;
			pendingTaps.push_back(tap);
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
		if(step.kind == ScenarioStep::Kind::MOVE_MOUSE || step.moveMousePointer)
		{
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
	case ScenarioStep::Kind::SCROLL_MOUSE:
	{
		SDL_Event wheel{};
		wheel.type = SDL_MOUSEWHEEL;
		wheel.wheel.type = SDL_MOUSEWHEEL;
		wheel.wheel.x = step.wheelX;
		wheel.wheel.y = step.wheelY;
		if(SDL_PushEvent(&wheel) != 1)
		{
			error = "SDL_PushEvent failed: " + std::string(SDL_GetError());
			return StepApplyResult::FAILED;
		}
		return StepApplyResult::APPLIED;
	}
	case ScenarioStep::Kind::REJECT_NEXT_BATTLE_ACTION:
		rejectNextBattleAction = true;
		return StepApplyResult::APPLIED;
	default:
		error = "operation '" + step.kindName() + "' cannot run before poll";
		return StepApplyResult::FAILED;
	}
}

void ControllerE2EExecutor::applyScheduledState()
{
	for(auto tap = pendingTaps.begin(); tap != pendingTaps.end();)
	{
		if(static_cast<int>(frame) < tap->releaseAtFrame)
		{
			++tap;
			continue;
		}
		const auto found = devices.find(tap->device);
		if(found != devices.end() && found->second.device)
		{
			std::string error;
			found->second.device->setButton(tap->control, false, error);
			if(!error.empty())
			{
				fail(E2E_DRIVER_ERROR, "scheduled tap release failed: " + error);
				return;
			}
			pressBlockedUntilPoll[tap->device + "/" + tap->control] = frame;
		}
		tap = pendingTaps.erase(tap);
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

const JsonNode
ControllerE2EExecutor::readProbeField(const std::string & probe, const std::string & field, std::string & error)
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
	if(spellFixture.pendingSetup)
	{
		tryPrepareSpellFixture();
		return;
	}
	if(spellFixture.pendingChoice)
	{
		if(ENGINE->captureChildren)
			return;
		auto choiceWindow = ENGINE->windows().topWindow<CInfoWindow>();
		const size_t choiceIndex = *spellFixture.pendingChoice;
		if(!choiceWindow || choiceWindow->buttons.size() <= choiceIndex
			|| choiceWindow->buttons[choiceIndex]->isBlocked())
		{
			fail(E2E_SCENARIO_ERROR, "battle-only spell fixture could not select the production spell action");
			return;
		}
		choiceWindow->buttons[choiceIndex]->clickPressed(Point());
		choiceWindow->buttons[choiceIndex]->clickReleased(Point());
		spellFixture.pendingChoice.reset();
		spellFixture.active = ENGINE->windows().topWindow<CObjectListWindow>() != nullptr;
		if(!spellFixture.active)
		{
			fail(E2E_SCENARIO_ERROR, "battle-only spell action did not open the production object list");
			return;
		}
	}

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
		case ScenarioStep::Kind::REMAP:
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
		case ScenarioStep::Kind::SCROLL_MOUSE:
		case ScenarioStep::Kind::REJECT_NEXT_BATTLE_ACTION:
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
			takeoverReason =
				inputModeName(current) + "_after_" + (lastEventKind.empty() ? std::string("unknown") : lastEventKind);
			lastInputMode = current;
		}
	}

	const auto now = std::chrono::steady_clock::now();
	if(std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() > scenario.timeoutMs)
	{
		fail(
			E2E_TIMEOUT,
			"scenario timeout of " + std::to_string(scenario.timeoutMs) + " ms exceeded at frame "
				+ std::to_string(frame)
		);
		return;
	}
	if(spellFixture.pendingSetup || spellFixture.pendingChoice)
		return;

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
	case ScenarioStep::Kind::REMAP:
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
	case ScenarioStep::Kind::SCROLL_MOUSE:
	case ScenarioStep::Kind::REJECT_NEXT_BATTLE_ACTION:
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
			fail(
				E2E_TIMEOUT,
				"step " + std::to_string(step.index) + " wait_until timed out after " + std::to_string(step.timeoutMs)
					+ " ms; last observed value: " + (readError.empty() ? observed.toString() : readError)
			);
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
		std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> composedSurface(nullptr, SDL_FreeSurface);
		if(surface && ENGINE && ENGINE->cursor().controllerE2EVisible())
		{
			composedSurface.reset(SDL_ConvertSurface(surface, surface->format, 0));
			if(composedSurface)
			{
				Canvas canvas = Canvas::createFromSurface(composedSurface.get(), CanvasScalingPolicy::AUTO);
				canvas.draw(
					ENGINE->cursor().getCurrentImage(),
					ENGINE->getCursorPosition() - ENGINE->cursor().getPivotOffset());
				surface = composedSurface.get();
			}
		}
		const bool ok = artifacts->savePng(
			surface, step.name, step.hasRegion, step.regionX, step.regionY, step.regionW, step.regionH, savedName
		);
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

bool ControllerE2EExecutor::consumeNextBattleActionRejection()
{
	const bool result = rejectNextBattleAction;
	rejectNextBattleAction = false;
	return result;
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

void ControllerE2EExecutor::recordBattleOnlyStartInfoApplied(
	const std::shared_ptr<BattleOnlyModeStartInfo> & startInfo)
{
	if(!spellFixture.startInfoRequested || !spellFixture.targetSpell || !startInfo)
		return;
	if(startInfo->spells[1].size() == 1 && startInfo->spells[1].front() == *spellFixture.targetSpell)
		spellFixture.startInfoApplied.store(true, std::memory_order_release);
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

	const auto elapsed =
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();

	JsonNode run;
	run["schema"].String() = "vcmi.controller-e2e/run/v1";
	run["scenario_id"].String() = scenario.id;
	run["scenario_path"].String() = scenarioPath.string();
	run["scenario_digest_sha256"].String() = scenarioDigest;
	run["result"].String() = exitCode == E2E_PASS && !artifacts->hasWriteErrors() ? "PASS" : "FAIL";
	run["exit_code"].Integer() = exitCode;
	run["frames"].Integer() = static_cast<si64>(frame);
	run["duration_ms"].Integer() = elapsed;
	run["pid"].Integer() = static_cast<si64>(
#ifdef VCMI_WINDOWS
		_getpid()
#else
		::getpid()
#endif
	);
	run["source_sha"].String() = GameConstants::GIT_SHA1;
	run["version"].String() = GameConstants::VCMI_VERSION;

	SDL_version linked{};
	SDL_GetVersion(&linked);
	run["sdl_compile_version"].String() = std::to_string(SDL_MAJOR_VERSION) + "." + std::to_string(SDL_MINOR_VERSION)
										+ "." + std::to_string(SDL_PATCHLEVEL);
	run["sdl_linked_version"].String() =
		std::to_string(linked.major) + "." + std::to_string(linked.minor) + "." + std::to_string(linked.patch);
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
	pushSpellFixture(true);
}

void ControllerE2EExecutor::pushRemoveSpellFixture()
{
	pushSpellFixture(false);
}

void ControllerE2EExecutor::pushSpellFixture(bool add)
{
	if(spellFixture.active)
		return;
	if(!ENGINE || !GAME || !LIBRARY || !LIBRARY->spellh)
	{
		fail(E2E_DRIVER_ERROR, "battle-only spell fixture requires initialized engine, game, and spell library");
		return;
	}

	const auto allSpells = sortedCombatSpellsForFixture();
	std::vector<SpellID> selectedSpells;
	if(!add)
	{
		const auto & preselected = scenario.fixture["preselected"];
		if(preselected.isVector())
		{
			for(const auto & entry : preselected.Vector())
			{
				if(entry.getType() != JsonNode::JsonType::DATA_INTEGER
					|| entry.Integer() < 0
					|| entry.Integer() >= static_cast<si64>(allSpells.size()))
				{
					fail(E2E_SCENARIO_ERROR, "fixture preselected index out of range");
					return;
				}
				selectedSpells.push_back(allSpells.at(static_cast<size_t>(entry.Integer())));
			}
		}
		if(selectedSpells.empty())
		{
			fail(E2E_SCENARIO_ERROR, "remove fixture requires a non-empty preselected spellbook");
			return;
		}
	}

	const auto & selectableSpells = add ? allSpells : selectedSpells;
	if(selectableSpells.size() < 2)
	{
		fail(E2E_SCENARIO_ERROR, "battle-only spell fixture requires at least two selectable spells");
		return;
	}
	spellFixture.add = add;
	spellFixture.targetSpell = selectableSpells[1];
	spellFixture.selectedSpells = std::move(selectedSpells);
	spellFixture.pendingSetup = true;

	auto & server = GAME->server();
	server.resetStateForLobby(EStartMode::NEW_GAME, ESelectionScreen::newGame, EServerMode::LOCAL, {});
	server.startLocalServerAndConnect(false);
}

void ControllerE2EExecutor::tryPrepareSpellFixture()
{
	if(GAME->server().getState() != EClientState::LOBBY)
		return;

	auto lobby = ENGINE->windows().topWindow<CLobbyScreen>();
	if(!lobby || !lobby->tabBattleOnlyMode)
		return;

	if(!spellFixture.startInfoRequested)
	{
		auto startInfo = std::make_shared<BattleOnlyModeStartInfo>();
		startInfo->spells[0] = spellFixture.selectedSpells;
		startInfo->spells[1] = {*spellFixture.targetSpell};
		spellFixture.startInfoRequested = true;
		GAME->server().setBattleOnlyModeStartInfo(startInfo);
		return;
	}
	if(!spellFixture.startInfoApplied.load(std::memory_order_acquire))
		return;

	BattleOnlyModeHeroSelector * selector = nullptr;
	for(auto * child : lobby->tabBattleOnlyMode->children)
	{
		selector = dynamic_cast<BattleOnlyModeHeroSelector *>(child);
		if(selector)
			break;
	}
	if(!selector)
	{
		spellFixture.pendingSetup = false;
		fail(E2E_SCENARIO_ERROR, "battle-only spell fixture could not find the production hero selector");
		return;
	}

	selector->manageSpells();
	spellFixture.pendingSetup = false;
	spellFixture.pendingChoice = spellFixture.add ? 0 : 1;
}

bool ControllerE2EExecutor::applyCreatureBonusFixtures()
{
	const JsonNode & overrides = scenario.fixture["creatureBonuses"];
	if(overrides.getType() == JsonNode::JsonType::DATA_NULL)
		return true;
	if(!overrides.isVector())
	{
		fail(E2E_SCENARIO_ERROR, "battle-map-start fixture field 'creatureBonuses' must be an array");
		return false;
	}

	struct SupportedCreatureBonus
	{
		BonusType type;
		bool requiresValue;
	};
	const std::map<std::string, SupportedCreatureBonus> supportedBonuses = {
		{"forgetfulness", {BonusType::FORGETFULL, true}},
		{"limited_shooting_range", {BonusType::LIMITED_SHOOTING_RANGE, true}},
		{"long_weapon", {BonusType::LONG_WEAPON, false}},
		{"no_distance_penalty", {BonusType::NO_DISTANCE_PENALTY, false}},
		{"shots", {BonusType::SHOTS, true}}
	};

	for(const JsonNode & entry : overrides.Vector())
	{
		if(!entry.isStruct() || !entry["creature"].isString() || !entry["bonus"].isString())
		{
			fail(E2E_SCENARIO_ERROR, "each creatureBonuses entry requires string fields 'creature' and 'bonus'");
			return false;
		}

		const std::string creatureName = entry["creature"].String();
		const std::string bonusName = entry["bonus"].String();
		const auto bonus = supportedBonuses.find(bonusName);
		if(bonus == supportedBonuses.end())
		{
			fail(E2E_SCENARIO_ERROR, "unsupported creatureBonuses bonus: " + bonusName);
			return false;
		}

		const JsonNode & valueNode = entry["value"];
		if(bonus->second.requiresValue && valueNode.getType() != JsonNode::JsonType::DATA_INTEGER)
		{
			fail(E2E_SCENARIO_ERROR, "creatureBonuses bonus '" + bonusName + "' requires integer field 'value'");
			return false;
		}
		if(!bonus->second.requiresValue && valueNode.getType() != JsonNode::JsonType::DATA_NULL)
		{
			fail(E2E_SCENARIO_ERROR, "creatureBonuses flag bonus '" + bonusName + "' does not accept field 'value'");
			return false;
		}
		const si64 value = bonus->second.requiresValue ? valueNode.Integer() : 0;
		if(value < std::numeric_limits<si32>::min() || value > std::numeric_limits<si32>::max())
		{
			fail(E2E_SCENARIO_ERROR, "creatureBonuses field 'value' is outside the supported integer range");
			return false;
		}

		try
		{
			const CreatureID creatureId(CreatureID::decode(creatureName));
			if(creatureId.getNum() < 0 || creatureId.getNum() >= static_cast<si32>(LIBRARY->creh->objects.size()))
				throw std::out_of_range("creature id is outside the loaded library");
			auto creature = LIBRARY->creh->objects.at(creatureId.getNum());
			if(!creature)
				throw std::runtime_error("creature is not loaded");
			if(bonus->second.requiresValue || !creature->hasBonusOfType(bonus->second.type))
				creature->addBonus(static_cast<si32>(value), bonus->second.type);
		}
		catch(const std::exception & error)
		{
			fail(E2E_SCENARIO_ERROR, "invalid creatureBonuses creature '" + creatureName + "': " + error.what());
			return false;
		}
	}

	return true;
}

int ControllerE2EExecutor::startBattleMapGame()
{
	if(!GAME)
	{
		fail(E2E_DRIVER_ERROR, "battle-map-start fixture requires an initialized game instance");
		return exitCode;
	}

	const JsonNode & mapNode = scenario.fixture["map"];
	if(!mapNode.isString() || mapNode.String().empty())
	{
		fail(E2E_SCENARIO_ERROR, "battle-map-start fixture requires a non-empty fixture map name");
		return exitCode;
	}
	if(!applyCreatureBonusFixtures())
		return exitCode;

	try
	{
		const std::string mapResource = "Maps/" + mapNode.String();
		auto mapInfo = std::make_shared<CMapInfo>();
		mapInfo->mapInit(mapResource);

		auto & server = GAME->server();
		server.resetStateForLobby(EStartMode::NEW_GAME, ESelectionScreen::newGame, EServerMode::LOCAL, {});
		server.startLocalServerAndConnect(false);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		while(!ENGINE->windows().topWindow<CLobbyScreen>())
			std::this_thread::sleep_for(std::chrono::milliseconds(50));

		while(!server.mi || server.mi->fileURI != mapInfo->fileURI)
		{
			server.setMapInfo(mapInfo);
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}

		autofightPlayerColors.clear();
		for(const JsonNode & colorNode : scenario.fixture["autofightPlayers"].Vector())
		{
			if(colorNode.isString())
				autofightPlayerColors.push_back(colorNode.String());
		}

		for(const std::string & colorName : autofightPlayerColors)
		{
			PlayerColor color = PlayerColor::CANNOT_DETERMINE;
			for(size_t index = 0; index < PlayerColor::PLAYER_LIMIT_I; ++index)
				if(GameConstants::PLAYER_COLOR_NAMES[index] == colorName)
					color = PlayerColor(static_cast<int>(index));

			if(!color.isValidPlayer())
			{
				fail(E2E_SCENARIO_ERROR, "battle-map-start fixture received unknown autofight player color: " + colorName);
				return exitCode;
			}

			int seatingAttempts = 0;
			while(server.si->playerInfos.count(color) == 0 || server.si->playerInfos.at(color).connectedPlayerIDs.empty())
			{
				if(++seatingAttempts > 200)
				{
					fail(E2E_DRIVER_ERROR, "battle-map-start fixture could not seat autofight player color: " + colorName);
					return exitCode;
				}
				LobbyForceSetPlayer seat;
				seat.targetConnectedPlayer = server.myFirstId();
				seat.targetPlayerColor = color;
				server.sendLobbyPack(seat);
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
		}

		ENGINE->input().seedControllerInputModeForE2E();

		while(true)
		{
			try
			{
				server.sendStartGame();
				break;
			}
			catch(...)
			{
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		return E2E_PASS;
	}
	catch(const std::exception & e)
	{
		fail(E2E_SCENARIO_ERROR, std::string("battle-map-start fixture failed: ") + e.what());
		return exitCode;
	}
}

bool ControllerE2EExecutor::shouldAutoFightE2E(const std::string & colorName) const
{
	return vstd::contains(autofightPlayerColors, colorName);
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

void recordBattleOnlyStartInfoApplied(const std::shared_ptr<BattleOnlyModeStartInfo> & startInfo)
{
	if(auto * executor = ControllerE2EExecutor::instance())
		executor->recordBattleOnlyStartInfoApplied(startInfo);
}

}

}
