/*
* InputSourceGameController.cpp, part of VCMI engine
*
* Authors: listed in file AUTHORS in main folder
*
* License: GNU General Public License v2.0 or later
* Full text of license available in license.txt file, in main folder
*
*/

#include "StdInc.h"
#include "InputSourceGameController.h"

#include "InputHandler.h"

#include "../GameEngine.h"
#include "../gui/CursorHandler.h"
#include "../gui/CIntObject.h"
#include "../gui/EventDispatcher.h"
#include "../gui/ShortcutHandler.h"
#include "../gui/WindowHandler.h"
#include "../render/IScreenHandler.h"

#include "../../lib/CConfigHandler.h"

namespace
{
ControllerPresentation controllerPresentation(SDL_GameController * controller)
{
	switch(SDL_GameControllerGetType(controller))
	{
	case SDL_CONTROLLER_TYPE_PS4:
	case SDL_CONTROLLER_TYPE_PS5:
		return ControllerPresentation::PLAYSTATION;
	case SDL_CONTROLLER_TYPE_XBOX360:
	case SDL_CONTROLLER_TYPE_XBOXONE:
		return ControllerPresentation::XBOX;
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
		return ControllerPresentation::NINTENDO;
	default:
		return ControllerPresentation::GENERIC;
	}
}
}

void InputSourceGameController::gameControllerDeleter(SDL_GameController * gameController)
{
	if(gameController)
		SDL_GameControllerClose(gameController);
}

InputSourceGameController::InputSourceGameController()
	: InputSourceGameController(HeadlessTestTag())
{
	tryOpenAllGameControllers();
}

InputSourceGameController::InputSourceGameController(HeadlessTestTag):
	cursorAxisValueX(0),
	cursorAxisValueY(0),
	cursorPlanDisX(0.0),
	cursorPlanDisY(0.0),
	scrollAxisMoved(false),
	scrollStart(Point(0,0)),
	scrollCurrent(Point(0,0)),
	scrollAxisValueX(0),
	scrollAxisValueY(0),
	scrollPlanDisX(0.0),
	scrollPlanDisY(0.0),
	configTriggerThreshold(settings["input"]["controllerTriggerThreshold"].Float()),
	configAxisDeadZone(settings["input"]["controllerAxisDeadZone"].Float()),
	configAxisFullZone(settings["input"]["controllerAxisFullZone"].Float()),
	configAxisSpeed(settings["input"]["controllerAxisSpeed"].Float()),
	configAxisScale(settings["input"]["controllerAxisScale"].Float())
{
}

void InputSourceGameController::tryOpenAllGameControllers()
{
	for(int i = 0; i < SDL_NumJoysticks(); ++i)
		if(SDL_IsGameController(i))
			openGameController(i);
		else
			logGlobal->warn("Joystick %d is an unsupported game controller!", i);
}

void InputSourceGameController::openGameController(int index)
{
	SDL_GameController * controller = SDL_GameControllerOpen(index);
	if(!controller)
	{
		logGlobal->error("Fail to open game controller %d!", index);
		return;
	}
	GameControllerPtr controllerPtr(controller, &gameControllerDeleter);

	// Need to save joystick index for event. Joystick index may not be equal to index sometimes.
	int joystickIndex = getJoystickIndex(controllerPtr.get());
	if(joystickIndex < 0)
	{
		logGlobal->error("Fail to get joystick index of game controller %d!", index);
		return;
	}

	if(gameControllerMap.find(joystickIndex) != gameControllerMap.end())
	{
		logGlobal->warn("Game controller with joystick index %d is already opened.", joystickIndex);
		return;
	}

	gameControllerMap.try_emplace(joystickIndex, std::move(controllerPtr));
	controllerPresentations.try_emplace(joystickIndex, controllerPresentation(gameControllerMap.at(joystickIndex).get()));
}

void InputSourceGameController::setActiveController(int instanceID)
{
	if(activeController != -1 && activeController != instanceID)
		resetAxisState();
	activeController = instanceID;
	if(const auto entry = controllerPresentations.find(instanceID); entry != controllerPresentations.end())
		activePresentation = entry->second;
	else
		activePresentation = ControllerPresentation::GENERIC;
}

void InputSourceGameController::invalidateControllerPresentation(int instanceID)
{
	controllerPresentations.erase(instanceID);
	if(activeController == instanceID)
		activePresentation = ControllerPresentation::GENERIC;
}

int InputSourceGameController::getJoystickIndex(SDL_GameController * controller)
{
	SDL_Joystick * joystick = SDL_GameControllerGetJoystick(controller);
	if(!joystick)
		return -1;

	SDL_JoystickID instanceID = SDL_JoystickInstanceID(joystick);
	if(instanceID < 0)
		return -1;
	return instanceID;
}

