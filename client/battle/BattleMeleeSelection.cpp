/*
 * BattleMeleeSelection.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "../StdInc.h"

#include "BattleMeleeSelection.h"

bool BattleMeleeSelection::refresh(PossiblePlayerBattleAction::Actions newAction, BattleHex newTarget,
	uint32_t newTargetUnitId, const std::vector<Candidate> & newCandidates)
{
	if(!isMeleeAction(newAction) || !newTarget.isValid())
	{
		clear();
		return false;
	}

	std::vector<Candidate> normalizedCandidates;
	for(const auto & candidate : newCandidates)
	{
		if(!candidate.attackFrom.isValid() || candidate.direction == BattleHex::NONE)
			continue;

		const auto duplicate = std::find_if(normalizedCandidates.begin(), normalizedCandidates.end(), [&candidate](const Candidate & existing)
		{
			return existing.attackFrom == candidate.attackFrom;
		});
		if(duplicate == normalizedCandidates.end())
			normalizedCandidates.push_back(candidate);
	}

	if(normalizedCandidates.empty())
	{
		clear();
		return false;
	}

	const auto previousCandidate = isValid() ? candidates[selectedIndex] : Candidate{};
	const bool sameSelection = isValid() && action == newAction && target == newTarget && targetUnitId == newTargetUnitId;

	action = newAction;
	target = newTarget;
	targetUnitId = newTargetUnitId;
	candidates = std::move(normalizedCandidates);
	selectedIndex = 0;

	if(sameSelection)
	{
		const auto previous = std::find(candidates.begin(), candidates.end(), previousCandidate);
		if(previous != candidates.end())
			selectedIndex = std::distance(candidates.begin(), previous);
	}

	return true;
}

bool BattleMeleeSelection::cycle(bool forward)
{
	if(candidates.size() <= 1)
		return false;

	if(forward)
		selectedIndex = (selectedIndex + 1) % candidates.size();
	else
		selectedIndex = (selectedIndex + candidates.size() - 1) % candidates.size();

	return true;
}

bool BattleMeleeSelection::isValid() const
{
	return isMeleeAction(action) && target.isValid() && selectedIndex < candidates.size();
}

bool BattleMeleeSelection::hasAlternativeCandidates() const
{
	return isValid() && candidates.size() > 1;
}

PossiblePlayerBattleAction::Actions BattleMeleeSelection::getAction() const
{
	return action;
}

BattleHex BattleMeleeSelection::getTarget() const
{
	return target;
}

std::optional<uint32_t> BattleMeleeSelection::getTargetUnitId() const
{
	return targetUnitId;
}

BattleMeleeSelection::Candidate BattleMeleeSelection::getCandidate() const
{
	return isValid() ? candidates[selectedIndex] : Candidate{};
}

bool BattleMeleeSelection::returnsAfterAttack() const
{
	return isValid() && action == PossiblePlayerBattleAction::ATTACK_AND_RETURN;
}

void BattleMeleeSelection::clear()
{
	action = PossiblePlayerBattleAction::INVALID;
	target = BattleHex::INVALID;
	targetUnitId.reset();
	candidates.clear();
	selectedIndex = 0;
}

bool BattleMeleeSelection::isMeleeAction(PossiblePlayerBattleAction::Actions action)
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
