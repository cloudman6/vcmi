/*
 * ControllerE2EScenario.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/json/JsonNode.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace ControllerE2E
{

/// Versioned, strictly validated scenario protocol. Scenarios express inputs
/// with physical-position control names only; they never carry C++ callbacks,
/// class names, shell commands or EShortcut/action identifiers.
struct ScenarioStep
{
	enum class Kind
	{
		ATTACH,
		DETACH,
		RECONNECT,
		REMAP,
		SELECT_DEVICE,
		PRESS,
		RELEASE,
		TAP,
		SET_AXIS,
		RAMP_AXIS,
		NEUTRALIZE,
		PRESS_KEY,
		RELEASE_KEY,
		TEXT_INPUT,
		MOVE_MOUSE,
		CLICK_MOUSE,
		SCROLL_MOUSE,
		REJECT_NEXT_BATTLE_ACTION,
		AUTHORITY_REMOVE_BATTLE_STACK,
		AUTHORITY_SET_ACTIVE_BATTLE_STACK,
		AUTHORITY_SWAP_ADVENTURE_HEROES,
		AUTHORITY_REMOVE_ADVENTURE_OBJECT,
		WAIT_FRAMES,
		WAIT_UNTIL,
		ASSERT,
		ASSERT_UNCHANGED,
		ASSERT_EVENT_COUNT,
		CHECKPOINT,
		CAPTURE_FRAME,
		CAPTURE_REGION,
		MANUAL_HOLD,
		REQUEST_SHUTDOWN
	};

	Kind kind = Kind::WAIT_FRAMES;
	int index = -1;

	/// device alias (device ops and input ops)
	std::string device;
	/// profile id for attach
	std::string profileId;
	/// physical control name for press/release/tap/set_axis/ramp_axis
	std::string control;
	/// controls already held when a reconnected device becomes visible
	std::vector<std::string> heldControls;

	int holdFrames = 1; /// tap expansion: explicit press/release distance
	double axisValue = 0.0; /// set_axis normalized value
	double axisFrom = 0.0; /// ramp_axis start
	double axisTo = 0.0; /// ramp_axis end
	int rampFrames = 0; /// ramp_axis duration

	int frames = 0; /// wait_frames / assert_unchanged

	/// wait_until condition
	std::string probe;
	std::string field; /// dotted path inside probe snapshot
	bool expectChange = false; /// wait_until "changes" mode
	JsonNode expected; /// wait_until/assert "equals" value
	int timeoutMs = 0;
	int stableForFrames = 1;

	/// assert_event_count
	std::string eventKind;
	int eventCount = 0;

	/// keyboard/mouse takeover input
	std::string keyName;
	std::string text;
	std::string mouseButton;
	int mouseX = 0;
	int mouseY = 0;
	int wheelX = 0;
	int wheelY = 0;
	bool moveMousePointer = true;
	int stackId = -1; /// authority battle-state injection
	int objectId = -1; /// authority adventure object removal
	int firstIndex = -1; /// authority adventure list reorder
	int secondIndex = -1; /// authority adventure list reorder

	/// evidence
	std::string name;
	bool hasRegion = false;
	int regionX = 0;
	int regionY = 0;
	int regionW = 0;
	int regionH = 0;

	/// original document of this step for scenario.resolved.json
	JsonNode raw;

	std::string kindName() const;
};

struct ScenarioSpec
{
	static constexpr const char * SCHEMA = "vcmi.controller-e2e/v1";

	std::string id;
	std::string profileId;
	int timeoutMs = 30000;
	/// fixture description, opaque to the parser; interpreted by the driver
	JsonNode fixture;
	/// steps allowed before any frame runs: device attach/select/neutralize only
	std::vector<ScenarioStep> prelude;
	std::vector<ScenarioStep> steps;
	/// resolved document written to scenario.resolved.json
	JsonNode resolved;
};

struct ScenarioParseResult
{
	std::optional<ScenarioSpec> scenario;
	std::vector<std::string> errors;
	bool ok() const { return scenario.has_value() && errors.empty(); }
};

/// Parses and strictly validates a scenario document. Validation is fully
/// structural and happens before any device is attached or state is modified.
ScenarioParseResult parseScenarioText(const std::string & text);

using ScenarioProfileButtons = std::map<std::string, std::set<std::string>>;

/// Validates profile and held-button references before any device is attached.
bool validateScenarioDevices(
	const ScenarioSpec & scenario,
	const ScenarioProfileButtons & profileButtons,
	std::vector<std::string> & errors);

}
