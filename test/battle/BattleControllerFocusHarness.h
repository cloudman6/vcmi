/*
 * BattleControllerFocusHarness.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/battle/BattleHex.h"
#include "../../lib/battle/BattleAction.h"

#include <optional>

class BattleActionsController;

/// Topic-local spike harness for focus routing. It deliberately owns no battle
/// legality: previews and confirmation remain delegated to the action controller.
class BattleControllerFocusHarness
{
public:
	class Sink
	{
	public:
		virtual ~Sink() = default;

		virtual void preview(const BattleHex & hex) = 0;
		virtual void submit(const BattleHex & hex) = 0;
	};

	explicit BattleControllerFocusHarness(Sink & sink);

	bool setFocus(BattleHex hex);
	bool move(BattleHex::EDir direction);
	bool selectAttackDirection(BattleHex::EDir direction);
	void onActionReply(const BattleAction & action);
	bool confirm();

	const BattleHex & focusedHex() const;
	const std::optional<BattleHex::EDir> & attackDirection() const;

private:
	static bool isHexDirection(BattleHex::EDir direction);

	Sink & sink;
	BattleHex focused = BattleHex::INVALID;
	std::optional<BattleHex::EDir> direction;
};

/// Adapter intentionally dispatches through BattleActionsController. This is a
/// test-only bridge, not a new runtime input or navigation API.
class BattleActionsControllerFocusSink final : public BattleControllerFocusHarness::Sink
{
public:
	explicit BattleActionsControllerFocusSink(BattleActionsController & actions);

	void preview(const BattleHex & hex) override;
	void submit(const BattleHex & hex) override;

private:
	BattleActionsController & actions;
};