void InputSourceGameController::handleEventDeviceAdded(const SDL_ControllerDeviceEvent & device)
{
	if(gameControllerMap.find(device.which) != gameControllerMap.end())
	{
		logGlobal->warn("Game controller %d is already opened.", device.which);
		return;
	}
	openGameController(device.which);
}

void InputSourceGameController::handleEventDeviceRemoved(const SDL_ControllerDeviceEvent & device)
{
	if(gameControllerMap.find(device.which) == gameControllerMap.end())
	{
		logGlobal->warn("Game controller %d is not opened before.", device.which);
		return;
	}
	gameControllerMap.erase(device.which);
	invalidateControllerPresentation(device.which);
	if(activeController == device.which)
	{
		resetAxisState();
		activeController = -1;
		activePresentation = ControllerPresentation::GENERIC;
	}
}

void InputSourceGameController::handleEventDeviceRemapped(const SDL_ControllerDeviceEvent & device)
{
	if(gameControllerMap.find(device.which) == gameControllerMap.end())
	{
		logGlobal->warn("Game controller %d is not opened.", device.which);
		return;
	}
	gameControllerMap.erase(device.which);
	invalidateControllerPresentation(device.which);
	openGameController(device.which);
	if(activeController == device.which)
		setActiveController(device.which);
}

double InputSourceGameController::getRealAxisValue(int value) const
{
	return normalizeAxisValue(value, configAxisDeadZone, configAxisFullZone);
}

double InputSourceGameController::normalizeAxisValue(int value, double deadZone, double fullZone)
{
	const double ratio = std::clamp(static_cast<double>(value) / SDL_JOYSTICK_AXIS_MAX, -1.0, 1.0);
	const double magnitude = std::abs(ratio);
	if(magnitude <= deadZone || fullZone <= deadZone)
		return 0.0;
	const double normalized = std::clamp((magnitude - deadZone) / (fullZone - deadZone), 0.0, 1.0);
	return std::copysign(normalized, ratio);
}

void InputSourceGameController::resetAxisState()
{
	cursorAxisValueX = cursorAxisValueY = 0.0;
	cursorPlanDisX = cursorPlanDisY = 0.0;
	scrollAxisValueX = scrollAxisValueY = 0.0;
	scrollPlanDisX = scrollPlanDisY = 0.0;
	scrollAxisMoved = false;
	pressedAxes.clear();
	ENGINE->windows().resetControllerAxis();
}

void InputSourceGameController::dispatchAxisShortcuts(const std::vector<EShortcut> & shortcutsVector, int instanceID, SDL_GameControllerAxis axisID, int axisValue, std::string axisName)
{
	const auto pressedAxis = std::make_pair(instanceID, axisID);
	if(getRealAxisValue(axisValue) > configTriggerThreshold)
	{
		if(!pressedAxes.count(pressedAxis))
		{
			ENGINE->events().dispatchKeyPressed(axisName);
			ENGINE->events().dispatchShortcutPressed(shortcutsVector);
			pressedAxes.insert(pressedAxis);
		}
	}
	else
	{
		if(pressedAxes.count(pressedAxis))
		{
			ENGINE->events().dispatchKeyReleased(axisName);
			ENGINE->events().dispatchShortcutReleased(shortcutsVector);
			pressedAxes.erase(pressedAxis);
		}
	}
}

bool InputSourceGameController::isAxisMotionActive(const SDL_ControllerAxisEvent & axis) const
{
	// Tolerant comparison: getRealAxisValue returns exactly zero inside the dead
	// zone and quantized magnitudes of at least 1/32767 outside of it.
	return std::fabs(getRealAxisValue(axis.value)) > 1e-9;
}

