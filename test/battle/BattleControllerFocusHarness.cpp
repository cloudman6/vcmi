/*
 * BattleControllerFocusHarness.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "BattleControllerFocusHarness.h"

#include "../../client/battle/BattleActionsController.h"

BattleControllerFocusHarness::BattleControllerFocusHarness(Sink & sink)
	: sink(sink)
{
}

bool BattleControllerFocusHarness::setFocus(BattleHex hex)
{
	direction.reset();
	focused = hex;

	if(!focused.isValid())
		return false;

	sink.preview(focused);
	return true;
}

bool BattleControllerFocusHarness::move(BattleHex::EDir direction)
{
	if(!focused.isValid() || !isHexDirection(direction))
		return false;

	return setFocus(focused.cloneInDirection(direction, false));
}

bool BattleControllerFocusHarness::selectAttackDirection(BattleHex::EDir direction)
{
	if(!focused.isValid() || !isHexDirection(direction))
		return false;

	this->direction = direction;
	return true;
}

void BattleControllerFocusHarness::refreshAfterBattleReply()
{
	direction.reset();

	if(focused.isValid())
		sink.preview(focused);
}

bool BattleControllerFocusHarness::confirm()
{
	if(!focused.isValid())
		return false;

	sink.submit(focused);
	return true;
}

const BattleHex & BattleControllerFocusHarness::focusedHex() const
{
	return focused;
}

const std::optional<BattleHex::EDir> & BattleControllerFocusHarness::attackDirection() const
{
	return direction;
}

bool BattleControllerFocusHarness::isHexDirection(BattleHex::EDir direction)
{
	return direction >= BattleHex::EDir::TOP_LEFT && direction <= BattleHex::EDir::LEFT;
}

BattleActionsControllerFocusSink::BattleActionsControllerFocusSink(BattleActionsController & actions)
	: actions(actions)
{
}

void BattleActionsControllerFocusSink::preview(const BattleHex & hex)
{
	actions.onHexHovered(hex);
}

void BattleActionsControllerFocusSink::submit(const BattleHex & hex)
{
	actions.onHexLeftClicked(hex);
}
