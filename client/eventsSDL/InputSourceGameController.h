/*
* InputSourceGameController.h, part of VCMI engine
*
* Authors: listed in file AUTHORS in main folder
*
* License: GNU General Public License v2.0 or later
* Full text of license available in license.txt file, in main folder
*
*/

#pragma once

#include <SDL_events.h>
#include <SDL_gamecontroller.h>

#include "../../lib/Point.h"
#include "../gui/Shortcut.h"

enum class ControllerPresentation
{
	GENERIC,
	UNKNOWN = GENERIC,
	PLAYSTATION,
	XBOX,
	NINTENDO
};

/// Class that handles game controller input from SDL events
class InputSourceGameController
{
	struct HeadlessTestTag
	{
	};

	friend class CObjectListWindowControllerTest;
	friend class InputHandler;

	static void gameControllerDeleter(SDL_GameController * gameController);
	using GameControllerPtr = std::unique_ptr<SDL_GameController, decltype(&gameControllerDeleter)>;

	std::map<int, GameControllerPtr> gameControllerMap;
	std::map<int, ControllerPresentation> controllerPresentations;
	std::set<std::pair<int, SDL_GameControllerAxis>> pressedAxes;
	int activeController = -1;
	ControllerPresentation activePresentation = ControllerPresentation::GENERIC;

	std::chrono::steady_clock::time_point lastCheckTime;
	double cursorAxisValueX;
	double cursorAxisValueY;
	double cursorPlanDisX;
	double cursorPlanDisY;

	bool scrollAxisMoved;
	Point scrollStart;
	Point scrollCurrent;
	double scrollAxisValueX;
	double scrollAxisValueY;
	double scrollPlanDisX;
	double scrollPlanDisY;

	const double configTriggerThreshold;
	const double configAxisDeadZone;
	const double configAxisFullZone;
	const double configAxisSpeed;
	const double configAxisScale;

	void openGameController(int index);
	void setActiveController(int instanceID);
	void invalidateControllerPresentation(int instanceID);
	InputSourceGameController(HeadlessTestTag);
	int getJoystickIndex(SDL_GameController * controller);
	double getRealAxisValue(int value) const;
	void resetAxisState();
	void dispatchAxisShortcuts(const std::vector<EShortcut> & shortcutsVector, int instanceID, SDL_GameControllerAxis axisID, int axisValue, std::string axisName);
	void tryToConvertCursor();
	void doCursorMove(int deltaX, int deltaY);
	int getMoveDis(float planDis);
	void handleCursorUpdate(int32_t deltaTimeMs);
	void handleScrollUpdate(int32_t deltaTimeMs);
	bool isScrollAxisReleased() const;

public:
	InputSourceGameController();
	void tryOpenAllGameControllers();
	void handleEventDeviceAdded(const SDL_ControllerDeviceEvent & device);
	void handleEventDeviceRemoved(const SDL_ControllerDeviceEvent & device);
	void handleEventDeviceRemapped(const SDL_ControllerDeviceEvent & device);
	bool isAxisMotionActive(const SDL_ControllerAxisEvent & axis) const;
	static double normalizeAxisValue(int value, double deadZone, double fullZone);
	void handleEventAxisMotion(const SDL_ControllerAxisEvent & axis);
	void handleEventButtonDown(const SDL_ControllerButtonEvent & button);
	void handleEventButtonUp(const SDL_ControllerButtonEvent & button);
	void handleUpdate();
	ControllerPresentation getActivePresentation() const;
	static std::optional<std::string> getGlyphToken(
		ControllerPresentation presentation, const std::vector<std::string> & bindings);
	static std::string getProfileName(ControllerPresentation presentation);
};
