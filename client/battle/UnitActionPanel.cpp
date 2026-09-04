/*
 * UnitActionPanel.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "UnitActionPanel.h"

#include "BattleInterface.h"
#include "BattleActionsController.h"

#include "../GameEngine.h"
#include "../eventsSDL/InputHandler.h"
#include "../gui/WindowHandler.h"
#include "../widgets/Buttons.h"
#include "../widgets/GraphicalPrimitiveCanvas.h"
#include "../widgets/Images.h"
#include "../widgets/TextControls.h"
#include "../windows/CSpellWindow.h"

#include "../../lib/CConfigHandler.h"
#include "../../lib/GameLibrary.h"
#include "../../lib/battle/CPlayerBattleCallback.h"
#include "../../lib/json/JsonUtils.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/spells/CSpell.h"
#include "../GameInstance.h"

UnitActionPanel::UnitActionPanel(BattleInterface & owner)
	: CIntObject(0)
	, owner(owner)
{
	OBJECT_CONSTRUCTION;

	addUsedEvents(LCLICK | SHOW_POPUP | MOVE);

	pos = Rect(0, 0, 52, 600);
	background = std::make_shared<CFilledTexture>(ImagePath::builtin("DIBOXBCK"), pos);
	rect = std::make_shared<TransparentFilledRectangle>(Rect(0, 0, pos.w + 1, pos.h + 1), ColorRGBA(0, 0, 0, 0), ColorRGBA(241, 216, 120, 255));
}

void UnitActionPanel::restoreAllActions()
{
	owner.actionsController->resetCurrentStackPossibleActions();
}

void UnitActionPanel::setActions(int buttonIndex, const std::vector<PossiblePlayerBattleAction> & filteredActions)
{
	for (const auto & button : buttons)
		if (button != buttons.at(buttonIndex))
			button->setSelectedSilent(false);

	owner.actionsController->setPriorityActions(filteredActions);
	if (filteredActions.front().spellcast())
		owner.actionsController->enterCreatureCastingMode();
	owner.actionsController->setPriorityActions(filteredActions);
}

void UnitActionPanel::addActionGroup(const BattleUnitActionGroup & group)
{
	int index = buttons.size();
	const auto & callback = [this, actions = group.actions, index](bool isSelected){ if (isSelected) setActions(index, actions); else restoreAllActions(); };

	MetaString tooltip;
	tooltip.appendTextID(group.descriptionTextID);
	if(group.spell)
		tooltip.replaceName(*group.spell);

	const auto hoverText = tooltip.toString(&GAME->translator());
	const auto description = group.spell ? group.spell->toSpell()->getDescriptionTranslated(0) : std::string{};
	auto button = std::make_shared<CToggleButton>(Point(2, 7 + 50 * index), AnimationPath::builtin("battleUnitAction"), CButton::tooltip(hoverText, description), callback);
	if(!group.iconAnimation.empty())
		button->setOverlay(std::make_shared<CAnimImage>(group.iconAnimation, group.iconFrame));
	else
		button->setOverlay(std::make_shared<CPicture>(group.iconImage));
	button->setHighlightedBorderColor(Colors::WHITE);
	if(!group.spell)
		button->setAllowDeselection(true);
	buttons.push_back(button);
}

std::vector<BattleUnitActionGroup> UnitActionPanel::buildActionGroups(const std::vector<PossiblePlayerBattleAction> & actions)
{
	static const std::vector actionsMove = { PossiblePlayerBattleAction::MOVE_STACK };
	static const std::vector actionsInfo = { PossiblePlayerBattleAction::CREATURE_INFO, PossiblePlayerBattleAction::HERO_INFO };
	static const std::vector actionsShoot = { PossiblePlayerBattleAction::SHOOT };
	static const std::vector actionsGenie = { PossiblePlayerBattleAction::RANDOM_GENIE_SPELL };
	static const std::vector actionsAttack = { PossiblePlayerBattleAction::ATTACK, PossiblePlayerBattleAction::WALK_AND_ATTACK };
	static const std::vector actionsReturn = { PossiblePlayerBattleAction::ATTACK_AND_RETURN };
	static const std::vector actionsAttackLongWeapon = { PossiblePlayerBattleAction::LONG_WEAPON_ATTACK };

	std::vector<BattleUnitActionGroup> result;
	const auto appendActions = [&actions, &result](const std::vector<PossiblePlayerBattleAction::Actions> & filter, const std::string & icon, const std::string & descriptionTextID)
	{
		std::vector<PossiblePlayerBattleAction> filteredActions;
		for(const auto & action : actions)
			if(vstd::contains(filter, action.get()))
				filteredActions.push_back(action);
		if(!filteredActions.empty())
			result.push_back({std::move(filteredActions), ImagePath::builtin(icon), {}, 0, descriptionTextID, std::nullopt});
	};

	appendActions(actionsMove, "battle/actionMove", "vcmi.battle.action.move");
	appendActions(actionsReturn, "battle/actionReturn", "vcmi.battle.action.return");
	appendActions(actionsAttack, "battle/actionAttack", "vcmi.battle.action.attack");
	appendActions(actionsShoot, "battle/actionShoot", "vcmi.battle.action.shoot");
	appendActions(actionsGenie, "battle/actionGenie", "vcmi.battle.action.genie");
	appendActions(actionsAttackLongWeapon, "battle/actionLongWeapon", "vcmi.battle.action.attackLongWeapon");

	std::vector<SpellID> spells;
	for(const auto & action : actions)
		if(action.spellcast() && !vstd::contains(spells, action.spell()))
			spells.push_back(action.spell());

	for(const auto & spell : spells)
	{
		std::vector<PossiblePlayerBattleAction> spellActions;
		for(const auto & action : actions)
			if(action.spellcast() && action.spell() == spell)
				spellActions.push_back(action);
		result.push_back({std::move(spellActions), {}, AnimationPath::builtin("spellint"), spell.getNum() + 1, "core.genrltxt.26", spell});
	}

	// Not really a unit action, so place it at the end.
	appendActions(actionsInfo, "battle/actionInfo", "vcmi.battle.action.info");
	return result;
}

void UnitActionPanel::setPossibleActions(const std::vector<PossiblePlayerBattleAction> & newActions)
{
	OBJECT_CONSTRUCTION;

	buttons.clear();
	groups = buildActionGroups(newActions);
	for(const auto & group : groups)
		addActionGroup(group);

	redraw();
}

const std::vector<BattleUnitActionGroup> & UnitActionPanel::getActionGroups() const
{
	return groups;
}

bool UnitActionPanel::selectActionGroup(const std::vector<PossiblePlayerBattleAction> & actions)
{
	const auto group = std::ranges::find(groups, actions, &BattleUnitActionGroup::actions);
	if(group == groups.end())
		return false;
	buttons.at(std::distance(groups.begin(), group))->setSelected(true);
	return true;
}

void UnitActionPanel::show(Canvas & to)
{
	showAll(to);
	CIntObject::show(to);
}
