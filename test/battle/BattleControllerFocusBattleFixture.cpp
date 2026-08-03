/*
 * BattleControllerFocusBattleFixture.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "BattleControllerFocusBattleFixture.h"

using namespace testing;

BattleControllerFocusBattleUnit::BattleControllerFocusBattleUnit(BattleHex position, BattleSide side, bool doubleWide)
{
	EXPECT_CALL(*this, getPosition()).Times(AnyNumber()).WillRepeatedly(Return(position));
	EXPECT_CALL(*this, unitSide()).Times(AnyNumber()).WillRepeatedly(Return(side));
	EXPECT_CALL(*this, unitId()).Times(AnyNumber()).WillRepeatedly(Return(1));
	EXPECT_CALL(*this, doubleWide()).Times(AnyNumber()).WillRepeatedly(Return(doubleWide));
	EXPECT_CALL(*this, alive()).Times(AnyNumber()).WillRepeatedly(Return(true));
	EXPECT_CALL(*this, isGhost()).Times(AnyNumber()).WillRepeatedly(Return(false));
	EXPECT_CALL(*this, isValidTarget(_)).Times(AnyNumber()).WillRepeatedly(Return(true));
	EXPECT_CALL(*this, getAllBonuses(_, _)).Times(AnyNumber()).WillRepeatedly(Invoke(&bonuses, &BonusBearerMock::getAllBonuses));
	EXPECT_CALL(*this, getTreeVersion()).Times(AnyNumber()).WillRepeatedly(Invoke(&bonuses, &BonusBearerMock::getTreeVersion));
}

const IBattleInfo * BattleControllerFocusBattleFixture::Subject::getBattle() const
{
	return battle;
}

std::optional<PlayerColor> BattleControllerFocusBattleFixture::Subject::getPlayerID() const
{
	return std::nullopt;
}

BattleControllerFocusBattleFixture::BattleControllerFocusBattleFixture()
{
	subject.battle = &battleState;
	ON_CALL(battleState, getAllObstacles()).WillByDefault(Return(IBattleInfo::ObstacleCList()));
	ON_CALL(battleState, getUnitsIf(_)).WillByDefault(Invoke(this, &BattleControllerFocusBattleFixture::getUnitsIf));
}

BattleControllerFocusBattleUnit & BattleControllerFocusBattleFixture::addUnit(BattleHex position, BattleSide side, bool doubleWide)
{
	units.push_back(std::make_unique<BattleControllerFocusBattleUnit>(position, side, doubleWide));
	return *units.back();
}

const CBattleInfoCallback & BattleControllerFocusBattleFixture::battle() const
{
	return subject;
}

battle::Units BattleControllerFocusBattleFixture::getUnitsIf(const battle::UnitFilter & predicate) const
{
	battle::Units result;

	for(const auto & unit : units)
	{
		if(predicate(unit.get()))
			result.push_back(unit.get());
	}

	return result;
}
