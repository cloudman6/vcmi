/*
 * ControllerE2EExecutor.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "ControllerE2EScenario.h"
#include "ControllerE2EVirtualController.h"

#include "../eventsSDL/InputHandler.h"

#include <boost/filesystem/path.hpp>

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

union SDL_Event;
class JsonNode;
class BattleOnlyModeStartInfo;

namespace ControllerE2E
{

class ArtifactWriter;

enum E2EExitCode
{
	E2E_PASS = 0,
	E2E_SCENARIO_ERROR = 10,
	E2E_ASSERTION_FAILURE = 11,
	E2E_TIMEOUT = 12,
	E2E_ABNORMAL = 13,
	E2E_ARTIFACT_FAILURE = 14,
	E2E_DRIVER_ERROR = 15,
	E2E_UNSUPPORTED_BINARY = 16
};

/// Deterministic frame-phase scheduler that drives one scenario inside the
/// real vcmiclient process. All virtual controller state enters through
/// SDL_JoystickSetVirtual* and the normal SDL poll; keyboard/mouse takeover
/// input enters through SDL keyboard/mouse events. The executor never injects
/// SDL_CONTROLLER* events and never calls shortcuts or UI actions directly.
class ControllerE2EExecutor
{
	struct DeviceState
	{
		std::unique_ptr<SDLVirtualController> device;
		bool detachedByScenario = false;
	};

	struct PendingTap
	{
		std::string device;
		std::string control;
		int releaseAtFrame = 0;
	};

	struct PendingRamp
	{
		std::string device;
		std::string control;
		double from = 0.0;
		double to = 0.0;
		int startFrame = 0;
		int frames = 1;
	};

	struct WaitState
	{
		bool active = false;
		bool sawInitialValue = false;
		JsonNode baseline;
		int stableFrames = 0;
		std::chrono::steady_clock::time_point deadline;
	};

	struct UnchangedState
	{
		bool active = false;
		JsonNode baseline;
		int framesLeft = 0;
	};

	ScenarioSpec scenario;
	std::unique_ptr<ArtifactWriter> artifacts;
	boost::filesystem::path scenarioPath;
	std::string scenarioDigest;
	std::chrono::steady_clock::time_point startTime;

	std::map<std::string, DeviceState> devices;
	std::map<std::string, VirtualControllerProfile> profiles;

	size_t stepCursor = 0;
	uint64_t frame = 0;
	bool activated = false;
	bool finished = false;
	bool allowShutdownThrow = false;
	int exitCode = E2E_DRIVER_ERROR;
	std::vector<std::string> resultMessages;

	std::vector<PendingTap> pendingTaps;
	std::vector<PendingRamp> pendingRamps;
	/// control key -> frame whose SDL poll observed the release; a new press
	/// on the same control must wait for the next poll or SDL collapses the
	/// release+press state transition into no event
	std::map<std::string, uint64_t> pressBlockedUntilPoll;
	WaitState waitState;
	UnchangedState unchangedState;

	std::deque<JsonNode> recordedEvents;
	std::map<std::string, int> eventCounts;
	InputMode lastInputMode = InputMode::KEYBOARD_AND_MOUSE;
	std::string lastEventKind;
	std::string takeoverReason;
	std::string selectedDeviceAlias;
	uint64_t modeChangeFrame = 0;
	std::vector<std::string> autofightPlayerColors;
	std::vector<std::string> manualBattlePlayerColors;
	bool rejectNextBattleAction = false;

	void registerBuiltinProbes();
	void registerProfiles();
	bool applyJoystickAxisBindingFixture(std::string & error);
	bool runPrelude(std::string & error);
	enum class StepApplyResult { APPLIED, PENDING, FAILED };
	StepApplyResult applyPrePollStep(const ScenarioStep & step, std::string & error);
	void advanceStepPointer();
	void applyScheduledState();
	void fail(int code, const std::string & message);
	void finish(int code, const std::string & message);
	void cleanupDevices();
	void writeStepRecord(const JsonNode & record);
	void recordSdlEventIdentity(const JsonNode & record);
	void recordEvent(const JsonNode & event);
	void flushEvents();
	bool applyCreatureBonusFixtures();
	bool evaluateCondition(bool & satisfied, std::string & error);
	const JsonNode readProbeField(const std::string & probe, const std::string & field, std::string & error);
	void pushSpellFixture(bool add);
	void tryPrepareSpellFixture();
	void throwShutdownIfAllowed();

public:
	ControllerE2EExecutor(
		ScenarioSpec scenario,
		boost::filesystem::path outputDir,
		boost::filesystem::path scenarioPath,
		std::string scenarioDigest
	);

	static ControllerE2EExecutor * instance();

	/// Validates the scenario file and prepares artifacts before any engine
	/// state exists. Returns E2E_PASS to continue, otherwise an exit code.
	static int earlyLoad(const std::string & scenarioPath, const std::string & outputDir);

	/// Hook: SDL initialized, InputHandler not constructed yet
	void activate();
	/// Hook: immediately before InputHandler::fetchEvents
	void onBeforePoll();
	/// Hook: immediately after presentScreenTexture
	void onAfterPresent();
	/// Hook: every SDL event seen by InputHandler::preprocessEvent
	void recordSdlEvent(const SDL_Event & event);
	/// Hook: controller shortcut routing invoked the virtual left mouse action
	void recordShortcutMouseLeft(bool pressed);
	/// Hook: the network thread finished applying a battle-only start-info pack
	void recordBattleOnlyStartInfoApplied(const std::shared_ptr<BattleOnlyModeStartInfo> & startInfo);

	/// Fixture support: requested fixture kind from scenario fixture section
	std::string fixtureKind() const;
	/// Fixture support: opens the battle-only Add Spell consumer window
	void pushAddSpellFixture();
	/// Fixture support: opens the battle-only Remove Spell consumer window
	void pushRemoveSpellFixture();
	int startBattleMapGame();
	bool shouldAutoFightE2E(const std::string & colorName) const;
	bool shouldUseManualBattleE2E(const std::string & colorName) const;
	bool consumeNextBattleActionRejection();

	/// Writes final manifest and returns the process exit code
	int finalize();

	bool isFinished() const { return finished; }
	uint64_t getFrame() const { return frame; }
};

/// No-op facade used by production hook sites; all methods are inert when no
/// executor is active.
namespace Hooks
{
void onBeforeInputHandler();
void onBeforePoll();
void onAfterPresent();
void recordSdlEvent(const SDL_Event & event);
void recordShortcutMouseLeft(bool pressed);
void recordBattleOnlyStartInfoApplied(const std::shared_ptr<BattleOnlyModeStartInfo> & startInfo);
}

}
