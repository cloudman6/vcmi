/*
 * BattleActionLifecycle.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "BattleActionLifecycle.h"

void BattleActionLifecycle::subscribe(Observer observer)
{
	observers.push_back(std::move(observer));
}

void BattleActionLifecycle::giveCommand(const BattleAction & action)
{
	notify(Stage::COMMAND_GIVEN, action);
}

void BattleActionLifecycle::sendCommand(const BattleAction & action)
{
	notify(Stage::COMMAND_SENT, action);
}

void BattleActionLifecycle::startAction(const BattleAction & action)
{
	notify(Stage::ACTION_STARTED, action);
}

void BattleActionLifecycle::endAction(const BattleAction & action)
{
	notify(Stage::ACTION_ENDED, action);
}

void BattleActionLifecycle::notify(Stage stage, const BattleAction & action)
{
	for(const auto & observer : observers)
		observer(stage, action);
}
