/*
 * BattleHintBar.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../StdInc.h"

#include "BattleHintBar.h"

static const std::string HINT_MOVE = "vcmi.battleWindow.hints.move";
static const std::string HINT_ATTACK = "vcmi.battleWindow.hints.attack";
static const std::string HINT_SHOOT = "vcmi.battleWindow.hints.shoot";
static const std::string HINT_AIM = "vcmi.battleWindow.hints.aim";
static const std::string HINT_ADJUST = "vcmi.battleWindow.hints.adjust";
static const std::string HINT_BACK = "vcmi.battleWindow.hints.back";
static const std::string HINT_SWITCH_UNIT = "vcmi.battleWindow.hints.switchUnit";
static const std::string HINT_REASON_NO_AMMO = "vcmi.battleWindow.hints.reason.noAmmo";
static const std::string HINT_REASON_BLOCKED_LINE = "vcmi.battleWindow.hints.reason.blockedLine";
static const std::string HINT_REASON_OUT_OF_RANGE = "vcmi.battleWindow.hints.reason.outOfRange";

std::vector<BattleHintEntry> BattleHintBar::entries(InputMode inputMode, BattleControllerStateMachine::State top, const Context & context)
{
	using State = BattleControllerStateMachine::State;

	if(inputMode != InputMode::CONTROLLER)
		return {};

	std::vector<BattleHintEntry> entries;

	const auto addBack = [&entries]()
	{
		entries.push_back({EShortcut::GLOBAL_CANCEL, HINT_BACK});
	};

	switch(top)
	{
		case State::BROWSE:
		{
			// same priority as the accept dispatch: melee beats shooting
			// beats movement; a disabled shooting target explains itself
			if(context.attackable)
				entries.push_back({EShortcut::GLOBAL_ACCEPT, HINT_ATTACK});
			else if(context.shootable)
				entries.push_back({EShortcut::GLOBAL_ACCEPT, HINT_SHOOT});
			else if(context.shootingDisabled != BattleRangedShooting::DisabledReason::NONE)
			{
				std::string reasonKey = HINT_REASON_NO_AMMO;
				if(context.shootingDisabled == BattleRangedShooting::DisabledReason::BLOCKED_LINE)
					reasonKey = HINT_REASON_BLOCKED_LINE;
				if(context.shootingDisabled == BattleRangedShooting::DisabledReason::OUT_OF_RANGE)
					reasonKey = HINT_REASON_OUT_OF_RANGE;
				entries.push_back({EShortcut::NONE, reasonKey, false});
			}
			else if(context.focusedReachable)
				entries.push_back({EShortcut::GLOBAL_ACCEPT, HINT_MOVE});

			addBack();

			// D7: the LB/RB switch prompts exist only while browsing
			entries.push_back({EShortcut::BATTLE_WAIT, HINT_SWITCH_UNIT});
			entries.push_back({EShortcut::BATTLE_DEFEND, HINT_SWITCH_UNIT});
			return entries;
		}
		case State::PREVIEW:
			entries.push_back({EShortcut::GLOBAL_ACCEPT, HINT_MOVE});
			addBack();
			return entries;
		case State::ACTION:
			if(context.attackable)
				entries.push_back({EShortcut::GLOBAL_ACCEPT, HINT_AIM});
			else if(context.shootable)
				entries.push_back({EShortcut::GLOBAL_ACCEPT, HINT_SHOOT});
			addBack();
			return entries;
		case State::ATTACK_DIRECTION:
			entries.push_back({EShortcut::GLOBAL_ACCEPT, HINT_ATTACK});
			entries.push_back({EShortcut::MOVE_LEFT, HINT_ADJUST});
			entries.push_back({EShortcut::MOVE_RIGHT, HINT_ADJUST});
			addBack();
			return entries;
		case State::TARGET:
		case State::COMMIT:
		default:
			return {};
	}
}
