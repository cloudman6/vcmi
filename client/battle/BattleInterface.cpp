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
#include "BattleAttackDirection.h"
#include "BattleConsole.h"
#include "BattleEffectsController.h"
#include "BattleFieldController.h"
#include "BattleFocusRestore.h"
#include "BattleFocusStatusSync.h"
#include "BattleHero.h"
#include "BattleMovementPreview.h"
#include "BattleObstacleController.h"
#include "BattleProjectileController.h"
#include "BattleRangedShooting.h"
#include "BattleRenderer.h"
#include "BattleResultWindow.h"
#include "BattleSiegeController.h"
#include "BattleStackSwitching.h"
#include "BattleStacksController.h"
#include "BattleWindow.h"
#include "CreatureAnimation.h"

#include "../CPlayerInterface.h"
#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../adventureMap/AdventureMapInterface.h"
#include "../gui/CursorHandler.h"
#include "../gui/WindowHandler.h"
#include "../media/IMusicPlayer.h"
#include "../media/ISoundPlayer.h"
#include "../render/Canvas.h"
#include "../windows/CTutorialWindow.h"

#include "../../lib/battle/CBattleInfoCallback.h"
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

	//if we found interface of player with tactics, then enter tactics mode
	tacticsMode = static_cast<bool>(tacticianInterface);

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

		if (settings["gameTweaks"]["skipBattleIntroMusic"].Bool())
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
	if(tacticsMode)
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
}

void BattleInterface::stackAdded(const CStack * stack)
{
	stacksController->stackAdded(stack, false);
}

