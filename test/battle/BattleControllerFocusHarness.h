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
#include "../../client/battle/BattleActionsControllerMeleeTargeting.h"

#include <optional>

/// Topic-local spike harness for focus routing. It deliberately owns no battle
/// legality: the production melee action seam owns preview and confirmation.
class BattleControllerFocusHarness
{
public:
	using Selection = battle::controller::MeleeTargetSelection;

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
