/*
 * BattleControllerActionRadialState.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

enum class BattleControllerActionRadialAction
{
	WAIT,
	DEFEND,
	AUTOCOMBAT
};

struct BattleControllerActionRadialEntry
{
	BattleControllerActionRadialAction action;
	bool enabled;
	size_t slot;
};

/// Tracks a Battle-local controller radial from selection through a matching confirm release.
class BattleControllerActionRadialState
{
	bool openState = false;
	std::vector<BattleControllerActionRadialEntry> entries;
	std::optional<BattleControllerActionRadialAction> selected;
	std::optional<BattleControllerActionRadialAction> pendingConfirm;

	const BattleControllerActionRadialEntry * findAction(
		BattleControllerActionRadialAction action) const;
	void close();

public:
	void open(const std::vector<BattleControllerActionRadialEntry> & currentEntries);
	bool selectDirection(double axisX, double axisY);
	bool pressConfirm();
	std::optional<BattleControllerActionRadialAction> releaseConfirm(
		const std::vector<BattleControllerActionRadialEntry> & currentEntries);
	void cancelConfirm();
	bool back();
	void reset();

	bool isOpen() const;
	std::optional<BattleControllerActionRadialAction> selectedAction() const;
};
