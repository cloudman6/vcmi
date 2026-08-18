/*
 * BattleControllerAction.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#pragma once

#include "../../lib/battle/BattleHex.h"
#include "../../lib/battle/PossiblePlayerBattleAction.h"

enum class BattleControllerPrimaryAction
{
	NONE,
	MOVE,
	ATTACK,
	SHOOT,
	INSPECT
};

/// Tracks a controller primary action from button press to matching release.
class BattleControllerActionPressState
{
	BattleControllerPrimaryAction pressedAction = BattleControllerPrimaryAction::NONE;
	BattleHex pressedHex = BattleHex::INVALID;
	BattleHex pressedAttackFrom = BattleHex::INVALID;
	BattleHex::EDir pressedAttackDirection = BattleHex::NONE;
	PossiblePlayerBattleAction::Actions pressedMeleeAction = PossiblePlayerBattleAction::INVALID;
	std::optional<uint32_t> pressedTargetUnitId;

public:
	bool press(BattleControllerPrimaryAction action, const BattleHex & focusedHex,
		const BattleHex & attackFrom = BattleHex::INVALID, BattleHex::EDir attackDirection = BattleHex::NONE,
		PossiblePlayerBattleAction::Actions meleeAction = PossiblePlayerBattleAction::INVALID,
		std::optional<uint32_t> targetUnitId = std::nullopt);
	BattleControllerPrimaryAction release(BattleControllerPrimaryAction action, const BattleHex & focusedHex,
		const BattleHex & attackFrom = BattleHex::INVALID, BattleHex::EDir attackDirection = BattleHex::NONE,
		PossiblePlayerBattleAction::Actions meleeAction = PossiblePlayerBattleAction::INVALID,
		std::optional<uint32_t> targetUnitId = std::nullopt);
	bool hasPendingAction() const;
	bool isPressed(BattleControllerPrimaryAction action, const BattleHex & focusedHex,
		const BattleHex & attackFrom = BattleHex::INVALID, BattleHex::EDir attackDirection = BattleHex::NONE,
		PossiblePlayerBattleAction::Actions meleeAction = PossiblePlayerBattleAction::INVALID,
		std::optional<uint32_t> targetUnitId = std::nullopt) const;
	void reset();
};

/// Tracks held shoulder-button repeats while choosing a melee attack origin.
struct BattleControllerMeleeOriginRepeatContext
{
	PossiblePlayerBattleAction::Actions action = PossiblePlayerBattleAction::INVALID;
	BattleHex target = BattleHex::INVALID;
	std::optional<uint32_t> targetUnitId;
	BattleHex attackFrom = BattleHex::INVALID;
	BattleHex::EDir direction = BattleHex::NONE;

	bool operator==(const BattleControllerMeleeOriginRepeatContext & other) const = default;
};

class BattleControllerMeleeOriginRepeatState
{
	std::optional<bool> pressedDirection;
	std::optional<BattleControllerMeleeOriginRepeatContext> expectedContext;
	uint32_t elapsedMilliseconds = 0;
	bool repeating = false;
	bool matchesContext(const BattleControllerMeleeOriginRepeatContext & context) const;

public:
	void press(bool forward, const BattleControllerMeleeOriginRepeatContext & context);
	bool release(bool forward);
	std::optional<bool> update(uint32_t msPassed);
	bool retainContext(const BattleControllerMeleeOriginRepeatContext & context);
	void selectionAdvanced(const BattleControllerMeleeOriginRepeatContext & context);
	bool hasPendingRepeat() const;
	void reset();
};