void InputSourceGameController::handleEventAxisMotion(const SDL_ControllerAxisEvent & axis)
{
	SDL_GameControllerAxis axisID = static_cast<SDL_GameControllerAxis>(axis.axis);
	std::string axisName = SDL_GameControllerGetStringForAxis(axisID);
	const double normalizedValue = getRealAxisValue(axis.value);
	const bool axisActive = std::fabs(normalizedValue) > 1e-9;

	if(axisActive)
		setActiveController(axis.which);

	const auto presentationEntry = controllerPresentations.find(axis.which);
	const auto presentation = presentationEntry == controllerPresentations.end() ? ControllerPresentation::GENERIC : presentationEntry->second;
	const auto axisActions = ENGINE->shortcuts().translateJoystickAxis(axisName);
	const auto buttonActions = ENGINE->shortcuts().translateJoystickButton(axisName, getProfileName(presentation));
	const bool controlsActiveDevice = axis.which == activeController;

	const bool ownershipSensitive = std::any_of(axisActions.begin(), axisActions.end(), [](EShortcut action)
	{
		return action == EShortcut::CONTROLLER_NAVIGATE_X || action == EShortcut::CONTROLLER_NAVIGATE_Y
			|| action == EShortcut::MOUSE_CURSOR_X || action == EShortcut::MOUSE_CURSOR_Y;
	});
	if(ownershipSensitive && controlsActiveDevice)
	{
		const ControllerAxisEvent event{axis.which, axisName, axisActions, normalizedValue};
		const auto route = ENGINE->windows().routeControllerAxis(event);
		if(route == ControllerAxisRoute::CAPTURED || route == ControllerAxisRoute::BLOCKED)
		{
			for(const auto action : axisActions)
			{
				if(action == EShortcut::MOUSE_CURSOR_X)
					cursorAxisValueX = cursorPlanDisX = 0.0;
				if(action == EShortcut::MOUSE_CURSOR_Y)
					cursorAxisValueY = cursorPlanDisY = 0.0;
			}
			return;
		}
	}

	if(axisActive && controlsActiveDevice)
		tryToConvertCursor();

	if(controlsActiveDevice)
	for(const auto & action : axisActions)
	{
		switch(action)
		{
			case EShortcut::MOUSE_CURSOR_X:
				cursorAxisValueX = normalizedValue;
				break;
			case EShortcut::MOUSE_CURSOR_Y:
				cursorAxisValueY = normalizedValue;
				break;
			case EShortcut::MOUSE_SWIPE_X:
				scrollAxisValueX = normalizedValue;
				break;
			case EShortcut::MOUSE_SWIPE_Y:
				scrollAxisValueY = normalizedValue;
				break;
		}
	}

	dispatchAxisShortcuts(buttonActions, axis.which, axisID, axis.value, axisName);
}

void InputSourceGameController::tryToConvertCursor()
{
	if(ENGINE->cursor().getShowType() == Cursor::ShowType::HARDWARE)
	{
		int scalingFactor = ENGINE->screenHandler().getScalingFactor();
		const Point & cursorPosition = ENGINE->getCursorPosition();
		ENGINE->cursor().changeCursor(Cursor::ShowType::SOFTWARE);
		ENGINE->cursor().cursorMove(cursorPosition.x * scalingFactor, cursorPosition.y * scalingFactor);
		ENGINE->input().setCursorPosition(cursorPosition, PointerEventSource::CONTROLLER_CURSOR);
	}
}

void InputSourceGameController::handleEventButtonDown(const SDL_ControllerButtonEvent & button)
{
	setActiveController(button.which);
	std::string buttonName = SDL_GameControllerGetStringForButton(static_cast<SDL_GameControllerButton>(button.button));
	const auto shortcutsVector = ENGINE->shortcuts().translateJoystickButton(buttonName, getProfileName(activePresentation));
	
	ENGINE->events().dispatchKeyPressed(buttonName);
	ENGINE->events().dispatchShortcutPressed(shortcutsVector);
}

void InputSourceGameController::handleEventButtonUp(const SDL_ControllerButtonEvent & button)
{
	setActiveController(button.which);
	std::string buttonName = SDL_GameControllerGetStringForButton(static_cast<SDL_GameControllerButton>(button.button));
	const auto shortcutsVector = ENGINE->shortcuts().translateJoystickButton(buttonName, getProfileName(activePresentation));
	ENGINE->events().dispatchKeyReleased(buttonName);
	ENGINE->events().dispatchShortcutReleased(shortcutsVector);
}

std::string InputSourceGameController::getProfileName(ControllerPresentation presentation)
{
	switch(presentation)
	{
		case ControllerPresentation::PLAYSTATION: return "playstation";
		case ControllerPresentation::XBOX: return "xbox";
		case ControllerPresentation::NINTENDO: return "nintendo";
		case ControllerPresentation::GENERIC: return "generic";
	}
	return "generic";
}

ControllerPresentation InputSourceGameController::getActivePresentation() const
{
	return activePresentation;
}

std::optional<std::string> InputSourceGameController::getGlyphToken(
	ControllerPresentation presentation, const std::vector<std::string> & bindings)
{
	if(presentation != ControllerPresentation::PLAYSTATION || bindings.size() != 1)
		return std::nullopt;

	if(bindings.front() == "a")
		return "×";
	if(bindings.front() == "b")
		return "○";
	return std::nullopt;
}

