/*
 * BattleActionLifecycle.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/battle/BattleAction.h"

#include <functional>

/// Internal BattleInterface seam for command and reply ordering. It is not an
/// input API; BattleInterface remains its sole production caller.
class BattleActionLifecycle
{
public:
	enum class Stage
	{
		COMMAND_GIVEN,
		COMMAND_SENT,
		ACTION_STARTED,
		ACTION_ENDED,
	};

	using Observer = std::function<void(Stage, const BattleAction &)>;

	void subscribe(Observer observer);
	void giveCommand(const BattleAction & action);
	void sendCommand(const BattleAction & action);
	void startAction(const BattleAction & action);
	void endAction(const BattleAction & action);

private:
	void notify(Stage stage, const BattleAction & action);

	std::vector<Observer> observers;
};
