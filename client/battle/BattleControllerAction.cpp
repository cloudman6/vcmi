/*
 * BattleControllerAction.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "StdInc.h"

#include "BattleControllerAction.h"

namespace
{
constexpr uint32_t MELEE_ORIGIN_REPEAT_DELAY_MS = 320;
constexpr uint32_t MELEE_ORIGIN_REPEAT_INTERVAL_MS = 110;

bool isMeleeAction(PossiblePlayerBattleAction::Actions action)
{
	switch(action)
	{
	case PossiblePlayerBattleAction::ATTACK:
	case PossiblePlayerBattleAction::LONG_WEAPON_ATTACK:
	case PossiblePlayerBattleAction::WALK_AND_ATTACK:
	case PossiblePlayerBattleAction::ATTACK_AND_RETURN:
		return true;
	default:
		return false;
	}
}
}

bool BattleControllerActionPressState::press(BattleControllerPrimaryAction action, const BattleHex & focusedHex,
	const BattleHex & attackFrom, BattleHex::EDir attackDirection,
	PossiblePlayerBattleAction::Actions meleeAction, std::optional<uint32_t> targetUnitId)
{
	if(action == BattleControllerPrimaryAction::NONE || !focusedHex.isValid())
		return false;
	if(action == BattleControllerPrimaryAction::ATTACK && (!isMeleeAction(meleeAction) || !targetUnitId))
		return false;

	if(pressedAction == BattleControllerPrimaryAction::NONE)
	{
		pressedAction = action;
		pressedHex = focusedHex;
		pressedAttackFrom = attackFrom;
		pressedAttackDirection = attackDirection;
		pressedMeleeAction = meleeAction;
		pressedTargetUnitId = targetUnitId;
	}
	return true;
}

BattleControllerPrimaryAction BattleControllerActionPressState::release(
	BattleControllerPrimaryAction action, const BattleHex & focusedHex,
	const BattleHex & attackFrom, BattleHex::EDir attackDirection,
	PossiblePlayerBattleAction::Actions meleeAction, std::optional<uint32_t> targetUnitId)
{
	const auto result = isPressed(action, focusedHex, attackFrom, attackDirection, meleeAction, targetUnitId)
		? action
		: BattleControllerPrimaryAction::NONE;
	reset();
	return result;
}

bool BattleControllerActionPressState::hasPendingAction() const
{
	return pressedAction != BattleControllerPrimaryAction::NONE;
}

bool BattleControllerActionPressState::isPressed(
	BattleControllerPrimaryAction action, const BattleHex & focusedHex,
	const BattleHex & attackFrom, BattleHex::EDir attackDirection,
	PossiblePlayerBattleAction::Actions meleeAction, std::optional<uint32_t> targetUnitId) const
{
	return action != BattleControllerPrimaryAction::NONE
		&& action == pressedAction
		&& focusedHex == pressedHex
		&& attackFrom == pressedAttackFrom
		&& attackDirection == pressedAttackDirection
		&& meleeAction == pressedMeleeAction
		&& targetUnitId == pressedTargetUnitId;
}

void BattleControllerActionPressState::reset()
{
	pressedAction = BattleControllerPrimaryAction::NONE;
	pressedHex = BattleHex::INVALID;
	pressedAttackFrom = BattleHex::INVALID;
	pressedAttackDirection = BattleHex::NONE;
	pressedMeleeAction = PossiblePlayerBattleAction::INVALID;
	pressedTargetUnitId.reset();
}

void BattleControllerMeleeOriginRepeatState::press(
	bool forward, const BattleControllerMeleeOriginRepeatContext & context)
{
	if(pressedDirection == forward && expectedContext == context)
		return;

	pressedDirection = forward;
	expectedContext = context;
	elapsedMilliseconds = 0;
	repeating = false;
}

bool BattleControllerMeleeOriginRepeatState::release(bool forward)
{
	if(pressedDirection != forward)
		return false;

	reset();
	return true;
}

std::optional<bool> BattleControllerMeleeOriginRepeatState::update(uint32_t msPassed)
{
	if(!pressedDirection)
		return std::nullopt;

	elapsedMilliseconds += msPassed;
	const uint32_t threshold = repeating ? MELEE_ORIGIN_REPEAT_INTERVAL_MS : MELEE_ORIGIN_REPEAT_DELAY_MS;
	if(elapsedMilliseconds < threshold)
		return std::nullopt;

	elapsedMilliseconds = 0;
	repeating = true;
	return pressedDirection;
}

bool BattleControllerMeleeOriginRepeatState::matchesContext(
	const BattleControllerMeleeOriginRepeatContext & context) const
{
	return pressedDirection && expectedContext == context;
}

bool BattleControllerMeleeOriginRepeatState::retainContext(const BattleControllerMeleeOriginRepeatContext & context)
{
	if(matchesContext(context))
		return true;

	reset();
	return false;
}

void BattleControllerMeleeOriginRepeatState::selectionAdvanced(
	const BattleControllerMeleeOriginRepeatContext & context)
{
	if(pressedDirection)
		expectedContext = context;
}

bool BattleControllerMeleeOriginRepeatState::hasPendingRepeat() const
{
	return pressedDirection.has_value();
}

void BattleControllerMeleeOriginRepeatState::reset()
{
	pressedDirection.reset();
	expectedContext.reset();
	elapsedMilliseconds = 0;
	repeating = false;
}
