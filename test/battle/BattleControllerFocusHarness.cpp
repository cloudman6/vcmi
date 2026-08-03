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

BattleControllerFocusHarness::BattleControllerFocusHarness(Sink & sink, BattleActionLifecycle & lifecycle)
	: sink(sink)
{
	lifecycle.subscribe([this](BattleActionLifecycle::Stage stage, const BattleAction & action)
	{
		onLifecycleEvent(stage, action);
	});
}

bool BattleControllerFocusHarness::setFocus(BattleHex hex)
{
	direction.reset();
	focused = hex;

	if(!focused.isValid())
		return false;

	sink.preview(selection());
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
	sink.preview(selection());
	return true;
}

bool BattleControllerFocusHarness::confirm()
{
	if(!focused.isValid())
		return false;

	sink.submit(selection());
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

BattleControllerFocusHarness::Selection BattleControllerFocusHarness::selection() const
{
	return {focused, direction};
}

void BattleControllerFocusHarness::onLifecycleEvent(BattleActionLifecycle::Stage stage, const BattleAction & action)
{
	if(stage != BattleActionLifecycle::Stage::ACTION_ENDED)
		return;

	if(action.actionType != EActionType::WAIT && action.actionType != EActionType::DEFEND && action.actionType != EActionType::HERO_SPELL)
		return;

	direction.reset();

	if(focused.isValid())
		sink.preview(selection());
}

bool BattleControllerFocusHarness::isHexDirection(BattleHex::EDir direction)
{
	return direction >= BattleHex::EDir::TOP_LEFT && direction <= BattleHex::EDir::LEFT;
}

BattleActionsControllerFocusSink::BattleActionsControllerFocusSink(BattleActionsController & actions)
	: actions(actions)
{
}

void BattleActionsControllerFocusSink::preview(const BattleControllerFocusHarness::Selection & selection)
{
	actions.onHexHovered(selection.targetHex);
}

void BattleActionsControllerFocusSink::submit(const BattleControllerFocusHarness::Selection & selection)
{
	actions.onHexLeftClicked(selection.targetHex);
}