void BattleInterface::stackRemoved(uint32_t stackID)
{
	stacksController->stackRemoved(stackID);
	fieldController->redrawBackgroundWithHexes();
	windowObject->updateQueue();
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

void BattleInterface::handleFocusNavigationShortcut(EShortcut shortcut)
{
	// D3: while choosing the melee approach, left/right cycle the origin
	// hex instead of moving the focus
	if(ENGINE->input().getCurrentInputMode() == InputMode::CONTROLLER
		&& controllerStates.top() == BattleControllerStateMachine::State::ATTACK_DIRECTION
		&& (shortcut == EShortcut::MOVE_LEFT || shortcut == EShortcut::MOVE_RIGHT))
	{
		controllerAttackFromHex = BattleAttackDirection::cycle(meleeAttackCandidates(), controllerAttackFromHex, shortcut == EShortcut::MOVE_RIGHT);
		return;
	}

	focusNavigation->handleShortcut(shortcut, ENGINE->input().getCurrentInputMode());

	// D5: the controller focus owns the official hover status host (damage
	// and retaliation preview) in every layer, not only while the movement
	// preview is open, so the focused target stays announced while navigating
	const BattleHex statusHex = BattleFocusStatusSync::decide(ENGINE->input().getCurrentInputMode(), focusModel);
	if(statusHex.isValid())
		actionsController->onHexHovered(statusHex);
}

void BattleInterface::handleControllerCancel()
{
	const bool controllerMode = ENGINE->input().getCurrentInputMode() == InputMode::CONTROLLER;
	const bool canPopLayer = controllerStates.depth() > 1;

	switch(BattleControllerStateMachine::decideCancel(controllerMode, canPopLayer, actionsController->isCastingSpell()))
	{
		case BattleControllerStateMachine::CancelDecision::POP_LAYER:
			controllerStates.cancel();
			return;
		case BattleControllerStateMachine::CancelDecision::CANCEL_SPELL:
			// pre-existing GLOBAL_CANCEL behavior, kept in every input mode
			actionsController->endCastingSpell();
			return;
		case BattleControllerStateMachine::CancelDecision::OPEN_PARENT_LAYER:
			windowObject->openOptionsWindow();
			return;
	}
}

void BattleInterface::onActiveStackChanged(const CStack * stack)
{
	// CStack::getPosition() is the head hex for wide units, the single-hex
	// anchor D8 and BT-01 require
	const BattleHex activeHead = stack != nullptr ? stack->getPosition() : BattleHex::INVALID;
	const BattleHex restoreHex = BattleFocusRestore::decide(ENGINE->input().getCurrentInputMode(), activeHead);
	if(!restoreHex.isValid())
		return;

	if(!focusModel.setFocus(restoreHex))
		return;

	// same status host as the other focus paths, so the damage preview
	// follows the restored focus
	actionsController->onHexHovered(BattleFocusStatusSync::decide(ENGINE->input().getCurrentInputMode(), focusModel));
}

bool BattleInterface::trySwitchStack(bool forward)
{
	if(ENGINE->input().getCurrentInputMode() != InputMode::CONTROLLER)
		return false;

	if(!controllerStates.canSwitchStacks())
		return false;

	const auto battle = getBattle();
	if(!battle)
		return false;

	const auto * activeUnit = battle->battleActiveUnit();
	if(!activeUnit)
		return false;

	const BattleSide ownSide = activeUnit->unitSide();

	// current-round turn queue; own alive units become switch candidates
	std::vector<battle::Units> turnOrder;
	battle->battleGetTurnOrder(turnOrder, GameConstants::ARMY_SIZE * 2, 1);

	std::vector<BattleStackSwitchEntry> candidates;
	for(const auto & turn : turnOrder)
	{
		for(const auto * unit : turn)
		{
			if(unit->unitSide() != ownSide || !unit->alive())
				continue;

			const auto unitId = static_cast<int32_t>(unit->unitId());
			bool duplicate = false;
			for(const auto & entry : candidates)
				duplicate |= entry.unitId == unitId;

			if(!duplicate)
				candidates.push_back({unitId, unit->getPosition(), unit->occupiedHex()});
		}
	}

	const auto entry = BattleStackSwitching::select(candidates, focusModel.getFocusedHex(), forward);
	if(entry.unitId < 0)
		return false;

	if(!focusModel.setFocus(entry.headHex))
		return false;

	// same status host as the mouse hover path, so the damage preview
	// follows the newly focused unit
	actionsController->onHexHovered(BattleFocusStatusSync::decide(ENGINE->input().getCurrentInputMode(), focusModel));
	return true;
}

std::vector<BattleHex> BattleInterface::meleeAttackCandidates() const
{
	std::vector<BattleHex> candidates;

	const CStack * activeStack = stacksController->getActiveStack();
	if(!activeStack || !focusModel.hasFocus())
		return candidates;

	const auto battle = getBattle();
	if(!battle)
		return candidates;

	const BattleHex focusHex = focusModel.getFocusedHex();
	const CStack * targetStack = battle->battleGetStackByPos(focusHex, true);
	if(!targetStack || targetStack->unitSide() == activeStack->unitSide())
		return candidates;

	if(!battle->battleCanAttackUnit(activeStack, targetStack))
		return candidates;

	// same reachability contract as the mouse path (canReach &&
	// battleCanAttackUnit): an origin hex only becomes an attack option
	// when the active stack can actually stand there this turn
	const auto & availableHexes = fieldController->getAvailableHexes();

	// same direction scan order as findAttackFromHex fallback
	for(int direction = 0; direction < 8; ++direction)
	{
		const BattleHex origin = battle->fromWhichHexAttack(activeStack, focusHex, static_cast<BattleHex::EDir>(direction), false);
		if(!origin.isValid() || !availableHexes.contains(origin))
			continue;

		bool duplicate = false;
		for(const auto & candidate : candidates)
			duplicate |= candidate == origin;

		if(!duplicate)
			candidates.push_back(origin);
	}

	return candidates;
}

BattleRangedShooting::DisabledReason BattleInterface::shootingDisabledReason() const
{
	const CStack * activeStack = stacksController->getActiveStack();
	if(!activeStack || !focusModel.hasFocus())
		return BattleRangedShooting::DisabledReason::NONE;

	const auto battle = getBattle();
	if(!battle)
		return BattleRangedShooting::DisabledReason::NONE;

	const BattleHex focusHex = focusModel.getFocusedHex();
	const CStack * targetStack = battle->battleGetStackByPos(focusHex, true);
	const bool enemyTarget = targetStack != nullptr && targetStack->unitSide() != activeStack->unitSide();

	// same rule order as battleCanShoot: ammo, adjacent-enemy block, range
	return BattleRangedShooting::classify(
		enemyTarget,
		activeStack->canShoot(),
		!activeStack->canShootBlocked() && battle->battleIsUnitBlocked(activeStack),
		battle->battleCanShoot(activeStack, focusHex));
}

BattleHintBar::Context BattleInterface::buildHintContext() const
{
	BattleHintBar::Context context;

	const CStack * activeStack = stacksController->getActiveStack();
	// the own stack's current position is never a movement destination
	context.focusedReachable = activeStack != nullptr
		&& focusModel.hasFocus()
		&& focusModel.getFocusedHex() != activeStack->getPosition()
		&& fieldController->getAvailableHexes().contains(focusModel.getFocusedHex());

	context.attackable = !meleeAttackCandidates().empty();

	const CStack * targetStack = activeStack != nullptr && focusModel.hasFocus()
		? getBattle()->battleGetStackByPos(focusModel.getFocusedHex(), true)
		: nullptr;
	const bool enemyFocus = targetStack != nullptr && targetStack->unitSide() != activeStack->unitSide();
	context.shootable = enemyFocus && getBattle()->battleCanShoot(activeStack, focusModel.getFocusedHex());
	context.shootingDisabled = shootingDisabledReason();
	return context;
}

void BattleInterface::handleControllerAccept()
{
	if(ENGINE->input().getCurrentInputMode() != InputMode::CONTROLLER)
		return;

	// D6: the hint bar and the accept dispatch derive their view of the
	// focus from one shared context so prompts and actions always agree
	const auto hintContext = buildHintContext();
	const CStack * activeStack = stacksController->getActiveStack();

	// melee attack takes priority: an enemy focus is never a movement
	// destination, so both contracts can only compete on empty overlap
	const auto meleeOrigins = meleeAttackCandidates();
	const bool attackable = hintContext.attackable;

	switch(BattleAttackDirection::decideAccept(controllerStates.top(), attackable))
	{
		case BattleAttackDirection::MeleeOutcome::START_ACTION:
			controllerStates.enter(BattleControllerStateMachine::State::ACTION);
			controllerAttackFromHex = BattleAttackDirection::recommend(meleeOrigins);
			actionsController->onHexHovered(focusModel.getFocusedHex());
			return;
		case BattleAttackDirection::MeleeOutcome::OPEN_DIRECTION:
			controllerStates.enter(BattleControllerStateMachine::State::ATTACK_DIRECTION);
			return;
		case BattleAttackDirection::MeleeOutcome::COMMIT:
		{
			controllerStates.enter(BattleControllerStateMachine::State::COMMIT);
			BattleHex attackFrom = controllerAttackFromHex;
			bool stillValid = false;
			for(const auto & origin : meleeOrigins)
				stillValid |= origin == attackFrom;
			if(!stillValid)
				attackFrom = meleeOrigins.front(); // chosen origin vanished, fall back to the recommendation
			const auto command = BattleAction::makeMeleeAttack(activeStack, focusModel.getFocusedHex(), attackFrom, false);
			sendCommand(command, activeStack);
			controllerStates.reset();
			return;
		}
		case BattleAttackDirection::MeleeOutcome::CANCEL_LAYER:
			controllerStates.cancel();
			return;
		case BattleAttackDirection::MeleeOutcome::NONE:
			break;
	}

	// D4: shooting takes priority over movement for the same reason - an
	// enemy focus is never a movement destination
	switch(BattleRangedShooting::decideAccept(controllerStates.top(), hintContext.shootable))
	{
		case BattleRangedShooting::Outcome::START_ACTION:
			controllerStates.enter(BattleControllerStateMachine::State::ACTION);
			actionsController->onHexHovered(focusModel.getFocusedHex());
			return;
		case BattleRangedShooting::Outcome::COMMIT:
			controllerStates.enter(BattleControllerStateMachine::State::COMMIT);
			giveCommand(EActionType::SHOOT, focusModel.getFocusedHex());
			controllerStates.reset();
			return;
		case BattleRangedShooting::Outcome::CANCEL_LAYER:
			controllerStates.cancel();
			return;
		case BattleRangedShooting::Outcome::NONE:
			break;
	}

	switch(BattleMovementPreview::decideAccept(controllerStates.top(), hintContext.focusedReachable))
	{
		case BattleMovementPreview::Outcome::START_PREVIEW:
			controllerStates.enter(BattleControllerStateMachine::State::PREVIEW);
			actionsController->onHexHovered(focusModel.getFocusedHex());
			return;
		case BattleMovementPreview::Outcome::COMMIT:
		{
			controllerStates.enter(BattleControllerStateMachine::State::COMMIT);
			// same destination resolution as the mouse MOVE_STACK path
			const auto toHex = getBattle()->toWhichHexMove(activeStack, focusModel.getFocusedHex());
			if(toHex.isValid())
				giveCommand(EActionType::WALK, toHex);
			controllerStates.reset();
			return;
		}
		case BattleMovementPreview::Outcome::CANCEL_PREVIEW:
			controllerStates.cancel();
			return;
		case BattleMovementPreview::Outcome::NONE:
			return;
	}
}

void BattleInterface::sendCommand(BattleAction command, const CStack * actor)
{
	command.stackNumber = actor ? actor->unitId() : ((command.side == BattleSide::ATTACKER) ? -1 : -2);

	if(!tacticsMode)
	{
		logGlobal->trace("Setting command for %s", (actor ? actor->nodeName() : "hero"));
		stacksController->setActiveStack(nullptr);
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
	if(!tacticsMode)
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
		std::string formatted = line.toString();
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

	if ( defenderInt && defenderInt->playerID == player )
		curInt = defenderInt;
}

void BattleInterface::activateStack()
{
	stacksController->activateStack();

	const CStack * s = stacksController->getActiveStack();
	if(!s)
		return;

	windowObject->updateQueue();
	windowObject->blockUI(false);
	fieldController->redrawBackgroundWithHexes();
	actionsController->activateStack();

	// D8: default focus and restore follow every active stack change; runs
	// after the actions controller has computed the new stack's legal actions
	// so the restored focus status host observes a ready action set
	onActiveStackChanged(s);

	ENGINE->fakeMouseMove();
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
	if (tacticsMode)
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

void BattleInterface::tacticPhaseEnd()
{
	stacksController->setActiveStack(nullptr);
	tacticsMode = false;

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

	if (tacticsMode)
	{
		// Always end tactics mode. Player interface is blocked currently, so it's not possible that
		// the AI can take any action except end tactics phase (AI actions won't be triggered)
		//TODO implement the possibility that the AI will be triggered for further actions
		//TODO any solution to merge tactics phase & normal phase in the way it is handled by the player and battle interface?
		tacticPhaseEnd();
		stacksController->setActiveStack(nullptr);
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
