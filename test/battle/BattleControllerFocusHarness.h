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

#include "../../lib/battle/BattleAction.h"
#include "../../lib/battle/BattleHex.h"

#include "../../client/battle/BattleActionLifecycle.h"

#include <optional>

class BattleActionsController;

/// Topic-local spike harness for focus routing. It deliberately owns no battle
/// legality: previews and confirmation remain delegated to the action controller.
class BattleControllerFocusHarness
{
public:
	struct Selection
	{
		BattleHex targetHex;
		std::optional<BattleHex::EDir> attackDirection;
	};

	class Sink
	{
	public:
		virtual ~Sink() = default;

		virtual void preview(const Selection & selection) = 0;
		virtual void submit(const Selection & selection) = 0;
	};

	BattleControllerFocusHarness(Sink & sink, BattleActionLifecycle & lifecycle);

	bool setFocus(BattleHex hex);
	bool move(BattleHex::EDir direction);
	bool selectAttackDirection(BattleHex::EDir direction);
	bool confirm();

	const BattleHex & focusedHex() const;
	const std::optional<BattleHex::EDir> & attackDirection() const;

private:
	static bool isHexDirection(BattleHex::EDir direction);
	Selection selection() const;
	void onLifecycleEvent(BattleActionLifecycle::Stage stage, const BattleAction & action);

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

	void preview(const BattleControllerFocusHarness::Selection & selection) override;
	void submit(const BattleControllerFocusHarness::Selection & selection) override;

private:
	BattleActionsController & actions;
};