void InputSourceGameController::doCursorMove(int deltaX, int deltaY)
{
	if(deltaX == 0 && deltaY == 0)
		return;
	const Point & screenSize = ENGINE->screenDimensions();
	const Point & cursorPosition = ENGINE->getCursorPosition();
	int scalingFactor = ENGINE->screenHandler().getScalingFactor();
	int newX = std::min(std::max(cursorPosition.x + deltaX, 0), screenSize.x);
	int newY = std::min(std::max(cursorPosition.y + deltaY, 0), screenSize.y);
	Point targetPosition{newX, newY};
	ENGINE->input().setCursorPosition(targetPosition, PointerEventSource::CONTROLLER_CURSOR);
	ENGINE->cursor().cursorMove(ENGINE->getCursorPosition().x * scalingFactor, ENGINE->getCursorPosition().y * scalingFactor);
}

int InputSourceGameController::getMoveDis(float planDis)
{
	if(planDis >= 0)
		return std::floor(planDis);
	else
		return std::ceil(planDis);
}

void InputSourceGameController::handleUpdate()
{
	std::chrono::steady_clock::time_point nowMs = std::chrono::steady_clock::now();

	if(lastCheckTime == std::chrono::steady_clock::time_point())
	{
		lastCheckTime = nowMs;
		return;
	}

	int32_t deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(nowMs - lastCheckTime).count();
	ENGINE->windows().updateControllerAxis(deltaTime);
	handleCursorUpdate(deltaTime);
	handleScrollUpdate(deltaTime);
	lastCheckTime = nowMs;
}

static double scaleAxis(double value, double power)
{
	if (value > 0)
		return std::pow(value, power);
	else
		return -std::pow(-value, power);
}

void InputSourceGameController::handleCursorUpdate(int32_t deltaTimeMs)
{
	if(!ENGINE->windows().isControllerCursorAllowed())
	{
		cursorAxisValueX = cursorAxisValueY = 0.0;
		cursorPlanDisX = cursorPlanDisY = 0.0;
		return;
	}

	float deltaTimeSeconds = static_cast<float>(deltaTimeMs) / 1000;

	if(vstd::isAlmostZero(cursorAxisValueX))
		cursorPlanDisX = 0;
	else
		cursorPlanDisX += deltaTimeSeconds * configAxisSpeed * scaleAxis(cursorAxisValueX, configAxisScale);

	if (vstd::isAlmostZero(cursorAxisValueY))
		cursorPlanDisY = 0;
	else
		cursorPlanDisY += deltaTimeSeconds * configAxisSpeed * scaleAxis(cursorAxisValueY, configAxisScale);

	int moveDisX = getMoveDis(cursorPlanDisX);
	int moveDisY = getMoveDis(cursorPlanDisY);
	cursorPlanDisX -= moveDisX;
	cursorPlanDisY -= moveDisY;
	doCursorMove(moveDisX, moveDisY);
}

void InputSourceGameController::handleScrollUpdate(int32_t deltaTimeMs)
{
	if(!scrollAxisMoved && isScrollAxisReleased())
	{
		return;
	}
	else if(!scrollAxisMoved && !isScrollAxisReleased())
	{
		scrollAxisMoved = true;
		scrollCurrent = scrollStart = ENGINE->input().getCursorPosition();
		ENGINE->events().dispatchGesturePanningStarted(scrollStart);
	}
	else if(scrollAxisMoved && isScrollAxisReleased())
	{
		ENGINE->events().dispatchGesturePanningEnded(scrollStart, scrollCurrent);
		scrollAxisMoved = false;
		scrollPlanDisX = scrollPlanDisY = 0;
		return;
	}
	float deltaTimeSeconds = static_cast<float>(deltaTimeMs) / 1000;
	scrollPlanDisX += deltaTimeSeconds * configAxisSpeed * scaleAxis(scrollAxisValueX, configAxisScale);
	scrollPlanDisY += deltaTimeSeconds * configAxisSpeed * scaleAxis(scrollAxisValueY, configAxisScale);
	int moveDisX = getMoveDis(scrollPlanDisX);
	int moveDisY = getMoveDis(scrollPlanDisY);
	if(moveDisX != 0 || moveDisY != 0)
	{
		scrollPlanDisX -= moveDisX;
		scrollPlanDisY -= moveDisY;
		scrollCurrent.x += moveDisX;
		scrollCurrent.y += moveDisY;
		Point distance(moveDisX, moveDisY);
		ENGINE->events().dispatchGesturePanning(scrollStart, scrollCurrent, distance);
	}
}

bool InputSourceGameController::isScrollAxisReleased() const
{
	return vstd::isAlmostZero(scrollAxisValueX) && vstd::isAlmostZero(scrollAxisValueY);
}
