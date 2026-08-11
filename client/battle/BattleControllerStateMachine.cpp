/*
 * BattleControllerStateMachine.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../StdInc.h"

#include "BattleControllerStateMachine.h"

BattleControllerStateMachine::State BattleControllerStateMachine::top() const
{
	return stack.back();
}

size_t BattleControllerStateMachine::depth() const
{
	return stack.size();
}

bool BattleControllerStateMachine::canEnter(State from, State to)
{
	switch(from)
	{
		case State::BROWSE:
			// movement preview starts directly from navigation; the action
			// layer hosts melee and target selection
			return to == State::ACTION || to == State::PREVIEW;
		case State::ACTION:
			return to == State::TARGET || to == State::ATTACK_DIRECTION;
		case State::TARGET:
			return to == State::PREVIEW;
		case State::ATTACK_DIRECTION:
			// the direction choice itself is the visible attack preview
			return to == State::COMMIT;
		case State::PREVIEW:
			return to == State::COMMIT;
		case State::COMMIT:
			return false;
	}
	return false;
}

bool BattleControllerStateMachine::enter(State state)
{
	if(!canEnter(top(), state))
		return false;

	stack.push_back(state);
	return true;
}

BattleControllerStateMachine::CancelDecision BattleControllerStateMachine::decideCancel(bool controllerMode, bool canPopLayer, bool castingSpell)
{
	if(controllerMode && canPopLayer)
		return CancelDecision::POP_LAYER;

	if(castingSpell || !controllerMode)
		return CancelDecision::CANCEL_SPELL;

	return CancelDecision::OPEN_PARENT_LAYER;
}

bool BattleControllerStateMachine::cancel()
{
	if(stack.size() <= 1)
		return false;

	stack.pop_back();
	return true;
}

void BattleControllerStateMachine::reset()
{
	stack = {State::BROWSE};
}
