/*
 * BattleControllerStateMachine.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include <vector>

/// Controller interaction states for battle as a stack: Browse, Action,
/// Target, AttackDirection, Preview and Commit. Pure state machine with no
/// rendering, input or battle legality - consumers decide when transitions
/// are meaningful and validate actions through BattleActionsController.
class BattleControllerStateMachine
{
public:
	enum class State
	{
		BROWSE,
		ACTION,
		TARGET,
		ATTACK_DIRECTION,
		PREVIEW,
		COMMIT
	};

	/// Outcome of a cancel request, decided without mutating the stack.
	enum class CancelDecision
	{
		POP_LAYER,       ///< one interaction layer is popped, stay in battle
		CANCEL_SPELL,    ///< run the pre-existing spell cancel path
		OPEN_PARENT_LAYER ///< idle in controller mode, return to parent layer
	};

	State top() const;
	size_t depth() const;

	/// Pushes the state when the transition from the current top is allowed
	/// by the contract, otherwise keeps the stack unchanged.
	bool enter(State state);

	/// Cancel contract: pops one layer at a time. Returns false when Browse
	/// is the only remaining layer; the caller then returns to the parent
	/// layer (for example the battle options window).
	bool cancel();

	/// Drops every layer and returns to Browse (commit finished, battle
	/// phase change or focus recovery).
	void reset();

	static bool canEnter(State from, State to);

	/// Pure cancel contract: controller mode pops a layer first when one is
	/// left; otherwise the pre-existing GLOBAL_CANCEL spell cancel path runs
	/// in every input mode; only an idle controller-mode cancel returns to
	/// the parent layer.
	static CancelDecision decideCancel(bool controllerMode, bool canPopLayer, bool castingSpell);

private:
	std::vector<State> stack = {State::BROWSE};
};
