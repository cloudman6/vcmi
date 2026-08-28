/*
 * BattleMeleeSelection.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/battle/BattleHex.h"
#include "../../lib/battle/PossiblePlayerBattleAction.h"

/// Exact controller selection for a melee action. Battle-rule evaluation stays
/// outside this class; refresh accepts only the legal candidates produced by it.
class BattleMeleeSelection
{
public:
	struct Candidate
	{
		BattleHex attackFrom = BattleHex::INVALID;
		BattleHex::EDir direction = BattleHex::NONE;

		bool operator==(const Candidate & other) const = default;
	};

	bool refresh(PossiblePlayerBattleAction::Actions action, BattleHex target, uint32_t targetUnitId,
		const std::vector<Candidate> & candidates);
	bool cycle(bool forward);

	bool isValid() const;
	bool hasAlternativeCandidates() const;
	PossiblePlayerBattleAction::Actions getAction() const;
	BattleHex getTarget() const;
	std::optional<uint32_t> getTargetUnitId() const;
	Candidate getCandidate() const;
	bool returnsAfterAttack() const;

	void clear();

private:
	static bool isMeleeAction(PossiblePlayerBattleAction::Actions action);

	PossiblePlayerBattleAction::Actions action = PossiblePlayerBattleAction::INVALID;
	BattleHex target = BattleHex::INVALID;
	std::optional<uint32_t> targetUnitId;
	std::vector<Candidate> candidates;
	std::size_t selectedIndex = 0;
};
