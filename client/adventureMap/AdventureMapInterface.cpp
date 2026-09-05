/*
 * AdventureMapInterface.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "AdventureMapInterface.h"

#include "AdventureOptions.h"
#include "AdventureState.h"
#include "CInGameConsole.h"
#include "CMinimap.h"
#include "CList.h"
#include "CInfoBar.h"
#include "MapAudioPlayer.h"
#include "TurnTimerWidget.h"
#include "AdventureMapWidget.h"
#include "AdventureMapShortcuts.h"

#include "../mapView/mapHandler.h"
#include "../mapView/MapView.h"
#include "../replay/GameplayReplayer.h"
#include "../windows/InfoWindows.h"
#include "../widgets/RadialMenu.h"
#include "../gui/CursorHandler.h"
#include "../gui/ControllerRadial.h"
#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../eventsSDL/InputHandler.h"
#include "../gui/Shortcut.h"
#include "../gui/WindowHandler.h"
#include "../render/Canvas.h"
#include "../render/IImage.h"
#include "../render/IRenderHandler.h"
#include "../render/IScreenHandler.h"
#include "../PlayerLocalState.h"
#include "../CPlayerInterface.h"

#include "../../lib/mapping/CMap.h"
#include "../../lib/GameLibrary.h"
#include "../../lib/IGameSettings.h"
#include "../../lib/StartInfo.h"
#include "../../lib/callback/CCallback.h"
#include "../../lib/texts/CGeneralTextHandler.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/CGTownInstance.h"
#include "../../lib/mapObjects/MiscObjects.h"
#include "../../lib/pathfinder/CGPathNode.h"
#include "../../lib/pathfinder/TurnInfo.h"
#include "../../lib/spells/adventure/AdventureSpellEffect.h"
#include "../../lib/spells/ISpellMechanics.h"
#include "../../lib/spells/Problem.h"
#include "../../lib/spells/CSpell.h"


std::shared_ptr<AdventureMapInterface> adventureInt;

namespace
{
int3 controllerTileDirection(double x, double y)
{
	const double angle = std::atan2(y, x) * 180.0 / M_PI;
	if(angle < -157.5 || angle >= 157.5) return int3(-1, 0, 0);
	if(angle < -112.5) return int3(-1, -1, 0);
	if(angle < -67.5) return int3(0, -1, 0);
	if(angle < -22.5) return int3(1, -1, 0);
	if(angle < 22.5) return int3(1, 0, 0);
	if(angle < 67.5) return int3(1, 1, 0);
	if(angle < 112.5) return int3(0, 1, 0);
	return int3(-1, 1, 0);
}

const CGObjectInstance * controllerActorMapObject(const CArmedInstance * actor)
{
	const auto * hero = dynamic_cast<const CGHeroInstance *>(actor);
	if(hero && hero->inBoat() && hero->getBoat())
		return hero->getBoat();
	return actor;
}
}

AdventureMapInterface::AdventureMapInterface():
	mapAudio(new MapAudioPlayer()),
	spellBeingCasted(nullptr),
	scrollingWasActive(false),
	scrollingWasBlocked(false),
	backgroundDimLevel(settings["adventure"]["backgroundDimLevel"].Integer())
{
	OBJECT_CONSTRUCTION;
	pos.x = pos.y = 0;
	pos.w = ENGINE->screenDimensions().x;
	pos.h = ENGINE->screenDimensions().y;

	shortcuts = std::make_shared<AdventureMapShortcuts>(*this);

	widget = std::make_shared<AdventureMapWidget>(shortcuts);
	shortcuts->setState(EAdventureState::MAKING_TURN);
	widget->getMapView()->onViewMapActivated();

	if(GAME->interface()->cb->getStartInfo()->turnTimerInfo.turnTimer != 0)
		watches = std::make_shared<TurnTimerWidget>(Point(24, 24));
	
	addUsedEvents(KEYBOARD | TIME | INPUT_MODE_CHANGE);
}

void AdventureMapInterface::onMapViewMoved(const Rect & visibleArea, int mapLevel)
{
	mapViewCenter = int3(visibleArea.center().x, visibleArea.center().y, mapLevel);

	shortcuts->onMapViewMoved(visibleArea, mapLevel);
	widget->getMinimap()->onMapViewMoved(visibleArea, mapLevel);
	widget->onMapViewMoved(visibleArea, mapLevel);
}

void AdventureMapInterface::onAudioResumed()
{
	mapAudio->onAudioResumed();
}

void AdventureMapInterface::onAudioPaused()
{
	mapAudio->onAudioPaused();
}

void AdventureMapInterface::onHeroMovementStarted(const CGHeroInstance * hero)
{
	if (shortcuts->optionMapViewActive())
	{
		widget->getInfoBar()->popAll();
		widget->getInfoBar()->showSelection();
	}
}

void AdventureMapInterface::onHeroChanged(const CGHeroInstance *h)
{
	widget->getHeroList()->updateElement(h);

	if (h && h == GAME->interface()->localState->getCurrentHero() && !widget->getInfoBar()->showingComponents())
		widget->getInfoBar()->showSelection();

	widget->updateActiveState();
}

void AdventureMapInterface::onTownChanged(const CGTownInstance * town)
{
	widget->getTownList()->updateElement(town);

	if (town && town == GAME->interface()->localState->getCurrentTown() && !widget->getInfoBar()->showingComponents())
		widget->getInfoBar()->showSelection();
}

void AdventureMapInterface::showInfoBoxMessage(const std::vector<Component> & components, std::string message, int timer)
{
	widget->getInfoBar()->pushComponents(components, message, timer);
}

void AdventureMapInterface::activate()
{
	CIntObject::activate();

	adjustActiveness();

	if(GAME->interface())
	{
		GAME->interface()->cingconsole->activate();
		GAME->interface()->cingconsole->pos = this->pos;
	}

	ENGINE->fakeMouseMove(); //to restore the cursor
	if(isControllerNativeMode())
	{
		ENGINE->cursor().setControllerNativeHidden(true);
		ensureControllerTarget();
		presentControllerTarget();
	}

	// workaround for an edge case:
	// if player unequips Angel Wings / Boots of Levitation of currently active hero
	// game will correctly invalidate paths but current route will not be updated since verifyPath() is not called for current hero
	if (GAME->interface()->makingTurn && GAME->interface()->localState->getCurrentHero())
		GAME->interface()->localState->verifyPath(GAME->interface()->localState->getCurrentHero());
}

void AdventureMapInterface::deactivate()
{
	controllerInputReset();
	CIntObject::deactivate();
	ENGINE->cursor().setControllerNativeHidden(false);
	ENGINE->cursor().set(Cursor::Map::POINTER);

	if(GAME->interface())
		GAME->interface()->cingconsole->deactivate();
}

bool AdventureMapInterface::isControllerNativeMode() const
{
	return ENGINE->input().getCurrentInputMode() == InputMode::CONTROLLER
		&& shortcuts->optionMapViewActive() && !controllerModeState.cursorMode();
}

bool AdventureMapInterface::usesNativeControllerAxis() const
{
	return ENGINE->input().getCurrentInputMode() == InputMode::CONTROLLER
		&& shortcuts->optionMapViewActive() && !controllerModeState.cursorMode();
}

void AdventureMapInterface::inputModeChanged(InputMode inputMode)
{
	controllerInputReset();
	ENGINE->cursor().setControllerNativeHidden(
		inputMode == InputMode::CONTROLLER
		&& shortcuts->optionMapViewActive()
		&& !controllerModeState.cursorMode());
	if(isControllerNativeMode())
	{
		ensureControllerTarget();
		presentControllerTarget();
	}
	redraw();
}

void AdventureMapInterface::controllerInputReset()
{
	controllerTileNavigation.reset();
	controllerObjectNavigation.reset();
	controllerNavigationOwner = ControllerNavigationOwner::NONE;
	controllerInstance = -1;
	controllerState.resetInput();
	controllerModeState.resetInput();
}

void AdventureMapInterface::updateControllerNavigationOwner(ControllerNavigationOwner changedOwner)
{
	auto & changed = changedOwner == ControllerNavigationOwner::TILE
		? controllerTileNavigation : controllerObjectNavigation;
	auto & other = changedOwner == ControllerNavigationOwner::TILE
		? controllerObjectNavigation : controllerTileNavigation;

	if(controllerNavigationOwner == ControllerNavigationOwner::NONE && changed.isActive())
		controllerNavigationOwner = changedOwner;
	else if(controllerNavigationOwner == changedOwner && !changed.isActive())
		controllerNavigationOwner = other.isActive()
			? (changedOwner == ControllerNavigationOwner::TILE
				? ControllerNavigationOwner::OBJECT : ControllerNavigationOwner::TILE)
			: ControllerNavigationOwner::NONE;
}

bool AdventureMapInterface::controllerAxisMoved(int instanceId, const std::vector<EShortcut> & actions, double value)
{
	if(controllerInstance != -1 && controllerInstance != instanceId)
		controllerInputReset();
	controllerInstance = instanceId;

	bool handled = false;
	for(const auto action : actions)
	{
		switch(action)
		{
		case EShortcut::MOUSE_CURSOR_X:
			if(controllerModeState.cameraHeld())
				controllerModeState.updateCameraAxis(true, value);
			else
			{
				controllerTileNavigation.update(true, value);
				updateControllerNavigationOwner(ControllerNavigationOwner::TILE);
			}
			handled = true;
			break;
		case EShortcut::MOUSE_CURSOR_Y:
			if(controllerModeState.cameraHeld())
				controllerModeState.updateCameraAxis(false, value);
			else
			{
				controllerTileNavigation.update(false, value);
				updateControllerNavigationOwner(ControllerNavigationOwner::TILE);
			}
			handled = true;
			break;
		case EShortcut::MOUSE_SWIPE_X:
			if(getState() != EAdventureState::WORLD_VIEW)
			{
				controllerObjectNavigation.update(true, value);
				updateControllerNavigationOwner(ControllerNavigationOwner::OBJECT);
			}
			handled = true;
			break;
		case EShortcut::MOUSE_SWIPE_Y:
			if(getState() != EAdventureState::WORLD_VIEW)
			{
				controllerObjectNavigation.update(false, value);
				updateControllerNavigationOwner(ControllerNavigationOwner::OBJECT);
			}
			handled = true;
			break;
		default:
			break;
		}
	}
	return handled;
}

const CGObjectInstance * AdventureMapInterface::getControllerObject(
	ObjectInstanceID id, const int3 & interactionTile) const
{
	const auto * object = GAME->interface()->cb->getObj(id, false);
	if(!object || !object->isVisitable() || object->visitablePos() != interactionTile)
		return nullptr;
	return object;
}

std::optional<AdventureMapControllerTarget> AdventureMapInterface::revalidateControllerTarget()
{
	if(!controllerState.target())
		return std::nullopt;
	const auto target = *controllerState.target();
	if(!target.objectId)
	{
		if(!GAME->interface()->cb->isInTheMap(target.interactionTile)
			|| target.interactionTile.z != mapViewCenter.z)
			return std::nullopt;
		return target;
	}

	for(const auto & candidate : widget->getMapView()->getVisibleObjectCandidates())
	{
		if(candidate.id != *target.objectId)
			continue;
		if(!handleTilePrimary(candidate.interactionTile, candidate.id, false))
			return std::nullopt;
		return AdventureMapControllerTarget{
			candidate.visualTile, candidate.id, candidate.interactionTile};
	}
	return std::nullopt;
}

void AdventureMapInterface::ensureControllerTarget()
{
	if(const auto current = revalidateControllerTarget())
	{
		controllerState.setTarget(*current);
		return;
	}

	const auto * actor = GAME->interface()->localState->getCurrentArmy();
	const auto * selected = controllerActorMapObject(actor);
	if(!selected)
	{
		controllerState.clearTarget();
		return;
	}

	for(const auto & candidate : widget->getMapView()->getVisibleObjectCandidates())
	{
		if(candidate.id == selected->id)
		{
			controllerState.setTarget({candidate.visualTile, candidate.id, candidate.interactionTile});
			return;
		}
	}
	controllerState.setTarget({selected->visitablePos(), selected->id, selected->visitablePos()});
}

std::vector<AdventureMapControllerObjectCandidate> AdventureMapInterface::getControllerObjectCandidates()
{
	std::vector<AdventureMapControllerObjectCandidate> result;
	for(const auto & candidate : widget->getMapView()->getVisibleObjectCandidates())
	{
		if(!handleTilePrimary(candidate.interactionTile, candidate.id, false))
			continue;
		result.push_back({candidate.id, candidate.visualCenter, candidate.interactionTile});
	}
	return result;
}

void AdventureMapInterface::presentControllerTarget()
{
	const auto target = revalidateControllerTarget();
	if(!target)
	{
		ensureControllerTarget();
		if(!controllerState.target())
		{
			controllerTargetCursorImage.reset();
			return;
		}
		onTileHovered(controllerState.target()->interactionTile, controllerState.target()->objectId);
	}
	else
	{
		controllerState.setTarget(*target);
		onTileHovered(target->interactionTile, target->objectId);
	}

	controllerTargetCursorImage = ENGINE->cursor().getCurrentImage();
	controllerTargetCursorPivot = ENGINE->cursor().getPivotOffset();
}

bool AdventureMapInterface::moveControllerTile()
{
	ensureControllerTarget();
	if(!controllerState.target())
		return false;

	const int3 target = controllerState.target()->visualTile
		+ controllerTileDirection(controllerTileNavigation.x(), controllerTileNavigation.y());
	if(!GAME->interface()->cb->isInTheMap(target))
		return false;

	controllerState.setTarget({target, std::nullopt, target});
	if(!widget->getMapView()->isTargetTileVisible(target))
		centerOnTile(target);
	presentControllerTarget();
	redraw();
	return true;
}

bool AdventureMapInterface::browseControllerObject()
{
	ensureControllerTarget();
	if(!controllerState.target())
		return false;

	const auto current = *controllerState.target();
	const auto selected = AdventureMapControllerObjectSelector::select(
		getControllerObjectCandidates(), Point(current.visualTile), current.objectId,
		controllerObjectNavigation.x(), controllerObjectNavigation.y());
	if(!selected)
		return false;

	for(const auto & candidate : widget->getMapView()->getVisibleObjectCandidates())
	{
		if(candidate.id != selected->id)
			continue;
		controllerState.setTarget({candidate.visualTile, candidate.id, candidate.interactionTile});
		presentControllerTarget();
		redraw();
		return true;
	}
	return false;
}

bool AdventureMapInterface::executeAdventureShortcut(EShortcut shortcut)
{
	for(const auto & state : shortcuts->getShortcuts())
	{
		if(state.shortcut != shortcut || !state.isEnabled)
			continue;
		state.callback();
		return true;
	}
	return false;
}

void AdventureMapInterface::focusControllerActor()
{
	const auto * actor = GAME->interface()->localState->getCurrentArmy();
	const auto * targetObject = controllerActorMapObject(actor);
	if(!targetObject)
	{
		controllerState.clearTarget();
		return;
	}

	for(const auto & candidate : widget->getMapView()->getVisibleObjectCandidates())
	{
		if(candidate.id != targetObject->id)
			continue;
		controllerState.setTarget({candidate.visualTile, candidate.id, candidate.interactionTile});
		presentControllerTarget();
		return;
	}
	controllerState.setTarget({targetObject->visitablePos(), targetObject->id, targetObject->visitablePos()});
	presentControllerTarget();
}

void AdventureMapInterface::panControllerCamera(uint32_t msPassed)
{
	if(!controllerModeState.cameraHeld() || !shortcuts->optionMapScrollingActive())
		return;
	const auto [x, y] = controllerModeState.cameraDirection();
	const double pixelsPerSecond = settings["adventure"]["scrollSpeedPixels"].Float();
	const Point delta(
		static_cast<int>(std::lround(x * pixelsPerSecond * msPassed / 1000.0)),
		static_cast<int>(std::lround(y * pixelsPerSecond * msPassed / 1000.0)));
	if(delta != Point())
		widget->getMapView()->onMapScrolled(delta);
}

void AdventureMapInterface::centerControllerCamera()
{
	ensureControllerTarget();
	if(controllerState.target())
		centerOnTile(controllerState.target()->visualTile);
}

void AdventureMapInterface::toggleControllerCursorMode()
{
	if(ENGINE->input().getCurrentInputMode() != InputMode::CONTROLLER
		|| getState() != EAdventureState::MAKING_TURN)
		return;
	ENGINE->input().clearControllerAxisMotion();
	controllerInputReset();
	controllerModeState.toggleCursorMode();
	ENGINE->cursor().setControllerNativeHidden(!controllerModeState.cursorMode());
	if(!controllerModeState.cursorMode())
	{
		ensureControllerTarget();
		presentControllerTarget();
	}
	redraw();
}

std::vector<ControllerRadialItem> AdventureMapInterface::controllerContextItems()
{
	struct Projection
	{
		EShortcut shortcut;
		const char * labelKey;
		size_t slot;
		const char * iconAnimation;
	};
	static constexpr std::array projections = {
		Projection{EShortcut::ADVENTURE_MOVE_HERO, "core.help.297.hover", 0, "IAM006"},
		Projection{EShortcut::ADVENTURE_CAST_SPELL, "core.help.298.hover", 7, "IAM007"},
		Projection{EShortcut::ADVENTURE_VISIT_OBJECT, "vcmi.adventureMap.revisitObject.hover", 6, ""},
		Projection{EShortcut::ADVENTURE_TOGGLE_MAP_LEVEL, "core.help.294.hover", 5, "IAM010"},
		Projection{EShortcut::ADVENTURE_END_TURN, "core.help.302.hover", 4, "IAM001"},
		Projection{EShortcut::ADVENTURE_TOGGLE_SLEEP, "core.help.296.hover", 3, "IAM005"},
		Projection{EShortcut::ADVENTURE_QUEST_LOG, "core.help.295.hover", 2, "IAM004"},
		Projection{EShortcut::ADVENTURE_DISEMBARK, "vcmi.adventureMap.disembark.hover", 1, ""}
	};

	const auto states = shortcuts->getShortcuts();
	std::vector<ControllerRadialItem> result;
	for(const auto & projection : projections)
	{
		const auto state = std::ranges::find(states, projection.shortcut, &AdventureMapShortcutState::shortcut);
		if(state == states.end() || !state->isEnabled)
			continue;
		ControllerRadialItem item{
			projection.shortcut,
			LIBRARY->generaltexth->translate(projection.labelKey),
			true,
			projection.slot,
			0,
			[this, shortcut = projection.shortcut](){ executeAdventureShortcut(shortcut); },
			projection.iconAnimation
		};
		item.iconPlayerColored = projection.iconAnimation[0] != '\0';
		result.push_back(std::move(item));
	}
	return result;
}

Rect AdventureMapInterface::controllerContextBounds() const
{
	return widget->getMapView()->pos;
}

void AdventureMapInterface::openControllerContext()
{
	if(getState() != EAdventureState::MAKING_TURN)
		return;
	controllerInputReset();
	ENGINE->windows().createAndPushWindow<ControllerRadial>(
		[this](){ return controllerContextItems(); },
		[this](){ return controllerContextBounds(); },
		EShortcut::ADVENTURE_CONTROLLER_CONTEXT,
		8,
		std::nullopt,
		std::vector{
			EShortcut::ADVENTURE_CONTROLLER_CAMERA,
			EShortcut::ADVENTURE_CONTROLLER_RECENTER,
			EShortcut::ADVENTURE_TOGGLE_CURSOR_MODE,
			EShortcut::ADVENTURE_NEXT_HERO,
			EShortcut::ADVENTURE_NEXT_TOWN,
			EShortcut::ADVENTURE_GAME_OPTIONS,
			EShortcut::GLOBAL_OPTIONS,
			EShortcut::ADVENTURE_CAST_SPELL,
			EShortcut::ADVENTURE_VISIT_OBJECT,
			EShortcut::ADVENTURE_TOGGLE_MAP_LEVEL,
			EShortcut::ADVENTURE_END_TURN,
			EShortcut::ADVENTURE_TOGGLE_SLEEP,
			EShortcut::ADVENTURE_QUEST_LOG,
			EShortcut::ADVENTURE_DISEMBARK,
			EShortcut::ADVENTURE_TOGGLE_GRID
		},
		LIBRARY->generaltexth->translate("vcmi.battleWindow.controller.select"),
		LIBRARY->generaltexth->translate("vcmi.battleWindow.controller.actionRadial.close")
	);
}

void AdventureMapInterface::drawControllerTarget(Canvas & to)
{
	if(!isControllerNativeMode() || !isActive() || !controllerState.target())
		return;

	CanvasClipRectGuard guard(to, terrainAreaPixels());
	const Rect target = widget->getMapView()->getTargetTileArea(controllerState.target()->visualTile);
	if(controllerTargetCursorImage)
		to.draw(controllerTargetCursorImage, target.center() - controllerTargetCursorPivot);
}

void AdventureMapInterface::showAll(Canvas & to)
{
	CIntObject::showAll(to);
	dim(to);
	drawControllerTarget(to);
	GAME->interface()->cingconsole->show(to);
}

void AdventureMapInterface::show(Canvas & to)
{
	CIntObject::show(to);
	dim(to);
	drawControllerTarget(to);
	GAME->interface()->cingconsole->show(to);
}

void AdventureMapInterface::dim(Canvas & to)
{
	auto const isBigWindow = [&](std::shared_ptr<CIntObject> window) { return window->pos.w >= 800 && window->pos.h >= 600; }; // OH3 fullscreen

	if(settings["adventure"]["hideBackground"].Bool())
		for (auto window : ENGINE->windows().findWindows<CIntObject>())
		{
			if(!std::dynamic_pointer_cast<AdventureMapInterface>(window) && std::dynamic_pointer_cast<CIntObject>(window) && isBigWindow(window))
			{
				to.fillTexture(ENGINE->renderHandler().loadImage(ImagePath::builtin("DiBoxBck"), EImageBlitMode::OPAQUE));
				return;
			}
		}
	for (auto window : ENGINE->windows().findWindows<CIntObject>())
	{
		if (!std::dynamic_pointer_cast<AdventureMapInterface>(window) && !std::dynamic_pointer_cast<RadialMenu>(window) && !window->isPopupWindow() && (settings["adventure"]["backgroundDimSmallWindows"].Bool() || isBigWindow(window) || shortcuts->getState() == EAdventureState::HOTSEAT_WAIT))
		{
			Rect targetRect(0, 0, ENGINE->screenDimensions().x, ENGINE->screenDimensions().y);
			ColorRGBA colorToFill(0, 0, 0, std::clamp<int>(backgroundDimLevel, 0, 255));
			if(backgroundDimLevel > 0)
				to.drawColorBlended(targetRect, colorToFill);
			return;
		}
	}
}

void AdventureMapInterface::tick(uint32_t msPassed)
{
	handleMapScrollingUpdate(msPassed);
	if(isControllerNativeMode())
	{
		panControllerCamera(msPassed);
		if(controllerNavigationOwner == ControllerNavigationOwner::TILE
			&& controllerTileNavigation.ready(msPassed))
			moveControllerTile();
		if(controllerNavigationOwner == ControllerNavigationOwner::OBJECT
			&& controllerObjectNavigation.ready(msPassed))
			browseControllerObject();
	}

	// we want animations to be active during enemy turn but map itself to be non-interactive
	// so call timer update directly on inactive element
	widget->getMapView()->tick(msPassed);
}

void AdventureMapInterface::handleMapScrollingUpdate(uint32_t timePassed)
{
	if(isControllerNativeMode())
	{
		scrollingWasActive = false;
		return;
	}

	/// Width of window border, in pixels, that triggers map scrolling
	static constexpr int32_t borderScrollWidth = 15;

	int32_t scrollSpeedPixels = settings["adventure"]["scrollSpeedPixels"].Float();
	int32_t scrollDistance = scrollSpeedPixels * timePassed / 1000;

	Point cursorPosition = ENGINE->getCursorPosition();
	Point scrollDirection;

	if (cursorPosition.x < borderScrollWidth)
		scrollDirection.x = -1;

	if (cursorPosition.x > ENGINE->screenDimensions().x - borderScrollWidth)
		scrollDirection.x = +1;

	if (cursorPosition.y < borderScrollWidth)
		scrollDirection.y = -1;

	if (cursorPosition.y > ENGINE->screenDimensions().y - borderScrollWidth)
		scrollDirection.y = +1;

	Point scrollDelta = scrollDirection * scrollDistance;

	bool cursorInScrollArea = scrollDelta != Point(0,0);
	bool scrollingActive = cursorInScrollArea && shortcuts->optionMapScrollingActive() && !scrollingWasBlocked;
	bool scrollingBlocked = ENGINE->isKeyboardCtrlDown() || !settings["adventure"]["borderScroll"].Bool() || !ENGINE->screenHandler().hasFocus();

	if (!scrollingWasActive && scrollingBlocked)
	{
		scrollingWasBlocked = true;
		return;
	}

	if (!cursorInScrollArea && scrollingWasBlocked)
	{
		scrollingWasBlocked = false;
		return;
	}

	if (scrollingActive)
		widget->getMapView()->onMapScrolled(scrollDelta);

	if (!scrollingActive && !scrollingWasActive)
		return;

	if(scrollDelta.x > 0)
	{
		if(scrollDelta.y < 0)
			ENGINE->cursor().set(Cursor::Map::SCROLL_NORTHEAST);
		if(scrollDelta.y > 0)
			ENGINE->cursor().set(Cursor::Map::SCROLL_SOUTHEAST);
		if(scrollDelta.y == 0)
			ENGINE->cursor().set(Cursor::Map::SCROLL_EAST);
	}
	if(scrollDelta.x < 0)
	{
		if(scrollDelta.y < 0)
			ENGINE->cursor().set(Cursor::Map::SCROLL_NORTHWEST);
		if(scrollDelta.y > 0)
			ENGINE->cursor().set(Cursor::Map::SCROLL_SOUTHWEST);
		if(scrollDelta.y == 0)
			ENGINE->cursor().set(Cursor::Map::SCROLL_WEST);
	}

	if (scrollDelta.x == 0)
	{
		if(scrollDelta.y < 0)
			ENGINE->cursor().set(Cursor::Map::SCROLL_NORTH);
		if(scrollDelta.y > 0)
			ENGINE->cursor().set(Cursor::Map::SCROLL_SOUTH);
		if(scrollDelta.y == 0)
			ENGINE->cursor().set(Cursor::Map::POINTER);
	}

	scrollingWasActive = scrollingActive;
}

void AdventureMapInterface::centerOnTile(int3 on)
{
	widget->getMapView()->onCenteredTile(on);
}

void AdventureMapInterface::centerOnObject(const CGObjectInstance * obj)
{
	widget->getMapView()->onCenteredObject(obj);
}

int3 AdventureMapInterface::getMapViewCenter() const
{
	return mapViewCenter;
}

void AdventureMapInterface::keyPressed(EShortcut key)
{
	if(ENGINE->input().getCurrentInputMode() == InputMode::CONTROLLER
		&& key == EShortcut::ADVENTURE_TOGGLE_CURSOR_MODE)
	{
		toggleControllerCursorMode();
		return;
	}
	if(isControllerNativeMode())
	{
		if(key == EShortcut::ADVENTURE_CONTROLLER_CONTEXT)
		{
			openControllerContext();
			return;
		}
		if(key == EShortcut::ADVENTURE_CONTROLLER_CAMERA)
		{
			if(getState() == EAdventureState::MAKING_TURN)
			{
				const auto direction = controllerTileNavigation.direction();
				controllerTileNavigation.reset();
				if(controllerNavigationOwner == ControllerNavigationOwner::TILE)
					controllerNavigationOwner = controllerObjectNavigation.isActive()
						? ControllerNavigationOwner::OBJECT : ControllerNavigationOwner::NONE;
				controllerModeState.setCameraHeld(true);
				controllerModeState.updateCameraAxis(true, direction.first);
				controllerModeState.updateCameraAxis(false, direction.second);
			}
			return;
		}
		if(key == EShortcut::ADVENTURE_CONTROLLER_RECENTER)
		{
			centerControllerCamera();
			return;
		}
		if(key == EShortcut::ADVENTURE_NEXT_HERO || key == EShortcut::ADVENTURE_NEXT_TOWN)
		{
			if(executeAdventureShortcut(key))
				focusControllerActor();
			return;
		}
		if(key == EShortcut::ADVENTURE_GAME_OPTIONS)
		{
			executeAdventureShortcut(key);
			return;
		}
		if(key == EShortcut::GLOBAL_ACCEPT || key == EShortcut::ADVENTURE_VIEW_SELECTED)
		{
			if(getState() == EAdventureState::WORLD_VIEW)
				hotkeyExitWorldView();
			else
			{
				ensureControllerTarget();
				controllerState.pressPrimary();
			}
			return;
		}
		if(key == EShortcut::GLOBAL_CANCEL)
		{
			if(spellBeingCasted)
				hotkeyAbortCastingMode();
			else if(getState() == EAdventureState::DISEMBARKING)
				exitDisembarkMode();
			else if(getState() == EAdventureState::WORLD_VIEW)
				hotkeyExitWorldView();
			else
			{
				controllerState.clearTarget();
				ensureControllerTarget();
				presentControllerTarget();
			}
			return;
		}
		if(key == EShortcut::MOUSE_LEFT || key == EShortcut::MOUSE_RIGHT)
			return;
	}
	if (key == EShortcut::GLOBAL_CANCEL && spellBeingCasted)
		hotkeyAbortCastingMode();
	if (key == EShortcut::GLOBAL_CANCEL && getState() == EAdventureState::DISEMBARKING)
		exitDisembarkMode();
	//fake mouse use to trigger onTileHovered()
	ENGINE->fakeMouseMove();
}

void AdventureMapInterface::keyReleased(EShortcut key)
{
	if(ENGINE->input().getCurrentInputMode() == InputMode::CONTROLLER
		&& key == EShortcut::ADVENTURE_CONTROLLER_CAMERA)
	{
		controllerModeState.setCameraHeld(false);
		return;
	}
	if(!isControllerNativeMode()
		|| (key != EShortcut::GLOBAL_ACCEPT && key != EShortcut::ADVENTURE_VIEW_SELECTED))
		return;
	if(getState() == EAdventureState::WORLD_VIEW)
		return;

	const auto current = revalidateControllerTarget();
	const auto committed = controllerState.releasePrimary(current);
	if(committed)
	{
		handleTilePrimary(committed->interactionTile, committed->objectId, true);
	}
	ensureControllerTarget();
	presentControllerTarget();
}

bool AdventureMapInterface::captureThisKey(EShortcut key)
{
	if(ENGINE->input().getCurrentInputMode() != InputMode::CONTROLLER)
		return false;
	if(!isActive())
		return false;
	if(key == EShortcut::ADVENTURE_TOGGLE_CURSOR_MODE)
		return true;
	if(!isControllerNativeMode())
		return false;
	return key == EShortcut::GLOBAL_ACCEPT || key == EShortcut::GLOBAL_CANCEL
		|| key == EShortcut::MOUSE_LEFT || key == EShortcut::MOUSE_RIGHT
		|| key == EShortcut::ADVENTURE_VIEW_SELECTED
		|| key == EShortcut::ADVENTURE_CONTROLLER_CONTEXT
		|| key == EShortcut::ADVENTURE_CONTROLLER_CAMERA
		|| key == EShortcut::ADVENTURE_CONTROLLER_RECENTER
		|| key == EShortcut::ADVENTURE_NEXT_HERO
		|| key == EShortcut::ADVENTURE_NEXT_TOWN
		|| key == EShortcut::ADVENTURE_GAME_OPTIONS;
}

void AdventureMapInterface::onSelectionChanged(const CArmedInstance *sel)
{
	assert(sel);

	widget->getInfoBar()->popAll();
	mapAudio->onSelectionChanged(sel);

	// while a replay follows another player, our own selection must not drag the camera along
	bool centerView = !settings["session"]["autoSkip"].Bool() && !replayFollowedPlayer();

	if (centerView)
		centerOnObject(sel);

	if(sel->ID==Obj::TOWN)
	{
		auto town = dynamic_cast<const CGTownInstance*>(sel);

		widget->getInfoBar()->showTownSelection(town);
		widget->getTownList()->updateWidget();
		widget->getTownList()->select(town);
		widget->getHeroList()->select(nullptr);
		onHeroChanged(nullptr);
	}
	else //hero selected
	{
		auto hero = dynamic_cast<const CGHeroInstance*>(sel);

		widget->getInfoBar()->showHeroSelection(hero);
		widget->getHeroList()->select(hero);
		widget->getTownList()->select(nullptr);

		GAME->interface()->localState->verifyPath(hero);
		onHeroChanged(hero);
	}

	widget->updateActiveState();
	widget->getHeroList()->redraw();
	widget->getTownList()->redraw();
}

void AdventureMapInterface::onTownOrderChanged()
{
	widget->getTownList()->updateWidget();
}

void AdventureMapInterface::onHeroOrderChanged()
{
	widget->getHeroList()->updateWidget();
}

void AdventureMapInterface::onMapTilesChanged(std::optional<FowTilesType> positions)
{
	if (positions)
		widget->getMinimap()->updateTiles(*positions);
	else
		widget->getMinimap()->update();

	if(isControllerNativeMode() && !revalidateControllerTarget())
	{
		ensureControllerTarget();
		presentControllerTarget();
	}
}

void AdventureMapInterface::onHotseatWaitStarted(PlayerColor playerID)
{
	backgroundDimLevel = 255;

	widget->getMinimap()->setAIRadar(true);
	onCurrentPlayerChanged(playerID);
	setState(EAdventureState::HOTSEAT_WAIT);
}

void AdventureMapInterface::onEnemyTurnStarted(PlayerColor playerID, bool isHuman)
{
	if(settings["session"]["spectate"].Bool())
		return;

	mapAudio->onEnemyTurnStarted();
	widget->getMinimap()->setAIRadar(!isHuman);
	widget->getInfoBar()->startEnemyTurn(playerID);
	setState(isHuman ? EAdventureState::MAKING_TURN : EAdventureState::AI_PLAYER_TURN);
}

EAdventureState AdventureMapInterface::getState() const
{
	return shortcuts->getState();
}

void AdventureMapInterface::setState(EAdventureState state)
{
	controllerInputReset();
	shortcuts->setState(state);
	adjustActiveness();
	widget->updateActiveState();
}

void AdventureMapInterface::adjustActiveness()
{
	bool widgetMustBeActive = isActive() && shortcuts->optionSidePanelActive();
	bool mapViewMustBeActive = isActive() && (shortcuts->optionMapViewActive());

	widget->setInputEnabled(widgetMustBeActive);
	widget->getMapView()->setInputEnabled(mapViewMustBeActive);
}

void AdventureMapInterface::onCurrentPlayerChanged(PlayerColor playerID)
{
	if (playerID == currentPlayerID)
		return;

	currentPlayerID = playerID;
	widget->setPlayerColor(playerID);
}

void AdventureMapInterface::onPlayerTurnStarted(PlayerColor playerID)
{
	backgroundDimLevel = settings["adventure"]["backgroundDimLevel"].Integer();

	onCurrentPlayerChanged(playerID);

	setState(EAdventureState::MAKING_TURN);
	if(playerID == GAME->interface()->playerID || settings["session"]["spectate"].Bool())
	{
		widget->getMinimap()->setAIRadar(false);
		widget->getInfoBar()->showSelection();
	}

	widget->getHeroList()->updateWidget();
	widget->getTownList()->updateWidget();

	const CGHeroInstance * heroToSelect = nullptr;

	// find first non-sleeping hero
	for (auto hero : GAME->interface()->localState->getWanderingHeroes())
	{
		if (!GAME->interface()->localState->isHeroSleeping(hero))
		{
			heroToSelect = hero;
			break;
		}
	}

	//select first hero if available.
	if (heroToSelect != nullptr)
	{
		GAME->interface()->localState->setSelection(heroToSelect);
	}
	else if (!GAME->interface()->localState->getOwnedTowns().empty())
	{
		GAME->interface()->localState->setSelection(GAME->interface()->localState->getOwnedTown(0));
	}
	else if (!GAME->interface()->localState->getWanderingHeroes().empty())
	{
		GAME->interface()->localState->setSelection(GAME->interface()->localState->getWanderingHero(0));
	}

	if (GAME->interface()->localState->getCurrentArmy())
		onSelectionChanged(GAME->interface()->localState->getCurrentArmy());

	//show new day animation and sound on infobar, except for 1st day of the game
	if (GAME->interface()->cb->getCalendar().getCurrentDay() != 1)
		widget->getInfoBar()->showDate();

	onHeroChanged(nullptr);
	ENGINE->windows().totalRedraw();
	mapAudio->onPlayerTurnStarted();

	if(settings["session"]["autoSkip"].Bool() && !ENGINE->isKeyboardShiftDown())
	{
		if(auto iw = ENGINE->windows().topWindow<CInfoWindow>())
			iw->close();

		ENGINE->dispatchMainThread([this]()
		{
			hotkeyEndingTurn();
		});
	}
}

void AdventureMapInterface::hotkeyEndingTurn()
{
	if(settings["session"]["spectate"].Bool())
		return;

	if(!settings["general"]["startTurnAutosave"].Bool())
	{
		GAME->interface()->performAutosave();
	}

	GAME->interface()->makingTurn = false;
	GAME->interface()->cb->endTurn();

	mapAudio->onPlayerTurnEnded();

	// Normally, game will receive PlayerStartsTurn call almost instantly with new player ID that will switch UI to waiting mode
	// However, when simturns are active it is possible for such call not to come because another player is still acting
	// So find first player other than ours that is acting at the moment and update UI as if he had started turn
	for (auto player = PlayerColor(0); player < PlayerColor::PLAYER_LIMIT; ++player)
	{
		if (player != GAME->interface()->playerID && GAME->interface()->cb->isPlayerMakingTurn(player))
		{
			onEnemyTurnStarted(player, GAME->interface()->cb->getStartInfo()->playerInfos.at(player).isControlledByHuman());
			break;
		}
	}
}

const CGObjectInstance* AdventureMapInterface::getActiveObject(const int3 &mapPos) const
{
	std::vector < const CGObjectInstance * > bobjs = GAME->interface()->cb->getBlockingObjs(mapPos);  //blocking objects at tile

	if (bobjs.empty())
		return nullptr;

	return *std::ranges::max_element(bobjs, &CMap::compareObjectBlitOrder);
}

void AdventureMapInterface::onTileLeftClicked(const int3 &targetPosition)
{
	handleTilePrimary(targetPosition, std::nullopt, true);
}

bool AdventureMapInterface::handleTilePrimary(
	const int3 & targetPosition, std::optional<ObjectInstanceID> fixedObject, bool execute)
{
	if(!shortcuts->optionMapViewActive())
		return false;

	const bool targetVisible = GAME->interface()->cb->isVisible(targetPosition);
	const CGObjectInstance * topBlocking = nullptr;
	if(fixedObject)
	{
		topBlocking = getControllerObject(*fixedObject, targetPosition);
		if(!targetVisible || !topBlocking)
			return false;
	}
	else if(targetVisible)
		topBlocking = getActiveObject(targetPosition);

	if(spellBeingCasted)
	{
		assert(shortcuts->optionSpellcasting());

		if(isValidAdventureSpellTarget(targetPosition))
		{
			if(execute)
				performSpellcasting(targetPosition);
			return true;
		}
		return false;
	}

	if(getState() == EAdventureState::DISEMBARKING)
	{
		if(isValidDisembarkTarget(targetPosition))
		{
			if(execute)
				performDisembark(targetPosition);
			return true;
		}
		return false;
	}

	if(!targetVisible)
		return false;

	const auto * currentArmy = GAME->interface()->localState->getCurrentArmy();
	if(!currentArmy)
		return false;
	const auto * currentHero = GAME->interface()->localState->getCurrentHero();
	if(fixedObject && currentHero && currentHero->inBoat() && currentHero->getBoat() == topBlocking)
		topBlocking = currentHero;

	//check if we can select this object
	bool canSelect = topBlocking && topBlocking->ID == Obj::HERO && topBlocking->tempOwner == GAME->interface()->playerID;
	canSelect |= topBlocking && topBlocking->ID == Obj::TOWN && GAME->interface()->cb->getPlayerRelations(GAME->interface()->playerID, topBlocking->tempOwner) != PlayerRelations::ENEMIES;

	if(currentArmy->ID != Obj::HERO) //hero is not selected (presumably town)
	{
		if(currentArmy == topBlocking) //selected town clicked
		{
			if(execute)
				GAME->interface()->openTownWindow(static_cast<const CGTownInstance*>(topBlocking));
			return true;
		}
		else if(canSelect)
		{
			if(execute)
				GAME->interface()->localState->setSelection(static_cast<const CArmedInstance*>(topBlocking));
			return true;
		}
		return false;
	}
	else if(currentHero) //hero is selected
	{
		EPathfindingLayer destinationLayer = EPathfindingLayer::AUTO;
		if (currentHero->inBoat() && currentHero->getBoat()->layer == EPathfindingLayer::AVIATE)
			destinationLayer = EPathfindingLayer::AVIATE;

		const CGPathNode *pn = GAME->interface()->getPathsInfo(currentHero)->getPathInfo(targetPosition, destinationLayer);
		if(!pn)
			return false;
		const auto shipyard = dynamic_cast<const IShipyard *>(topBlocking);

		if(currentHero == topBlocking) //clicked selected hero
		{
			if(execute)
				GAME->interface()->openHeroWindow(currentHero);
			return true;
		}
		else if(canSelect && pn->turns == 255 ) //selectable object at inaccessible tile
		{
			if(execute)
				GAME->interface()->localState->setSelection(static_cast<const CArmedInstance*>(topBlocking));
			return true;
		}
		else if(shipyard != nullptr && pn->turns == 255 && GAME->interface()->cb->getPlayerRelations(GAME->interface()->playerID, topBlocking->tempOwner) != PlayerRelations::ENEMIES)
		{
			if(execute)
				GAME->interface()->showShipyardDialogOrProblemPopup(shipyard);
			return true;
		}
		else //still here? we need to move hero if we clicked end of already selected path or calculate a new path otherwise
		{
			if(fixedObject && pn->turns == 255)
				return false;
			if(!execute)
				return true;

			int3 destinationTile = targetPosition;

			if(topBlocking && topBlocking->isVisitable() && !topBlocking->visitableAt(destinationTile) && settings["gameTweaks"]["simpleObjectSelection"].Bool())
				destinationTile = topBlocking->visitablePos();

			if(!settings["adventure"]["showMovePath"].Bool())
			{
				GAME->interface()->localState->setPath(currentHero, destinationTile, destinationLayer);
				onHeroChanged(currentHero);				
			}

			if(GAME->interface()->localState->hasPath(currentHero) &&
				GAME->interface()->localState->getPath(currentHero).endPos() == destinationTile &&
					(destinationLayer == EPathfindingLayer::AUTO || GAME->interface()->localState->getPath(currentHero).endLayer() == destinationLayer) &&
				!ENGINE->isKeyboardShiftDown())//we'll be moving
			{
				assert(!GAME->map().hasOngoingAnimations());
				if(!GAME->map().hasOngoingAnimations() && GAME->interface()->localState->getPath(currentHero).nextNode().turns == 0)
					GAME->interface()->moveHero(currentHero, GAME->interface()->localState->getPath(currentHero));
				return true;
			}
			else
			{
				if(ENGINE->isKeyboardShiftDown()) //normal click behaviour (as no hero selected)
				{
					if(canSelect)
						GAME->interface()->localState->setSelection(static_cast<const CArmedInstance*>(topBlocking));
				}
				else //remove old path and find a new one if we clicked on accessible tile
				{
					GAME->interface()->localState->setPath(currentHero, destinationTile, destinationLayer);
					onHeroChanged(currentHero);
				}
			}
			return true;
		}
	} //end of hero is selected "case"
	else
	{
		throw std::runtime_error("Nothing is selected...");
	}
	return false;
}

void AdventureMapInterface::onTileHovered(const int3 &targetPosition)
{
	onTileHovered(targetPosition, std::nullopt);
}

void AdventureMapInterface::onTileHovered(
	const int3 & targetPosition, std::optional<ObjectInstanceID> fixedObject)
{
	if(!shortcuts->optionMapViewActive())
		return;
	//if the player is not ingame (loser, winner, wrong) we are in a shutdown process
	if (!GAME->interface()->cb || GAME->interface()->cb->getPlayerStatus(GAME->interface()->playerID) != EPlayerStatus::INGAME)
		return;
	//may occur just at the start of game (fake move before full initialization)
	if(!GAME->interface()->localState->getCurrentArmy())
		return;

	bool isTargetPositionVisible = GAME->interface()->cb->isVisible(targetPosition);
	const CGObjectInstance * objAtTile = nullptr;
	if(isTargetPositionVisible)
		objAtTile = fixedObject ? getControllerObject(*fixedObject, targetPosition) : getActiveObject(targetPosition);
	const auto * currentHero = GAME->interface()->localState->getCurrentHero();
	if(fixedObject && currentHero && currentHero->inBoat() && currentHero->getBoat() == objAtTile)
		objAtTile = currentHero;

	if(spellBeingCasted)
	{
		const auto * hero = currentHero;
		const auto * spellEffect = spellBeingCasted->getAdventureMechanics().getEffectAs<AdventureSpellRangedEffect>(hero);
		spells::detail::ProblemImpl problem;

		if(spellEffect && spellEffect->canBeCastAtImpl(problem, GAME->interface()->cb.get(), hero, targetPosition))
			ENGINE->cursor().set(spellEffect->getCursorForTarget(GAME->interface()->cb.get(), hero, targetPosition));
		else
			ENGINE->cursor().set(Cursor::Map::POINTER);

		return;
	}

	if(getState() == EAdventureState::DISEMBARKING)
	{
		Cursor::Map cursorIndex = Cursor::Map::POINTER;
		const CGHeroInstance * hero = GAME->interface()->localState->getCurrentHero();

		if (hero && isValidDisembarkTarget(targetPosition))
		{
			std::array<Cursor::Map, 4> cursorDisembark = { Cursor::Map::T1_DISEMBARK,  Cursor::Map::T2_DISEMBARK,  Cursor::Map::T3_DISEMBARK,  Cursor::Map::T4_DISEMBARK,  };
			const CGPathNode * pathNode = GAME->interface()->getPathsInfo(hero)->getPathInfo(targetPosition, EPathfindingLayer::LAND);
			assert(pathNode);
			int turns = pathNode->turns;
			vstd::amin(turns, 3);
			cursorIndex = cursorDisembark[turns];
		}
		ENGINE->cursor().set(cursorIndex);
		return;
	}

	if(!isTargetPositionVisible)
	{
		ENGINE->cursor().set(Cursor::Map::POINTER);
		return;
	}

	auto objRelations = PlayerRelations::ALLIES;

	if(objAtTile)
	{
		objRelations = GAME->interface()->cb->getPlayerRelations(GAME->interface()->playerID, objAtTile->tempOwner);
		std::string text = (GAME->interface()->localState->getCurrentHero() ? objAtTile->getHoverText(GAME->interface()->localState->getCurrentHero()) : objAtTile->getHoverText(GAME->interface()->playerID)).toString(&GAME->translator());
		boost::replace_all(text,"\n"," ");
		if (ENGINE->isKeyboardCmdDown())
			text.append(" (" + std::to_string(targetPosition.x) + ", " + std::to_string(targetPosition.y) + ", " + std::to_string(targetPosition.z) + ")");
		ENGINE->statusbar()->write(text);
	}
	else if(isTargetPositionVisible)
	{
		std::string tileTooltipText = GAME->map().getTerrainDescr(targetPosition, false);
		if (ENGINE->isKeyboardCmdDown())
			tileTooltipText.append(" (" + std::to_string(targetPosition.x) + ", " + std::to_string(targetPosition.y) + ", " + std::to_string(targetPosition.z) + ")");
		ENGINE->statusbar()->write(tileTooltipText);
	}

	if(GAME->interface()->localState->getCurrentArmy()->ID == Obj::TOWN || ENGINE->isKeyboardShiftDown())
	{
		if(objAtTile)
		{
			if(objAtTile->ID == Obj::TOWN && objRelations != PlayerRelations::ENEMIES)
				ENGINE->cursor().set(Cursor::Map::TOWN);
			else if(objAtTile->ID == Obj::HERO && objRelations == PlayerRelations::SAME_PLAYER)
				ENGINE->cursor().set(Cursor::Map::HERO);
			else
				ENGINE->cursor().set(Cursor::Map::POINTER);
		}
		else
			ENGINE->cursor().set(Cursor::Map::POINTER);
	}
	else if(const CGHeroInstance * hero = GAME->interface()->localState->getCurrentHero())
	{
		std::array<Cursor::Map, 4> cursorMove      = { Cursor::Map::T1_MOVE,       Cursor::Map::T2_MOVE,       Cursor::Map::T3_MOVE,       Cursor::Map::T4_MOVE,       };
		std::array<Cursor::Map, 4> cursorAttack    = { Cursor::Map::T1_ATTACK,     Cursor::Map::T2_ATTACK,     Cursor::Map::T3_ATTACK,     Cursor::Map::T4_ATTACK,     };
		std::array<Cursor::Map, 4> cursorSail      = { Cursor::Map::T1_SAIL,       Cursor::Map::T2_SAIL,       Cursor::Map::T3_SAIL,       Cursor::Map::T4_SAIL,       };
		std::array<Cursor::Map, 4> cursorDisembark = { Cursor::Map::T1_DISEMBARK,  Cursor::Map::T2_DISEMBARK,  Cursor::Map::T3_DISEMBARK,  Cursor::Map::T4_DISEMBARK,  };
		std::array<Cursor::Map, 4> cursorExchange  = { Cursor::Map::T1_EXCHANGE,   Cursor::Map::T2_EXCHANGE,   Cursor::Map::T3_EXCHANGE,   Cursor::Map::T4_EXCHANGE,   };
		std::array<Cursor::Map, 4> cursorVisit     = { Cursor::Map::T1_VISIT,      Cursor::Map::T2_VISIT,      Cursor::Map::T3_VISIT,      Cursor::Map::T4_VISIT,      };
		std::array<Cursor::Map, 4> cursorSailVisit = { Cursor::Map::T1_SAIL_VISIT, Cursor::Map::T2_SAIL_VISIT, Cursor::Map::T3_SAIL_VISIT, Cursor::Map::T4_SAIL_VISIT, };
		std::array<Cursor::Map, 4> cursorAviate    = { Cursor::Map::T1_AVIATE,     Cursor::Map::T2_AVIATE,     Cursor::Map::T3_AVIATE,     Cursor::Map::T4_AVIATE,     };

		EPathfindingLayer destinationLayer = EPathfindingLayer::AUTO;
		if (hero->inBoat() && hero->getBoat()->layer == EPathfindingLayer::AVIATE)
			destinationLayer = EPathfindingLayer::AVIATE;

		const CGPathNode * pathNode = GAME->interface()->getPathsInfo(hero)->getPathInfo(targetPosition, destinationLayer);
		assert(pathNode);

		if((ENGINE->isKeyboardAltDown() || settings["gameTweaks"]["forceMovementInfo"].Bool()) && pathNode->reachable()) //overwrite status bar text with movement info
		{
			showMoveDetailsInStatusbar(*hero, *pathNode);
		}

		if (objAtTile && pathNode->action == EPathNodeAction::UNKNOWN)
		{
			if(objAtTile->ID == Obj::TOWN && objRelations != PlayerRelations::ENEMIES)
			{
				ENGINE-> cursor().set(Cursor::Map::TOWN);
				return;
			}
			else if(objAtTile->ID == Obj::HERO && objRelations == PlayerRelations::SAME_PLAYER)
			{
				ENGINE-> cursor().set(Cursor::Map::HERO);
				return;
			}
			else if (objAtTile->ID == Obj::SHIPYARD && objRelations != PlayerRelations::ENEMIES)
			{
				ENGINE-> cursor().set(Cursor::Map::T1_SAIL);
				return;
			}

			if(objAtTile->isVisitable() && !objAtTile->visitableAt(targetPosition) && settings["gameTweaks"]["simpleObjectSelection"].Bool())
				pathNode = GAME->interface()->getPathsInfo(hero)->getPathInfo(objAtTile->visitablePos());
		}

		int turns = pathNode->turns;
		vstd::amin(turns, 3);
		switch(pathNode->action)
		{
		case EPathNodeAction::NORMAL:
		case EPathNodeAction::TELEPORT_NORMAL:
			if(pathNode->layer == EPathfindingLayer::LAND)
				ENGINE->cursor().set(cursorMove[turns]);
			else if (pathNode->layer == EPathfindingLayer::AVIATE)
				ENGINE->cursor().set(cursorAviate[turns]);
			else
				ENGINE->cursor().set(cursorSail[turns]);
			break;

		case EPathNodeAction::VISIT:
		case EPathNodeAction::BLOCKING_VISIT:
		case EPathNodeAction::TELEPORT_BLOCKING_VISIT:
			if(objAtTile && objAtTile->ID == Obj::HERO)
			{
				if(GAME->interface()->localState->getCurrentArmy()  == objAtTile)
					ENGINE->cursor().set(Cursor::Map::HERO);
				else
					ENGINE->cursor().set(cursorExchange[turns]);
			}
			else if(pathNode->layer == EPathfindingLayer::LAND)
				ENGINE->cursor().set(cursorVisit[turns]);
			else if (pathNode->layer == EPathfindingLayer::SAIL &&
					 objAtTile &&
					 objAtTile->isCoastVisitable() &&
					 pathNode->theNodeBefore &&
					 pathNode->theNodeBefore->layer == EPathfindingLayer::LAND )
			{
				// exception - when visiting shipwreck located on coast from land - show 'horse' cursor, not 'ship' cursor
				ENGINE->cursor().set(cursorVisit[turns]);
			}
			else
				ENGINE->cursor().set(cursorSailVisit[turns]);
			break;

		case EPathNodeAction::BATTLE:
		case EPathNodeAction::TELEPORT_BATTLE:
			ENGINE->cursor().set(cursorAttack[turns]);
			break;

		case EPathNodeAction::EMBARK:
			{
				const CGBoat * boat = dynamic_cast<const CGBoat*>(objAtTile);
				assert(boat);
				ENGINE->cursor().set(boat->layer == EPathfindingLayer::AVIATE ? cursorAviate[turns] : cursorSail[turns]);
			}
			break;

		case EPathNodeAction::DISEMBARK:
			ENGINE->cursor().set(cursorDisembark[turns]);
			break;

		default:
				ENGINE->cursor().set(Cursor::Map::POINTER);
			break;
		}
	}
}

void AdventureMapInterface::showMoveDetailsInStatusbar(const CGHeroInstance & hero, const CGPathNode & pathNode)
{
	const int maxMovementPointsAtStartOfLastTurn = pathNode.turns > 0 ? hero.getTurnInfo(0)->getMaxMovePoints(pathNode.layer) : hero.movementPointsRemaining();
	const int movementPointsLastTurnCost = maxMovementPointsAtStartOfLastTurn - pathNode.moveRemains;
	const int remainingPointsAfterMove = pathNode.moveRemains;

	int totalMovementCost = hero.movementPointsRemaining();
	for (int i = 1; i <= pathNode.turns; ++i)
	{
		auto turnInfo = hero.getTurnInfo(i);
		if (pathNode.layer == EPathfindingLayer::SAIL)
			totalMovementCost += turnInfo->getMovePointsLimitWater();
		if (pathNode.layer == EPathfindingLayer::AVIATE)
			totalMovementCost += turnInfo->getMovePointsLimitAir();
		else
			totalMovementCost += turnInfo->getMovePointsLimitLand();
	}

	totalMovementCost -= pathNode.moveRemains;

	MetaString result;
	result.appendTextID(TextIdentifier("vcmi.adventureMap", pathNode.turns > 0 ? "moveCostDetails" : "moveCostDetailsNoTurns").get());
	result.replaceTokenNumber("%TURNS", pathNode.turns);
	result.replaceTokenNumber("%POINTS", movementPointsLastTurnCost);
	result.replaceTokenNumber("%REMAINING", remainingPointsAfterMove);
	result.replaceTokenNumber("%TOTAL", totalMovementCost);

	ENGINE->statusbar()->write(result.toString(&GAME->translator()));
}

void AdventureMapInterface::onTileRightClicked(const int3 &mapPos)
{
	if(!shortcuts->optionMapViewActive())
		return;

	if(spellBeingCasted)
	{
		hotkeyAbortCastingMode();
		return;
	}

	if(getState() == EAdventureState::DISEMBARKING)
	{
		exitDisembarkMode();
		return;
	}

	if(!GAME->interface()->cb->isVisible(mapPos))
	{
		CRClickPopup::createAndPush(LIBRARY->generaltexth->allTexts[61]); //Uncharted Territory
		return;
	}

	const CGObjectInstance * obj = getActiveObject(mapPos);
	if(!obj)
	{
		// Bare or undiscovered terrain
		const TerrainTile * tile = GAME->interface()->cb->getTile(mapPos);
		if(tile)
		{
			std::string hlp = GAME->map().getTerrainDescr(mapPos, true);
			CRClickPopup::createAndPush(hlp);
		}
		return;
	}

	CRClickPopup::createAndPush(obj, ENGINE->getCursorPosition(), ETextAlignment::CENTER);
}

void AdventureMapInterface::enterCastingMode(const CSpell * sp)
{
	spellBeingCasted = sp;
	GAME->interface()->localState->setCurrentSpell(sp->id);
	setState(EAdventureState::CASTING_SPELL);
}

void AdventureMapInterface::exitCastingMode()
{
	assert(spellBeingCasted);
	spellBeingCasted = nullptr;
	setState(EAdventureState::MAKING_TURN);
	GAME->interface()->localState->setCurrentSpell(SpellID::NONE);
}

void AdventureMapInterface::hotkeyAbortCastingMode()
{
	exitCastingMode();
	GAME->interface()->showInfoDialog(LIBRARY->generaltexth->allTexts[731]); //Spell cancelled
}

void AdventureMapInterface::performSpellcasting(const int3 & dest)
{
	SpellID id = spellBeingCasted->id;
	exitCastingMode();
	GAME->interface()->cb->castSpell(GAME->interface()->localState->getCurrentHero(), id, dest);
}

Rect AdventureMapInterface::terrainAreaPixels() const
{
	return widget->getMapView()->pos;
}

void AdventureMapInterface::hotkeyExitWorldView()
{
	setState(EAdventureState::MAKING_TURN);
	widget->getMapView()->onViewMapActivated();
}

void AdventureMapInterface::openWorldView(int tileSize)
{
	setState(EAdventureState::WORLD_VIEW);
	widget->getMapView()->onViewWorldActivated(tileSize);
}

void AdventureMapInterface::openWorldView()
{
	openWorldView(11);
}

void AdventureMapInterface::openWorldView(const std::vector<ObjectPosInfo>& objectPositions, bool showTerrain)
{
	openWorldView(11);
	widget->getMapView()->onViewSpellActivated(11, objectPositions, showTerrain);
}

void AdventureMapInterface::enterDisembarkMode()
{
	setState(EAdventureState::DISEMBARKING);
}

void AdventureMapInterface::exitDisembarkMode()
{
	assert(getState() == EAdventureState::DISEMBARKING);
	setState(EAdventureState::MAKING_TURN);
}

bool AdventureMapInterface::isValidDisembarkTarget(int3 targetPosition) const
{
	const CGHeroInstance * hero = GAME->interface()->localState->getCurrentHero();
	if (!hero || !hero->inBoat())
		return false;

	const CGPathNode * node = GAME->interface()->getPathsInfo(hero)->getPathInfo(targetPosition);
	
	return node && node->layer == CGPathNode::ELayer::LAND && node->reachable() &&
		(node->action == EPathNodeAction::DISEMBARK || node->action == EPathNodeAction::NORMAL) &&
		(node->accessible == EPathAccessibility::ACCESSIBLE || node->accessible == EPathAccessibility::GUARDED);
}

void AdventureMapInterface::performDisembark(const int3 & destTarget)
{
	const CGHeroInstance * hero = GAME->interface()->localState->getCurrentHero();
	exitDisembarkMode();
	
	// Set path to destination and move hero
	GAME->interface()->localState->setPath(hero, destTarget, CGPathNode::ELayer::LAND);
	if(GAME->interface()->localState->hasPath(hero))
	{
		const CGPath & path = GAME->interface()->localState->getPath(hero);
		if(path.nextNode().turns == 0)
			GAME->interface()->moveHero(hero, path);
	}
}

void AdventureMapInterface::hotkeyNextTown()
{
	int selectedIndex = widget->getTownList()->getSelectedIndex();
	widget->getTownList()->selectNext();

	if(selectedIndex == widget->getTownList()->getSelectedIndex())
		widget->getTownList()->refreshSelected();
}

void AdventureMapInterface::hotkeySwitchMapLevel()
{
	widget->getMapView()->onMapLevelSwitched();
}

void AdventureMapInterface::hotkeyZoom(int delta, bool useDeadZone)
{
	widget->getMapView()->onMapZoomLevelChanged(delta, useDeadZone);
}

void AdventureMapInterface::onScreenResize()
{
	OBJECT_CONSTRUCTION;

	// remember our activation state and reactive after reconstruction
	// since othervice activate() calls for created elements will bypass virtual dispatch
	// and will call directly CIntObject::activate() instead of dispatching virtual function call
	bool widgetActive = isActive();

	if (widgetActive)
		deactivate();

	widget.reset();
	pos.x = pos.y = 0;
	pos.w = ENGINE->screenDimensions().x;
	pos.h = ENGINE->screenDimensions().y;

	widget = std::make_shared<AdventureMapWidget>(shortcuts);
	widget->getMapView()->onViewMapActivated();
	widget->setPlayerColor(currentPlayerID);
	widget->updateActiveState();
	widget->getMinimap()->update();
	widget->getInfoBar()->showSelection();

	if (GAME->interface() && GAME->interface()->localState->getCurrentArmy())
		widget->getMapView()->onCenteredObject(GAME->interface()->localState->getCurrentArmy());

	adjustActiveness();

	if (widgetActive)
		activate();
}

bool AdventureMapInterface::isValidAdventureSpellTarget(int3 targetPosition) const
{
	spells::detail::ProblemImpl problem;

	return spellBeingCasted->getAdventureMechanics().canBeCastAt(problem, GAME->interface()->cb.get(), GAME->interface()->localState->getCurrentHero(), targetPosition);
}

void AdventureMapInterface::updateActiveState()
{
	widget->updateActiveState();
}
