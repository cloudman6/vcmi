/*
 * BattleControllerFocusBattleFixture.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/battle/CBattleInfoCallback.h"

#include "mock/mock_BonusBearer.h"
#include "mock/mock_battle_IBattleState.h"
#include "mock/mock_battle_Unit.h"

class BattleControllerFocusBattleUnit final : public UnitMock
{
public:
	BattleControllerFocusBattleUnit(BattleHex position, BattleSide side, bool doubleWide, uint32_t id);

private:
	BonusBearerMock bonuses;
};

class BattleControllerFocusBattleFixture
{
	class Subject final : public CBattleInfoCallback
	{
	public:
		const IBattleInfo * battle = nullptr;

		const IBattleInfo * getBattle() const override;
		std::optional<PlayerColor> getPlayerID() const override;
	};

public:
	BattleControllerFocusBattleFixture();

	BattleControllerFocusBattleUnit & addUnit(BattleHex position, BattleSide side, bool doubleWide);
	const CBattleInfoCallback & battle() const;

private:
	battle::Units getUnitsIf(const battle::UnitFilter & predicate) const;

	Subject subject;
	testing::NiceMock<BattleStateMock> battleState;
	std::vector<std::unique_ptr<BattleControllerFocusBattleUnit>> units;
	uint32_t nextUnitId = 1;
};
