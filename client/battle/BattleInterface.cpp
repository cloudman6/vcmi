/*
 * BattleInterface.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "BattleInterface.h"

#include "BattleActionsController.h"
#include "BattleAnimationClasses.h"
#include "BattleConsole.h"
#include "BattleEffectsController.h"
#include "BattleFieldController.h"
#include "BattleHero.h"
#include "BattleObstacleController.h"
#include "BattleProjectileController.h"
#include "BattleRenderer.h"
#include "BattleResultWindow.h"
#include "BattleSiegeController.h"
#include "BattleStacksController.h"
#include "BattleWindow.h"
#include "CreatureAnimation.h"

#ifdef VCMI_CONTROLLER_E2E
#include "BattleControllerActionRadial.h"
#include "../controllerE2E/ControllerE2EExecutor.h"
#include "../controllerE2E/ControllerE2EProbes.h"
#include "../windows/CSpellWindow.h"
#endif

#include "../CPlayerInterface.h"
#include "../GameEngine.h"
#include "../CServerHandler.h"
#include "../GameInstance.h"
#include "../eventsSDL/InputHandler.h"
#include "../gui/Shortcut.h"
#include "../adventureMap/AdventureMapInterface.h"
#include "../gui/CursorHandler.h"
#include "../gui/WindowHandler.h"
#include "../media/IMusicPlayer.h"
#include "../media/ISoundPlayer.h"
#include "../render/Canvas.h"
#include "../windows/CTutorialWindow.h"
#include "../windows/CCreatureWindow.h"

#include "../../lib/BattleFieldHandler.h"
#include "../../lib/CConfigHandler.h"
#include "../../lib/CStack.h"
#include "../../lib/CThreadHelper.h"
#include "../../lib/GameLibrary.h"
#include "../../lib/TerrainHandler.h"
#include "../../lib/UnlockGuard.h"
#include "../../lib/battle/CPlayerBattleCallback.h"
#include "../../lib/callback/CCallback.h"
#include "../../lib/gameState/InfoAboutArmy.h"
#include "../../lib/mapObjects/CGTownInstance.h"
#include "../../lib/networkPacks/PacksForClientBattle.h"
#include "../../lib/spells/CSpell.h"
#include "../../lib/texts/CGeneralTextHandler.h"

namespace
{
class ControllerInspectStackWindow final : public CStackWindow
{
public:
	explicit ControllerInspectStackWindow(const CStack * stack)
		: CStackWindow(stack, false)
	{
	}

	bool captureThisKey(EShortcut key) override
	{
		return ENGINE->input().getCurrentInputMode() == InputMode::CONTROLLER
			&& (key == EShortcut::GLOBAL_ACCEPT || key == EShortcut::GLOBAL_CANCEL);
	}

	void keyPressed(EShortcut key) override
	{
		if(key == EShortcut::GLOBAL_ACCEPT)
			close();
		else
			CStackWindow::keyPressed(key);
	}
};

class ControllerHoldStackWindow final : public CStackWindow
{
public:
	explicit ControllerHoldStackWindow(const CStack * stack)
		: CStackWindow(stack, true)
	{
	}

	bool captureThisKey(EShortcut key) override
	{
		return ENGINE->input().getCurrentInputMode() == InputMode::CONTROLLER
			&& key == EShortcut::GLOBAL_CANCEL;
	}

	void keyReleased(EShortcut key) override
	{
		if(key == EShortcut::GLOBAL_CANCEL)
			close();
	}
};
}

BattleInterface::BattleInterface(const BattleID & battleID, const CCreatureSet *army1, const CCreatureSet *army2,
		const CGHeroInstance *hero1, const CGHeroInstance *hero2,
		std::shared_ptr<CPlayerInterface> att,
		std::shared_ptr<CPlayerInterface> defen,
		std::shared_ptr<CPlayerInterface> spectatorInt)
	: attackingHeroInstance(hero1)
	, defendingHeroInstance(hero2)
	, attackerInt(att)
	, defenderInt(defen)
	, curInt(att)
	, battleID(battleID)
	, battleOpeningDelayActive(true)
	, round(0)
	, focusNavigation(std::make_unique<BattleFocusNavigation>(focusModel))
	, unitNavigation(std::make_unique<BattleUnitNavigation>(focusModel, [this]
	{
		return getControllerUnitCandidates();
	}))
{
	if(spectatorInt)
	{
		curInt = spectatorInt;
	}
	else if(!curInt)
	{
		//May happen when we are defending during network MP game -> attacker interface is just not present
		curInt = defenderInt;
	}

	//hot-seat -> check tactics for both players (defender may be local human)
	if(attackerInt && attackerInt->cb->getBattle(getBattleID())->battleGetTacticDist())
		tacticianInterface = attackerInt;
	else if(defenderInt && defenderInt->cb->getBattle(getBattleID())->battleGetTacticDist())
		tacticianInterface = defenderInt;

	//initializing armies
	this->army1 = army1;
	this->army2 = army2;

	const CGTownInstance *town = getBattle()->battleGetDefendedTown();
	if(town && town->fortificationsLevel().wallsHealth > 0)
		siegeController.reset(new BattleSiegeController(*this, town));

	windowObject = std::make_shared<BattleWindow>(*this);
	projectilesController.reset(new BattleProjectileController(*this));
	stacksController.reset( new BattleStacksController(*this));
	actionsController.reset( new BattleActionsController(*this));
	effectsController.reset(new BattleEffectsController(*this));
	obstacleController.reset(new BattleObstacleController(*this));

	adventureInt->onAudioPaused();
	ongoingAnimationsState.setBusy();

	ENGINE->windows().pushWindow(windowObject);
	windowObject->blockUI(true);
	windowObject->updateQueue();

	playIntroSoundAndUnlockInterface();

#ifdef VCMI_CONTROLLER_E2E
	registerControllerE2EProbe();
#endif
}

bool BattleInterface::isInTacticsMode()
{
	return tacticianInterface && tacticianInterface->cb->getBattle(getBattleID())->battleGetTacticDist() > 0;
}

void BattleInterface::playIntroSoundAndUnlockInterface()
{
	auto onIntroPlayed = [this]()
	{
		// Make sure that battle have not ended while intro was playing AND that a different one has not started
		if(GAME->interface()->battleInt.get() == this)
			onIntroSoundPlayed();
	};

	auto bfieldType = getBattle()->battleGetBattlefieldType();
	const auto & battlefieldSound = bfieldType.getInfo()->musicFilename;

	std::vector<soundBase::soundID> battleIntroSounds =
	{
		soundBase::battle00, soundBase::battle01,
		soundBase::battle02, soundBase::battle03, soundBase::battle04,
		soundBase::battle05, soundBase::battle06, soundBase::battle07
	};

	int battleIntroSoundChannel = -1;

	if (!battlefieldSound.empty())
		battleIntroSoundChannel = ENGINE->sound().playSound(battlefieldSound);
	else
		battleIntroSoundChannel = ENGINE->sound().playSoundFromSet(battleIntroSounds);

	if (battleIntroSoundChannel != -1)
	{
		ENGINE->sound().setCallback(battleIntroSoundChannel, onIntroPlayed);

		// a replay is watched, not played - the intro is always skipped there
		if (settings["gameTweaks"]["skipBattleIntroMusic"].Bool() || GAME->server().isReplayActive())
			openingEnd();
	}
	else // failed to play sound
	{
		onIntroSoundPlayed();
	}
}

bool BattleInterface::openingPlaying() const
{
	return battleOpeningDelayActive;
}

void BattleInterface::onIntroSoundPlayed()
{
	if (openingPlaying())
		openingEnd();

	auto bfieldType = getBattle()->battleGetBattlefieldType();
	const auto & battlefieldMusic = bfieldType.getInfo()->musicFilename;

	if (!battlefieldMusic.empty())
		ENGINE->music().playMusic(battlefieldMusic, true, true);
	else
		ENGINE->music().playMusicFromSet("battle", true, true);
}

void BattleInterface::openingEnd()
{
	assert(openingPlaying());
	if (!openingPlaying())
		return;

	onAnimationsFinished();
	if(isInTacticsMode())
	{
		// h3 tactics phase tutorial
		if(!persistentStorage["gui"]["tacticsPhaseHintShown"].Bool())
		{
			curInt->showInfoDialog(LIBRARY->generaltexth->translate("core.genrltxt.372"));
			Settings s = persistentStorage.write["gui"]["tacticsPhaseHintShown"];
			s->Bool() = true;
		}
		tacticNextStack(nullptr);
	}
	activateStack();
	battleOpeningDelayActive = false;

	CTutorialWindow::openWindowFirstTime(TutorialMode::TOUCH_BATTLE);
}

BattleInterface::~BattleInterface()
{
	CPlayerInterface::battleInt = nullptr;

#ifdef VCMI_CONTROLLER_E2E
	ControllerE2E::ProbeRegistry::instance().unregisterProbe("battle");
#endif

	if (adventureInt)
		adventureInt->onAudioResumed();

	awaitingEvents.clear();
	onAnimationsFinished();
}

void BattleInterface::redrawBattlefield()
{
	fieldController->redrawBackgroundWithHexes();
	ENGINE->windows().totalRedraw();
}

void BattleInterface::stackReset(const CStack * stack)
{
	stacksController->stackReset(stack);
	if(isControllerNativeMode())
		syncControllerFocusPresentation();
}

void BattleInterface::stackAdded(const CStack * stack)
{
	stacksController->stackAdded(stack, false);
	if(isControllerNativeMode())
		syncControllerFocusPresentation();
}

void BattleInterface::stackRemoved(uint32_t stackID)
{
	stacksController->stackRemoved(stackID);
	if(controllerInspectUnitId && *controllerInspectUnitId == stackID)
	{
		if(const auto inspectWindow = controllerInspectWindow.lock();
			inspectWindow && ENGINE->windows().isTopWindow(inspectWindow))
			ENGINE->windows().popWindow(inspectWindow);
		controllerInspectUnitId.reset();
		controllerInspectWindow.reset();
		controllerHoldInspectWindow.reset();
	}
	fieldController->redrawBackgroundWithHexes();
	windowObject->updateQueue();
	if(isControllerNativeMode())
		syncControllerFocusPresentation();
}

void BattleInterface::stackActivated(const CStack *stack)
{
	stacksController->stackActivated(stack);
}

void BattleInterface::stackMoved(const CStack *stack, const BattleHexArray & destHex, int distance, bool teleport)
{
	if (teleport)
		stacksController->stackTeleported(stack, destHex, distance);
	else
		stacksController->stackMoved(stack, destHex, distance);
	if(isControllerNativeMode())
		syncControllerFocusPresentation();
}

void BattleInterface::stacksAreAttacked(std::vector<StackAttackedInfo> attackedInfos)
{
	stacksController->stacksAreAttacked(attackedInfos);

	BattleSideArray<int> killedBySide{0,0};

	for(const StackAttackedInfo & attackedInfo : attackedInfos)
	{
		BattleSide side = attackedInfo.defender->unitSide();
		killedBySide.at(side) += attackedInfo.amountKilled;
	}

	for(BattleSide side : { BattleSide::ATTACKER, BattleSide::DEFENDER })
	{
		if(killedBySide.at(side) > killedBySide.at(getBattle()->otherSide(side)))
			setHeroAnimation(side, EHeroAnimType::DEFEAT);
		else if(killedBySide.at(side) < killedBySide.at(getBattle()->otherSide(side)))
			setHeroAnimation(side, EHeroAnimType::VICTORY);
	}
}

void BattleInterface::stackAttacking( const StackAttackInfo & attackInfo )
{
	stacksController->stackAttacking(attackInfo);
}

void BattleInterface::newRoundFirst()
{
	waitForAnimations();
}

void BattleInterface::newRound()
{
	console->addText(LIBRARY->generaltexth->allTexts[412]);
	round++;
}

void BattleInterface::giveCommand(EActionType action, const BattleHex & tile, SpellID spell)
{
	std::vector<BattleHex> tiles = {tile};
	giveCommand(action, tiles, spell);
}

void BattleInterface::giveCommand(EActionType action, const std::vector<BattleHex> & tiles,  SpellID spell)
{
	const CStack * actor = nullptr;
	if(action != EActionType::HERO_SPELL && action != EActionType::RETREAT && action != EActionType::SURRENDER)
	{
		actor = stacksController->getActiveStack();
	}

	auto side = getBattle()->playerToSide(curInt->playerID);
	if(side == BattleSide::NONE)
	{
		logGlobal->error("Player %s is not in battle", curInt->playerID.toString());
		return;
	}

	BattleAction ba;
	ba.side = side;
	ba.actionType = action;

	for(auto & tile : tiles)
		ba.aimToHex(tile);
	ba.spell = spell;

	sendCommand(ba, actor);
}

#ifdef VCMI_CONTROLLER_E2E
namespace
{
si64 controllerE2EHexId(const BattleHex & hex)
{
	return hex.isValid() ? hex.getX() + hex.getY() * GameConstants::BFIELD_WIDTH : -1;
}

JsonNode controllerE2EStackNode(const battle::Unit & stack)
{
	JsonNode node;
	node["id"].Integer() = stack.unitId();
	node["type"].String() = stack.unitType() ? stack.unitType()->getJsonKey() : "";
	node["position"].Integer() = controllerE2EHexId(stack.getPosition());
	node["count"].Integer() = stack.getCount();
	node["available_health"].Integer() = stack.getAvailableHealth();
	if(const auto * clientStack = dynamic_cast<const CStack *>(&stack))
		node["shots_available"].Integer() = clientStack->shots.available();
	return node;
}

std::string controllerE2EPrimaryActionName(BattleControllerPrimaryAction action)
{
	switch(action)
	{
		case BattleControllerPrimaryAction::MOVE:
			return "move";
		case BattleControllerPrimaryAction::ATTACK:
			return "attack";
		case BattleControllerPrimaryAction::SHOOT:
			return "shoot";
		case BattleControllerPrimaryAction::INSPECT:
			return "inspect";
		case BattleControllerPrimaryAction::NONE:
			return "none";
	}
	return "none";
}

std::string controllerE2EMeleeActionName(PossiblePlayerBattleAction::Actions action)
{
	switch(action)
	{
		case PossiblePlayerBattleAction::ATTACK:
			return "attack";
		case PossiblePlayerBattleAction::LONG_WEAPON_ATTACK:
			return "long_weapon_attack";
		case PossiblePlayerBattleAction::WALK_AND_ATTACK:
			return "walk_and_attack";
		case PossiblePlayerBattleAction::ATTACK_AND_RETURN:
			return "attack_and_return";
		default:
			return "none";
	}
}

std::string controllerE2EShootDisabledReasonName(const std::string & textKey)
{
	if(textKey == "vcmi.battleWindow.controller.shootDisabled.noAmmo")
		return "no_ammo";
	if(textKey == "vcmi.battleWindow.controller.shootDisabled.blocked")
		return "blocked_by_adjacent_enemy";
	if(textKey == "vcmi.battleWindow.controller.shootDisabled.outOfRange")
		return "out_of_range";
	if(textKey == "vcmi.battleWindow.controller.shootDisabled.prohibited")
		return "rule_prohibited";
	return "none";
}

std::string controllerE2ECommandName(int actionType)
{
	switch(static_cast<EActionType>(actionType))
	{
	case EActionType::WAIT:
		return "wait";
	case EActionType::DEFEND:
		return "defend";
	default:
		return "other";
	}
}
}

void BattleInterface::registerControllerE2EProbe()
{
	ControllerE2E::ProbeRegistry::instance().registerProbe("battle", []()
	{
		JsonNode snapshot;
		auto observed = CPlayerInterface::battleInt;
		if(!observed)
		{
			snapshot["open"].Bool() = false;
			return snapshot;
		}

		snapshot["open"].Bool() = true;
		snapshot["round"].Integer() = observed->round;
		snapshot["tactics_mode"].Bool() = observed->isInTacticsMode();
		snapshot["native_mode"].Bool() = observed->isControllerNativeMode();
		snapshot["cursor_mode"].Bool() = observed->isControllerCursorMode();
		snapshot["focus_hex"].Integer() = controllerE2EHexId(observed->getControllerFocusedHex());
		snapshot["hovered_hex"].Integer() = observed->fieldController
			? controllerE2EHexId(observed->fieldController->getHoveredHex())
			: -1;
		snapshot["focus_cursor_drawn"].Bool() = observed->fieldController
			&& observed->fieldController->wasControllerFocusCursorDrawnForE2E();
		snapshot["action_prompt_drawn"].Bool() = observed->fieldController
			&& observed->fieldController->wasControllerActionPromptDrawnForE2E();
		snapshot["commands_sent"].Integer() = observed->controllerE2ECommandsSent;
		snapshot["melee_cycle_attempts"].Integer() = observed->controllerE2EMeleeCycleAttempts;
		snapshot["melee_cycle_successes"].Integer() = observed->controllerE2EMeleeCycleSuccesses;
		snapshot["melee_cycle_before_sync"].Integer() = observed->controllerE2EMeleeCycleBeforeSync;
		snapshot["melee_cycle_after_sync"].Integer() = observed->controllerE2EMeleeCycleAfterSync;
		snapshot["primary_action"].String() = controllerE2EPrimaryActionName(observed->getControllerPrimaryAction());
		snapshot["shoot_disabled_reason"].String() = controllerE2EShootDisabledReasonName(
			observed->actionsController->controllerE2EGetShootDisabledReasonTextKey(
				observed->getControllerFocusedHex()));
		snapshot["hero_spellcasting_mode"].Bool() = observed->actionsController->heroSpellcastingModeActive();
		snapshot["autocombat_active"].Bool() = observed->curInt->isAutoFightOn;
		snapshot["end_with_autocombat_setting"].Bool() = settings["battle"]["endWithAutocombat"].Bool();
		snapshot["only_one_player_human"].Bool() = observed->windowObject->controllerE2EOnlyOnePlayerHuman();
		snapshot["spellbook_open"].Bool() = ENGINE->windows().topWindow<CSpellWindow>() != nullptr;
		const auto actionRadial = ENGINE->windows().topWindow<BattleControllerActionRadial>();
		if(actionRadial)
			snapshot["action_radial"] = actionRadial->controllerE2ESnapshot();
		else
			snapshot["action_radial"]["open"].Bool() = false;
		snapshot["status_text"].String() = observed->console ? observed->console->controllerE2EHoverText() : "";
		snapshot["hold_inspect_available"].Bool() = observed->hasControllerInspectTarget();
		if(const auto * hero = observed->getBattle()->battleGetMyHero())
		{
			snapshot["hero_has_spellbook"].Bool() = hero->hasSpellbook();
			snapshot["hero_can_cast_spells"].Bool() = observed->getBattle()->battleCanCastSpell(hero, spells::Mode::HERO) == ESpellCastProblem::OK;
		}
		if(observed->controllerMeleeSelection.isValid())
		{
			const auto candidate = observed->controllerMeleeSelection.getCandidate();
			auto & melee = snapshot["melee_selection"];
			melee["valid"].Bool() = true;
			melee["action"].String() = controllerE2EMeleeActionName(observed->controllerMeleeSelection.getAction());
			melee["target_hex"].Integer() = controllerE2EHexId(observed->controllerMeleeSelection.getTarget());
			melee["attack_from"].Integer() = controllerE2EHexId(candidate.attackFrom);
			melee["direction"].Integer() = candidate.direction;
			melee["allows_long_weapon"].Bool() = observed->controllerMeleeSelection.getAction()
				== PossiblePlayerBattleAction::LONG_WEAPON_ATTACK;
			melee["returns_after_attack"].Bool() = observed->controllerMeleeSelection.returnsAfterAttack();
		}
		else
			snapshot["melee_selection"]["valid"].Bool() = false;

		if(observed->controllerE2ELastCommandType >= 0)
		{
			auto & lastCommand = snapshot["last_command"];
			lastCommand["type"].Integer() = observed->controllerE2ELastCommandType;
			lastCommand["name"].String() = controllerE2ECommandName(observed->controllerE2ELastCommandType);
			lastCommand["actor_id"].Integer() = observed->controllerE2ELastCommandActor;
			lastCommand["target_count"].Integer() = observed->controllerE2ELastCommandTargets.size();
			auto & targets = lastCommand["targets"].Vector();
			for(std::size_t index = 0; index < observed->controllerE2ELastCommandTargets.size(); ++index)
			{
				const auto & target = observed->controllerE2ELastCommandTargets[index];
				targets.push_back(JsonNode(controllerE2EHexId(target)));
				lastCommand["target_" + std::to_string(index)].Integer() = controllerE2EHexId(target);
			}
		}
		const auto inspectWindow = observed->controllerInspectWindow.lock();
		snapshot["inspect_open"].Bool() = inspectWindow && ENGINE->windows().isTopWindow(inspectWindow);

		if(observed->fieldController && observed->getControllerFocusedHex().isValid())
		{
			const Point center = observed->fieldController->hexPositionAbsolute(observed->getControllerFocusedHex()).center();
			snapshot["focus_center_x"].Integer() = center.x;
			snapshot["focus_center_y"].Integer() = center.y;
			const auto * focusedStack = observed->getBattle()->battleGetStackByPos(observed->getControllerFocusedHex(), true);
			if(focusedStack)
				snapshot["focused_stack"] = controllerE2EStackNode(*focusedStack);
		}

		const CStack * activeStack = observed->stacksController
			? observed->stacksController->getActiveStack()
			: nullptr;
		if(activeStack)
			snapshot["active_stack"] = controllerE2EStackNode(*activeStack);

		auto & stacks = snapshot["stacks"].Vector();
		auto & stacksById = snapshot["stacks_by_id"].Struct();
		std::vector<uint32_t> aliveUnitIds;
		for(const auto * stack : observed->getBattle()->battleAliveUnits())
			if(stack)
			{
				JsonNode stackNode = controllerE2EStackNode(*stack);
				stackNode["double_wide"].Bool() = stack->doubleWide();
				stackNode["occupied_position"].Integer() = controllerE2EHexId(stack->occupiedHex());
				stacks.push_back(stackNode);
				stacksById["id_" + std::to_string(stack->unitId())] = std::move(stackNode);
				aliveUnitIds.push_back(stack->unitId());
			}
		std::sort(aliveUnitIds.begin(), aliveUnitIds.end());
		auto & aliveIds = snapshot["alive_unit_ids"].Vector();
		for(const auto unitId : aliveUnitIds)
			aliveIds.push_back(JsonNode(static_cast<si64>(unitId)));

		return snapshot;
	});
}
#endif

void BattleInterface::sendCommand(BattleAction command, const CStack * actor)
{
#ifdef VCMI_CONTROLLER_E2E
	++controllerE2ECommandsSent;
#endif
	command.stackNumber = actor ? actor->unitId() : ((command.side == BattleSide::ATTACKER) ? -1 : -2);
#ifdef VCMI_CONTROLLER_E2E
	controllerE2ELastCommandType = static_cast<int>(command.actionType);
	controllerE2ELastCommandActor = command.stackNumber;
	controllerE2ELastCommandTargets.clear();
	for(const auto & target : command.target)
		controllerE2ELastCommandTargets.push_back(target.hexValue);

	auto * executor = ControllerE2E::ControllerE2EExecutor::instance();
	const bool rejectByHarness = executor && executor->consumeNextBattleActionRejection();
#endif

	if(!isInTacticsMode())
	{
		logGlobal->trace("Setting command for %s", (actor ? actor->nodeName() : "hero"));
		stacksController->setActiveStack(nullptr);
#ifdef VCMI_CONTROLLER_E2E
		if(rejectByHarness)
			curInt->controllerE2ERejectBattleAction();
		else
#endif
		curInt->cb->battleMakeUnitAction(battleID, command);
	}
	else
	{
		curInt->cb->battleMakeTacticAction(battleID, command);
		stacksController->setActiveStack(nullptr);
		//next stack will be activated when action ends
	}
	ENGINE->cursor().set(Cursor::Combat::POINTER);
}

const CGHeroInstance * BattleInterface::getActiveHero()
{
	const CStack *attacker = stacksController->getActiveStack();
	if(!attacker)
	{
		return nullptr;
	}

	if(attacker->unitSide() == BattleSide::ATTACKER)
	{
		return attackingHeroInstance;
	}

	return defendingHeroInstance;
}

void BattleInterface::stackIsCatapulting(const CatapultAttack & ca)
{
	if (!siegeController)
		return;

	// a spell-caused catapult (earthquake, attacker == -1) applied while a hero cast is mid-flight must play
	// at the caster's climax (HIT stage); otherwise the wall explosions appear before the hero's cast animation
	if (ca.attacker == -1 && !awaitingEvents.empty())
		addToAnimationStage(EAnimationEvents::HIT, [this, ca](){ siegeController->stackIsCatapulting(ca); });
	else
		siegeController->stackIsCatapulting(ca);
}

void BattleInterface::gateStateChanged(const EGateState state)
{
	if (siegeController)
		siegeController->gateStateChanged(state);
}

void BattleInterface::battleFinished(const BattleResult& br, QueryID queryID)
{
	checkForAnimations();
	stacksController->setActiveStack(nullptr);

	ENGINE->cursor().set(Cursor::Map::POINTER);
	curInt->waitWhileDialog();

	if(settings["session"]["spectate"].Bool() && settings["session"]["spectate-skip-battle-result"].Bool())
	{
		curInt->cb->selectionMade(0, queryID);
		windowObject->close();
		return;
	}

	// a replay has nothing to ask, so the result window is skipped like every other dialog
	if(GAME->server().isReplayActive())
	{
		windowObject->close();
		CPlayerInterface::battleInt.reset();
		return;
	}

	auto wnd = std::make_shared<BattleResultWindow>(br, *(this->curInt));
	wnd->resultCallback = [this, queryID](ui32 selection)
	{
		curInt->cb->selectionMade(selection, queryID);
	};
	ENGINE->windows().pushWindow(wnd);

	curInt->waitWhileDialog(); // Avoid freeze when AI end turn after battle. Check bug #1897
	CPlayerInterface::battleInt.reset();
}

void BattleInterface::spellCast(const BattleSpellCast * sc)
{
	waitForAnimations();

	// Do not deactivate anything in tactics mode
	// This is battlefield setup spells
	if(!isInTacticsMode())
	{
		windowObject->blockUI(true);

		// Disable current active stack duing the cast
		// Store the current activeStack to stackToActivate
		stacksController->deactivateStack();
	}

	ENGINE->cursor().set(Cursor::Combat::BLOCKED);

	const SpellID spellID = sc->spellID;

	if(!spellID.hasValue())
		return;

	const CSpell * spell = spellID.toSpell();
	auto targetedTile = sc->tile;

	const AudioPath & castSoundPath = spell->getCastSound();

	if (!castSoundPath.empty())
	{
		auto group = spell->animationInfo.projectile.empty() ?
					EAnimationEvents::HIT:
					EAnimationEvents::BEFORE_HIT;//FIXME: recheck whether this should be on projectile spawning

		addToAnimationStage(group, [=]() {
			ENGINE->sound().playSound(castSoundPath);
		});
	}

	if ( sc->activeCast )
	{
		const CStack * casterStack = getBattle()->battleGetStackByID(sc->casterStack);

		if(casterStack != nullptr )
		{
			// mass spells (RANGE:X) have no target hex, so there is no direction to turn towards
			if (targetedTile.isValid() && stacksController->shouldRotate(casterStack, casterStack->getPosition(), targetedTile))
			{
				addToAnimationStage(EAnimationEvents::MOVEMENT, [this, casterStack]()
				{
					stacksController->addNewAnim(new ReverseAnimation(*this, casterStack, casterStack->getPosition()));
				});
			}

			addToAnimationStage(EAnimationEvents::BEFORE_HIT, [this, casterStack, targetedTile, spell]()
			{
				stacksController->addNewAnim(new CastAnimation(*this, casterStack, targetedTile, getBattle()->battleGetStackByPos(targetedTile), spell));
				displaySpellCast(spell, casterStack->getPosition());
			});
		}
		else
		{
			auto hero = sc->side == BattleSide::DEFENDER ? defendingHero : attackingHero;
			assert(hero);

			addToAnimationStage(EAnimationEvents::BEFORE_HIT, [this, hero, targetedTile, spell]()
			{
				stacksController->addNewAnim(new HeroCastAnimation(*this, hero, targetedTile, getBattle()->battleGetStackByPos(targetedTile), spell));
			});
		}
	}

	addToAnimationStage(EAnimationEvents::HIT, [this, spell, targetedTile](){
		displaySpellHit(spell, targetedTile);
	});

	const bool usesChainRay = !spell->animationInfo.ray.empty() && !sc->affectedCres.empty();

	//queuing affect animation
	if(usesChainRay)
	{
		// only the primary (first) target gets the full affect; the rest get the trailing spark frames
		const auto & affect = spell->animationInfo.affect;
		SpellAnimationQueue sparks(affect.begin() + (affect.empty() ? 0 : 1), affect.end());

		std::vector<Point> targetPoints;
		for(size_t i = 0; i < sc->affectedCres.size(); ++i)
		{
			auto stack = getBattle()->battleGetStackByID(sc->affectedCres[i], false);
			if(!stack)
				continue;

			BattleHex hex = stack->getPosition();
			if(i == 0)
				addToAnimationStage(EAnimationEvents::HIT, [this, spell, hex](){ displaySpellEffect(spell, hex); });
			else
				addToAnimationStage(EAnimationEvents::HIT, [this, spell, sparks, hex](){ displaySpellAnimationQueue(spell, sparks, hex, false); });

			Point directionOffset(30, 0);
			targetPoints.push_back(stacksController->getStackPositionAtHex(hex, stack) + Point(225, 225) + (stacksController->facingRight(stack) ? -directionOffset : directionOffset));
		}

		const CStack * casterStack = getBattle()->battleGetStackByID(sc->casterStack);
		addToAnimationStage(EAnimationEvents::HIT, [this, casterStack, targetPoints, spell](){
			stacksController->addNewAnim(new ChainLightningAnimation(*this, casterStack, targetPoints, spell));
		});
	}
	else
	{
		size_t affectedIndex = 0;
		for(auto & elem : sc->affectedCres)
		{
			auto stack = getBattle()->battleGetStackByID(elem, false);
			assert(stack);
			if(stack)
			{
				// secondary affected targets (e.g. the sacrificed unit) use a distinct effect if the spell defines one
				bool useSecondary = affectedIndex > 0 && !spell->animationInfo.affectSecondary.empty();
				addToAnimationStage(EAnimationEvents::HIT, [this, stack, spell, useSecondary](){
					if(useSecondary)
						displaySpellAnimationQueue(spell, spell->animationInfo.affectSecondary, stack->getPosition(), false);
					else
						displaySpellEffect(spell, stack->getPosition());
				});
			}
			++affectedIndex;
		}
	}

	for(auto & elem : sc->reflectedCres)
	{
		auto stack = getBattle()->battleGetStackByID(elem, false);
		assert(stack);
		addToAnimationStage(EAnimationEvents::HIT, [this, stack](){
			effectsController->displayEffect(EBattleEffect::MAGIC_MIRROR, stack->getPosition());
		});
	}

	if (!sc->resistedCres.empty())
	{
		addToAnimationStage(EAnimationEvents::HIT, [](){
			ENGINE->sound().playSound(AudioPath::builtin("MAGICRES"));
		});
	}

	for(auto & elem : sc->resistedCres)
	{
		auto stack = getBattle()->battleGetStackByID(elem, false);
		assert(stack);
		addToAnimationStage(EAnimationEvents::HIT, [this, stack](){
			effectsController->displayEffect(EBattleEffect::RESISTANCE, stack->getPosition());
		});
	}

	//mana absorption
	if (sc->manaGained > 0)
	{
		Point leftHero = Point(15, 30);
		Point rightHero = Point(755, 30);
		BattleSide side = sc->side;

		addToAnimationStage(EAnimationEvents::AFTER_HIT, [this, side, leftHero, rightHero](){
			stacksController->addNewAnim(new EffectAnimation(*this, AnimationPath::builtin(side == BattleSide::DEFENDER ? "SP07_A.DEF" : "SP07_B.DEF"), leftHero));
			stacksController->addNewAnim(new EffectAnimation(*this, AnimationPath::builtin(side == BattleSide::DEFENDER ? "SP07_B.DEF" : "SP07_A.DEF"), rightHero));
		});
	}

	// animations will be executed by spell effects
}

void BattleInterface::battleStacksEffectsSet(const SetStackEffect & sse)
{
	if(stacksController->getActiveStack() != nullptr)
		fieldController->redrawBackgroundWithHexes();
}

void BattleInterface::setHeroAnimation(BattleSide side, EHeroAnimType phase)
{
	if(side == BattleSide::ATTACKER)
	{
		if(attackingHero)
			attackingHero->setPhase(phase);
	}
	else
	{
		if(defendingHero)
			defendingHero->setPhase(phase);
	}
}

void BattleInterface::displayBattleLog(const std::vector<MetaString> & battleLog)
{
	for(const auto & line : battleLog)
	{
		std::string formatted = line.toString(&GAME->translator());
		boost::algorithm::trim(formatted);
		appendBattleLog(formatted);
	}
}

void BattleInterface::displaySpellAnimationQueue(const CSpell * spell, const SpellAnimationQueue & q, const BattleHex & destinationTile, bool isHit)
{
	for(const auto & animation : q)
	{
		if(animation.pause > 0)
			stacksController->addNewAnim(new DummyAnimation(*this, animation.pause));

		if (!animation.effectName.empty())
		{
			const CStack * destStack = getBattle()->battleGetStackByPos(destinationTile, false);

			if (destStack)
				stacksController->addNewAnim(new ColorTransformAnimation(*this, destStack, animation.effectName, spell ));
		}

		if(!animation.resourceName.empty())
		{
			int flags = 0;

			if (isHit)
				flags |= EffectAnimation::FORCE_ON_TOP;

			if (animation.verticalPosition == VerticalPosition::BOTTOM)
				flags |= EffectAnimation::ALIGN_TO_BOTTOM;

			if (!destinationTile.isValid())
				flags |= EffectAnimation::SCREEN_FILL;

			if (!destinationTile.isValid())
				stacksController->addNewAnim(new EffectAnimation(*this, animation.resourceName, flags, animation.transparency));
			else
				stacksController->addNewAnim(new EffectAnimation(*this, animation.resourceName, destinationTile, flags, animation.transparency));
		}
	}
}

void BattleInterface::displaySpellCast(const CSpell * spell, const BattleHex & destinationTile)
{
	if(spell)
		displaySpellAnimationQueue(spell, spell->animationInfo.cast, destinationTile, false);
}

void BattleInterface::displaySpellEffect(const CSpell * spell, const BattleHex & destinationTile)
{
	if(spell)
		displaySpellAnimationQueue(spell, spell->animationInfo.affect, destinationTile, false);
}

void BattleInterface::displaySpellHit(const CSpell * spell, const BattleHex & destinationTile)
{
	if(spell)
		displaySpellAnimationQueue(spell, spell->animationInfo.hit, destinationTile, true);
}

CPlayerInterface *BattleInterface::getCurrentPlayerInterface() const
{
	return curInt.get();
}

void BattleInterface::trySetActivePlayer( PlayerColor player )
{
	if ( attackerInt && attackerInt->playerID == player )
		curInt = attackerInt;
	else if ( defenderInt && defenderInt->playerID == player )
		curInt = defenderInt;
}

bool BattleInterface::isControllerNativeMode() const
{
	return ENGINE->input().getCurrentInputMode() == InputMode::CONTROLLER
		&& !controllerInteractionState.isCursorMode();
}

bool BattleInterface::isControllerCursorMode() const
{
	return controllerInteractionState.isCursorMode();
}

bool BattleInterface::acceptsPointerPresentation(PointerEventSource source)
{
	return controllerInteractionState.acceptsPointerPresentation(source);
}

bool BattleInterface::ensureControllerFocus()
{
	if(focusModel.hasFocus())
		return true;

	const auto * activeStack = stacksController->getActiveStack();
	return activeStack != nullptr && focusModel.setFocus(activeStack->getPosition());
}

void BattleInterface::syncControllerFocusPresentation()
{
	if(!isControllerNativeMode() || !focusModel.hasFocus())
		return;

	const auto focus = focusModel.getFocusedHex();
	fieldController->setControllerFocusedHex(focus);
	actionsController->refreshControllerMeleeSelection(controllerMeleeSelection, focus);
	if(controllerMeleeOriginRepeatState.hasPendingRepeat())
		controllerMeleeOriginRepeatState.retainContext(getControllerMeleeOriginRepeatContext());
	fieldController->setControllerAttackDirection(
		controllerMeleeSelection.getTarget(), controllerMeleeSelection.getCandidate().direction);
	actionsController->onHexHovered(focus);
	redrawBattlefield();
}

std::vector<BattleUnitNavigationCandidate> BattleInterface::getControllerUnitCandidates() const
{
	std::vector<BattleUnitNavigationCandidate> result;
	for(const auto * unit : getBattle()->battleAliveUnits())
	{
		const auto headHex = unit->getPosition();
		if(!unit->isValidTarget(false) || !headHex.isValid())
			continue;

		result.push_back({
			unit->unitId(),
			headHex,
			unit->doubleWide() ? unit->occupiedHex() : BattleHex::INVALID
		});
	}
	return result;
}

bool BattleInterface::handleControllerAxis(const ControllerAxisEvent & event)
{
	if(!isControllerNativeMode())
		return false;
	if(stacksController->getActiveStack() == nullptr || !ensureControllerFocus())
	{
		focusNavigation->reset();
		unitNavigation->reset();
		navigationArbiter.reset();
		return false;
	}

	bool consumed = false;
	for(const auto action : event.actions)
	{
		if(action == EShortcut::CONTROLLER_NAVIGATE_X)
		{
			focusNavigation->updateAxis(event.instanceId, BattleFocusNavigation::Axis::HORIZONTAL, event.value);
			navigationArbiter.update(BattleNavigationArbiter::Source::HEX,
				focusNavigation->isActive(), unitNavigation->isActive());
			consumed = true;
		}
		if(action == EShortcut::CONTROLLER_NAVIGATE_Y)
		{
			focusNavigation->updateAxis(event.instanceId, BattleFocusNavigation::Axis::VERTICAL, event.value);
			navigationArbiter.update(BattleNavigationArbiter::Source::HEX,
				focusNavigation->isActive(), unitNavigation->isActive());
			consumed = true;
		}
		if(action == EShortcut::CONTROLLER_UNIT_NAVIGATE_X)
		{
			unitNavigation->updateAxis(event.instanceId, BattleUnitNavigation::Axis::HORIZONTAL, event.value);
			navigationArbiter.update(BattleNavigationArbiter::Source::UNIT,
				unitNavigation->isActive(), focusNavigation->isActive());
			consumed = true;
		}
		if(action == EShortcut::CONTROLLER_UNIT_NAVIGATE_Y)
		{
			unitNavigation->updateAxis(event.instanceId, BattleUnitNavigation::Axis::VERTICAL, event.value);
			navigationArbiter.update(BattleNavigationArbiter::Source::UNIT,
				unitNavigation->isActive(), focusNavigation->isActive());
			consumed = true;
		}
	}
	return consumed;
}

void BattleInterface::updateControllerAxis(uint32_t msPassed)
{
	if(!isControllerNativeMode())
		return;
	if(stacksController->getActiveStack() == nullptr)
	{
		focusNavigation->reset();
		unitNavigation->reset();
		navigationArbiter.reset();
		return;
	}

	if(const auto repeatDirection = controllerMeleeOriginRepeatState.update(msPassed);
		repeatDirection && !cycleControllerMeleeOrigin(*repeatDirection, true))
	{
		controllerMeleeOriginRepeatState.reset();
	}

	bool focusMoved = false;
	switch(navigationArbiter.owner())
	{
	case BattleNavigationArbiter::Source::HEX:
		focusMoved = focusNavigation->update(msPassed);
		break;
	case BattleNavigationArbiter::Source::UNIT:
		focusMoved = unitNavigation->update(msPassed);
		break;
	case BattleNavigationArbiter::Source::NONE:
		break;
	}
	const bool presentationLost = focusModel.hasFocus()
		&& fieldController->getHoveredHex() != focusModel.getFocusedHex();
	if(focusMoved || presentationLost)
		syncControllerFocusPresentation();
}

void BattleInterface::resetControllerAxis()
{
	focusNavigation->reset();
	unitNavigation->reset();
	navigationArbiter.reset();
	controllerActionPressState.reset();
	controllerMeleeOriginRepeatState.reset();
	controllerMeleeSelection.clear();
	if(const auto inspectWindow = controllerHoldInspectWindow.lock();
		inspectWindow && ENGINE->windows().isTopWindow(inspectWindow))
	{
		controllerHoldInspectWindow.reset();
		ENGINE->windows().popWindow(inspectWindow);
	}
}

void BattleInterface::controllerInputModeActivated()
{
	controllerInteractionState.controllerInputActivated();
	controllerActionPressState.reset();
	controllerMeleeOriginRepeatState.reset();
	controllerMeleeSelection.clear();
	if(ensureControllerFocus())
		syncControllerFocusPresentation();
}

void BattleInterface::toggleControllerCursorMode()
{
	if(ENGINE->input().getCurrentInputMode() != InputMode::CONTROLLER)
		return;

	ENGINE->input().resetControllerAxisState();
	if(!controllerInteractionState.isCursorMode())
	{
		controllerInteractionState.enterCursorMode(focusModel.getFocusedHex());
	}
	else
	{
		const auto * activeStack = stacksController->getActiveStack();
		const BattleHex activeHex = activeStack != nullptr ? activeStack->getPosition() : BattleHex::INVALID;
		const BattleHex restoreHex = controllerInteractionState.leaveCursorMode(activeHex);
		if(restoreHex.isValid())
			focusModel.setFocus(restoreHex);
		if(ensureControllerFocus())
			syncControllerFocusPresentation();
	}
	ENGINE->windows().refreshControllerCursorPolicy();
	redrawBattlefield();
}

bool BattleInterface::handleControllerAcceptPressed()
{
	if(!isControllerNativeMode())
		return false;

	const auto action = getControllerPrimaryAction();
	const auto focus = focusModel.getFocusedHex();
	const bool directAction = action == BattleControllerPrimaryAction::MOVE
		|| action == BattleControllerPrimaryAction::ATTACK
		|| action == BattleControllerPrimaryAction::SHOOT
		|| action == BattleControllerPrimaryAction::INSPECT;
	const auto candidate = controllerMeleeSelection.getCandidate();
	if(!directAction || !controllerActionPressState.press(
		action, focus, candidate.attackFrom, candidate.direction,
		controllerMeleeSelection.getAction(), getControllerActionTargetUnitId(action)))
		return false;

	redrawBattlefield();
	return true;
}

bool BattleInterface::handleControllerAcceptReleased()
{
	if(!controllerActionPressState.hasPendingAction())
		return false;

	const auto action = getControllerPrimaryAction();
	const auto focus = focusModel.getFocusedHex();
	const auto candidate = controllerMeleeSelection.getCandidate();
	const auto targetUnitId = getControllerActionTargetUnitId(action);
	const auto releasedAction = controllerActionPressState.release(
		action, focus, candidate.attackFrom, candidate.direction,
		controllerMeleeSelection.getAction(), targetUnitId);
	redrawBattlefield();
	if(!isControllerNativeMode())
		return false;
	if(releasedAction == BattleControllerPrimaryAction::MOVE)
	{
		actionsController->onHexLeftClicked(focus);
		return true;
	}
	if(releasedAction == BattleControllerPrimaryAction::ATTACK)
	{
		const bool submitted = actionsController->realizeControllerMeleeSelection(controllerMeleeSelection);
		if(!submitted)
			syncControllerFocusPresentation();
		return true;
	}
	if(releasedAction == BattleControllerPrimaryAction::SHOOT)
	{
		const bool submitted = actionsController->realizeControllerShoot(focus, targetUnitId);
		if(!submitted)
			syncControllerFocusPresentation();
		return true;
	}
	if(releasedAction != BattleControllerPrimaryAction::INSPECT)
		return false;

	const auto * stack = getBattle()->battleGetStackByPos(focus, true);
	if(stack == nullptr)
		return false;

	auto inspectWindow = std::make_shared<ControllerInspectStackWindow>(stack);
	controllerInspectUnitId = stack->unitId();
	controllerInspectWindow = inspectWindow;
	ENGINE->windows().pushWindow(inspectWindow);
	return true;
}

bool BattleInterface::handleControllerInspectPressed()
{
	if(!canControllerInspectFocusedStack())
		return false;

	const auto * stack = getBattle()->battleGetStackByPos(focusModel.getFocusedHex(), true);
	auto inspectWindow = std::make_shared<ControllerHoldStackWindow>(stack);
	controllerInspectUnitId = stack->unitId();
	controllerInspectWindow = inspectWindow;
	controllerHoldInspectWindow = inspectWindow;
	ENGINE->windows().pushWindow(inspectWindow);
	return true;
}

bool BattleInterface::isControllerAcceptPressed()
{
	const auto candidate = controllerMeleeSelection.getCandidate();
	const auto action = getControllerPrimaryAction();
	return controllerActionPressState.isPressed(
		action, focusModel.getFocusedHex(), candidate.attackFrom, candidate.direction,
		controllerMeleeSelection.getAction(), getControllerActionTargetUnitId(action));
}

bool BattleInterface::hasControllerInspectTarget() const
{
	return canControllerInspectFocusedStack();
}

bool BattleInterface::canControllerInspectFocusedStack() const
{
	if(!isControllerNativeMode() || !focusModel.hasFocus())
		return false;
	if(!stacksController || stacksController->getActiveStack() == nullptr)
		return false;
	if(actionsController->heroSpellcastingModeActive() || actionsController->creatureSpellcastingModeActive())
		return false;
	return getBattle()->battleGetStackByPos(focusModel.getFocusedHex(), true) != nullptr;
}

bool BattleInterface::hasControllerMeleeAlternatives() const
{
	return isControllerNativeMode() && controllerMeleeSelection.hasAlternativeCandidates();
}

bool BattleInterface::handleControllerMeleeOriginPressed(bool forward)
{
	if(!cycleControllerMeleeOrigin(forward, false))
	{
		controllerMeleeOriginRepeatState.reset();
		return false;
	}

	controllerMeleeOriginRepeatState.press(forward, getControllerMeleeOriginRepeatContext());
	return true;
}

bool BattleInterface::handleControllerMeleeOriginReleased(bool forward)
{
	const bool hadPressedDirection = controllerMeleeOriginRepeatState.release(forward);
	return hadPressedDirection || isControllerNativeMode();
}

bool BattleInterface::cycleControllerMeleeOrigin(bool forward, bool repeated)
{
#ifdef VCMI_CONTROLLER_E2E
	++controllerE2EMeleeCycleAttempts;
#endif
	if(!isControllerNativeMode() || !focusModel.hasFocus())
		return false;
	if(repeated && !controllerMeleeOriginRepeatState.retainContext(getControllerMeleeOriginRepeatContext()))
		return false;

	if(!actionsController->refreshControllerMeleeSelection(controllerMeleeSelection, focusModel.getFocusedHex()))
		return false;
	if(repeated && !controllerMeleeOriginRepeatState.retainContext(getControllerMeleeOriginRepeatContext()))
	{
		syncControllerFocusPresentation();
		return false;
	}
	if(!controllerMeleeSelection.cycle(forward))
		return false;
	if(repeated)
		controllerMeleeOriginRepeatState.selectionAdvanced(getControllerMeleeOriginRepeatContext());

#ifdef VCMI_CONTROLLER_E2E
	++controllerE2EMeleeCycleSuccesses;
	controllerE2EMeleeCycleBeforeSync = controllerMeleeSelection.getCandidate().attackFrom.toInt();
#endif

	syncControllerFocusPresentation();
#ifdef VCMI_CONTROLLER_E2E
	controllerE2EMeleeCycleAfterSync = controllerMeleeSelection.getCandidate().attackFrom.toInt();
#endif
	return true;
}

BattleControllerMeleeOriginRepeatContext BattleInterface::getControllerMeleeOriginRepeatContext() const
{
	const auto candidate = controllerMeleeSelection.getCandidate();
	return {
		controllerMeleeSelection.getAction(),
		controllerMeleeSelection.getTarget(),
		controllerMeleeSelection.getTargetUnitId(),
		candidate.attackFrom,
		candidate.direction
	};
}

void BattleInterface::controllerWindowRestored()
{
	if(!controllerInspectUnitId)
		return;

	controllerInspectUnitId.reset();
	controllerInspectWindow.reset();
	controllerHoldInspectWindow.reset();
	if(isControllerNativeMode() && ensureControllerFocus())
		syncControllerFocusPresentation();
}

BattleControllerPrimaryAction BattleInterface::getControllerPrimaryAction()
{
	if(!isControllerNativeMode() || !focusModel.hasFocus())
		return BattleControllerPrimaryAction::NONE;
	return actionsController->getControllerPrimaryAction(focusModel.getFocusedHex());
}

std::optional<uint32_t> BattleInterface::getControllerActionTargetUnitId(
	BattleControllerPrimaryAction action) const
{
	if(action == BattleControllerPrimaryAction::ATTACK)
		return controllerMeleeSelection.getTargetUnitId();
	if(action != BattleControllerPrimaryAction::SHOOT || !focusModel.hasFocus())
		return std::nullopt;

	const auto * target = getBattle()->battleGetStackByPos(focusModel.getFocusedHex(), true);
	return target != nullptr
		? std::optional<uint32_t>(target->unitId())
		: std::nullopt;
}

BattleHex BattleInterface::getControllerFocusedHex() const
{
	return focusModel.getFocusedHex();
}

void BattleInterface::activateStack()
{
	controllerActionPressState.reset();
	controllerMeleeOriginRepeatState.reset();
	controllerMeleeSelection.clear();
	stacksController->activateStack();

	const CStack * s = stacksController->getActiveStack();
	if(!s)
		return;

	windowObject->updateQueue();
	windowObject->blockUI(false);
	fieldController->redrawBackgroundWithHexes();
	actionsController->activateStack();
	if(isControllerNativeMode())
	{
		focusModel.setFocus(s->getPosition());
		syncControllerFocusPresentation();
	}
	else
	{
		ENGINE->fakeMouseMove();
	}
}

bool BattleInterface::makingTurn() const
{
	return stacksController->getActiveStack() != nullptr;
}

BattleID BattleInterface::getBattleID() const
{
	return battleID;
}

std::shared_ptr<CPlayerBattleCallback> BattleInterface::getBattle() const
{
	return curInt->cb->getBattle(battleID);
}

void BattleInterface::endAction(const BattleAction &action)
{
	// deferred spell hit reactions (e.g. chain lightning) are left undriven; start them so the wait below completes them
	if(!awaitingEvents.empty() && !hasAnimations())
		executeStagedAnimations();

	// it is possible that tactics mode ended while opening music is still playing
	waitForAnimations();

	const CStack *stack = getBattle()->battleGetStackByID(action.stackNumber);

	// Activate stack from stackToActivate because this might have been temporary disabled, e.g., during spell cast
	activateStack();

	stacksController->endAction(action);
	windowObject->updateQueue();

	//stack ended movement in tactics phase -> select the next one
	if (isInTacticsMode())
		tacticNextStack(stack);

	//we have activated next stack after sending request that has been just realized -> blockmap due to movement has changed
	if(action.actionType == EActionType::HERO_SPELL)
		fieldController->redrawBackgroundWithHexes();
}

void BattleInterface::appendBattleLog(const std::string & newEntry)
{
	console->addText(newEntry);
}

void BattleInterface::startAction(const BattleAction & action)
{
	if(action.actionType == EActionType::END_TACTIC_PHASE)
	{
		windowObject->tacticPhaseEnded();
		return;
	}

	stacksController->startAction(action);

	if (!action.isUnitAction())
		return;

	assert(getBattle()->battleGetStackByID(action.stackNumber));
	windowObject->updateQueue();
	effectsController->startAction(action);
}

void BattleInterface::actionRejected()
{
	controllerActionPressState.reset();
	controllerMeleeOriginRepeatState.reset();
	controllerMeleeSelection.clear();

	const auto * authoritativeUnit = getBattle()->battleActiveUnit();
	if(!authoritativeUnit || authoritativeUnit->unitOwner() != curInt->playerID)
		return;

	const auto * authoritativeStack = getBattle()->battleGetStackByID(authoritativeUnit->unitId());
	if(authoritativeStack)
		stacksController->stackActivated(authoritativeStack);
}

void BattleInterface::tacticPhaseEnd()
{
	stacksController->setActiveStack(nullptr);

	auto side = tacticianInterface->cb->getBattle(battleID)->playerToSide(tacticianInterface->playerID);
	auto action = BattleAction::makeEndOFTacticPhase(side);

	tacticianInterface->cb->battleMakeTacticAction(battleID, action);
}

static bool immobile(const CStack *s)
{
	return s->getMovementRange() == 0; //should bound stacks be immobile?
}

void BattleInterface::tacticNextStack(const CStack * current)
{
	if (!current)
		current = stacksController->getActiveStack();

	//no switching stacks when the current one is moving
	checkForAnimations();

	TStacks stacksOfMine = tacticianInterface->cb->getBattle(battleID)->battleGetStacks(CPlayerBattleCallback::ONLY_MINE);
	vstd::erase_if (stacksOfMine, &immobile);
	if (stacksOfMine.empty())
	{
		tacticPhaseEnd();
		return;
	}

	auto it = vstd::find(stacksOfMine, current);
	if (it != stacksOfMine.end() && ++it != stacksOfMine.end())
		stackActivated(*it);
	else
		stackActivated(stacksOfMine.front());

}

void BattleInterface::obstaclePlaced(const std::shared_ptr<const CObstacleInstance> & oi)
{
	// if a spell cast is mid-flight, show the obstacle after the caster's animation reaches its climax (HIT stage)
	if(!awaitingEvents.empty())
		addToAnimationStage(EAnimationEvents::HIT, [this, oi](){ obstacleController->obstaclePlaced(oi); });
	else
		obstacleController->obstaclePlaced(oi);
}

void BattleInterface::obstacleRemoved(const ObstacleChanges & obstacle)
{
	obstacleController->obstacleRemoved(obstacle);
}

const CGHeroInstance *BattleInterface::currentHero() const
{
	if (attackingHeroInstance && attackingHeroInstance->tempOwner == curInt->playerID)
		return attackingHeroInstance;

	if (defendingHeroInstance && defendingHeroInstance->tempOwner == curInt->playerID)
		return defendingHeroInstance;

	return nullptr;
}

InfoAboutHero BattleInterface::enemyHero() const
{
	InfoAboutHero ret;
	if (attackingHeroInstance->tempOwner == curInt->playerID)
		curInt->cb->getHeroInfo(defendingHeroInstance, ret);
	else
		curInt->cb->getHeroInfo(attackingHeroInstance, ret);

	return ret;
}

void BattleInterface::requestAutofightingAIToTakeAction()
{
	assert(curInt->isAutoFightOn);

	if(getBattle()->battleIsFinished())
	{
		return; // battle finished with spellcast
	}

	auto tacticsDist = curInt->cb->getBattle(battleID)->battleGetTacticDist();

	if (tacticsDist > 0)
	{
		stacksController->setActiveStack(nullptr);
		std::thread aiThread([localBattleID = battleID, localCurInt = curInt, tacticsDist]()
		{
			setThreadName("autofightingAI");
			localCurInt->autofightingAI->yourTacticPhase(localBattleID, tacticsDist);
		});
		aiThread.detach();
	}
	else
	{
		const CStack* activeStack = stacksController->getActiveStack();

		// If enemy is moving, activeStack can be null
		if (activeStack)
		{
			stacksController->setActiveStack(nullptr);

			// FIXME: unsafe
			// Run task in separate thread to avoid UI lock while AI is making turn (which might take some time)
			// HOWEVER this thread won't atttempt to lock game state, potentially leading to races
			std::thread aiThread([localBattleID = battleID, localCurInt = curInt, activeStack]()
			{
				setThreadName("autofightingAI");
				localCurInt->autofightingAI->activeStack(localBattleID, activeStack);
			});
			aiThread.detach();
		}
	}
}

void BattleInterface::castThisSpell(SpellID spellID)
{
	actionsController->castThisSpell(spellID);
}

void BattleInterface::endNetwork()
{
	ongoingAnimationsState.requestTermination();
}

void BattleInterface::executeStagedAnimations()
{
	EAnimationEvents earliestStage = EAnimationEvents::COUNT;

	for(const auto & event : awaitingEvents)
		earliestStage = std::min(earliestStage, event.event);

	if(earliestStage != EAnimationEvents::COUNT)
		executeAnimationStage(earliestStage);
}

void BattleInterface::executeAnimationStage(EAnimationEvents event)
{
	decltype(awaitingEvents) executingEvents;

	for(auto it = awaitingEvents.begin(); it != awaitingEvents.end();)
	{
		if(it->event == event)
		{
			executingEvents.push_back(*it);
			it = awaitingEvents.erase(it);
		}
		else
			++it;
	}
	for(const auto & event : executingEvents)
		event.action();
}

void BattleInterface::onAnimationsStarted()
{
	ongoingAnimationsState.setBusy();
}

void BattleInterface::onAnimationsFinished()
{
	ongoingAnimationsState.setFree();
}

void BattleInterface::waitForAnimations()
{
	{
		auto unlockInterface = vstd::makeUnlockGuard(ENGINE->interfaceMutex);
		ongoingAnimationsState.waitWhileBusy();
	}

	assert(!hasAnimations());
	assert(awaitingEvents.empty());

	if (!awaitingEvents.empty())
	{
		logGlobal->error("Wait for animations finished but we still have awaiting events!");
		awaitingEvents.clear();
	}
}

bool BattleInterface::hasAnimations()
{
	return ongoingAnimationsState.isBusy();
}

void BattleInterface::checkForAnimations()
{
	assert(!hasAnimations());
	if(hasAnimations())
		logGlobal->error("Unexpected animations state: expected all animations to be over, but some are still ongoing!");

	waitForAnimations();
}

void BattleInterface::addToAnimationStage(EAnimationEvents event, const AwaitingAnimationAction & action)
{
	awaitingEvents.push_back({action, event});
}

bool BattleInterface::hasQueuedStage(EAnimationEvents event) const
{
	for(const auto & e : awaitingEvents)
		if(e.event == event)
			return true;
	return false;
}

void BattleInterface::setBattleQueueVisibility(bool visible)
{
	windowObject->hideQueue();
	if(visible)
		windowObject->showQueue();
}

void BattleInterface::setStickyHeroWindowsVisibility(bool visible)
{
	windowObject->hideStickyHeroWindows();
	if(visible)
		windowObject->showStickyHeroWindows();
}

void BattleInterface::setStickyQuickSpellWindowVisibility(bool visible)
{
	windowObject->hideStickyQuickSpellWindow();
	if(visible)
		windowObject->showStickyQuickSpellWindow();
}
