/*
 * WindowHandler.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "WindowHandler.h"

#include "GameEngine.h"
#include "CIntObject.h"
#include "ControllerAxisReceiver.h"
#include "CursorHandler.h"

#include "../render/Canvas.h"
#include "../render/IScreenHandler.h"
#include "../render/Colors.h"

void WindowHandler::popWindow(std::shared_ptr<IShowActivatable> top)
{
	if (windowsStack.back() != top)
		throw std::runtime_error("Attempt to pop non-top window from stack!");

	top->deactivate();
	disposed.push_back(top);
	windowsStack.pop_back();
	if(!windowsStack.empty())
		windowsStack.back()->activate();
	refreshControllerCursorPolicy();

	totalRedraw();
}

void WindowHandler::pushWindow(std::shared_ptr<IShowActivatable> newInt)
{
	if (newInt == nullptr)
		throw std::runtime_error("Attempt to push null window onto windows stack!");

	if (vstd::contains(windowsStack, newInt))
		throw std::runtime_error("Attempt to add already existing window to stack!");

	if(!windowsStack.empty())
		windowsStack.back()->deactivate();
	windowsStack.push_back(newInt);
	ENGINE->cursor().set(Cursor::Map::POINTER);
	newInt->activate();
	refreshControllerCursorPolicy();
	totalRedraw();
}

bool WindowHandler::isTopWindowPopup() const
{
	if (windowsStack.empty())
		return false;

	return windowsStack.back()->isPopupWindow();
}

void WindowHandler::popWindows(int howMany)
{
	if(!howMany)
		return; //senseless but who knows...

	assert(windowsStack.size() >= howMany);
	windowsStack.back()->deactivate();
	for(int i = 0; i < howMany; i++)
	{
		disposed.push_back(windowsStack.back());
		windowsStack.pop_back();
	}

	if(!windowsStack.empty())
	{
		windowsStack.back()->activate();
		totalRedraw();
	}
	ENGINE->fakeMouseMove();
	refreshControllerCursorPolicy();
}

std::shared_ptr<IShowActivatable> WindowHandler::topWindowImpl() const
{
	if(windowsStack.empty())
		return nullptr;

	return windowsStack.back();
}

bool WindowHandler::isTopWindow(std::shared_ptr<IShowActivatable> window) const
{
	assert(window != nullptr);
	return !windowsStack.empty() && windowsStack.back() == window;
}

bool WindowHandler::isTopWindow(IShowActivatable * window) const
{
	assert(window != nullptr);
	return !windowsStack.empty() && windowsStack.back().get() == window;
}

void WindowHandler::totalRedraw()
{
	totalRedrawRequested = true;
}

void WindowHandler::totalRedrawImpl()
{
	logGlobal->debug("totalRedraw requested!");

	Canvas target = ENGINE->screenHandler().getScreenCanvas();

	for(auto & elem : windowsStack)
		elem->showAll(target);

	if(overlay)
		overlay->showAll(target);
}

void WindowHandler::simpleRedraw()
{
	if (totalRedrawRequested)
		totalRedrawImpl();
	else
		simpleRedrawImpl();

	totalRedrawRequested = false;
}

void WindowHandler::simpleRedrawImpl()
{
	Canvas target = ENGINE->screenHandler().getScreenCanvas();

	if(!windowsStack.empty())
		windowsStack.back()->show(target); //blit active interface/window

	if(overlay)
		overlay->showAll(target);
}

void WindowHandler::onScreenResize()
{
	for(const auto & entry : windowsStack)
		entry->onScreenResize();

	totalRedraw();
}

void WindowHandler::onFrameRendered()
{
	disposed.clear();
}

size_t WindowHandler::count() const
{
	return windowsStack.size();
}

ControllerAxisRoute WindowHandler::routeControllerAxis(const ControllerAxisEvent & event)
{
	for(auto it = windowsStack.rbegin(); it != windowsStack.rend(); ++it)
	{
		if(auto * receiver = dynamic_cast<IControllerAxisReceiver *>(it->get()))
		{
			if(it != windowsStack.rbegin())
				return ControllerAxisRoute::BLOCKED;
			return receiver->controllerAxisMoved(event);
		}
	}
	return ControllerAxisRoute::UNOWNED;
}

void WindowHandler::updateControllerAxis(uint32_t msPassed)
{
	if(!windowsStack.empty())
		if(auto * receiver = dynamic_cast<IControllerAxisReceiver *>(windowsStack.back().get()))
			receiver->controllerAxisUpdate(msPassed);
}

void WindowHandler::resetControllerAxis()
{
	for(auto it = windowsStack.rbegin(); it != windowsStack.rend(); ++it)
		if(auto * receiver = dynamic_cast<IControllerAxisReceiver *>(it->get()))
		{
			receiver->controllerAxisReset();
			return;
		}
}

void WindowHandler::refreshControllerCursorPolicy()
{
	ENGINE->cursor().setControllerNativeHidden(!isControllerCursorAllowed());
}

bool WindowHandler::isControllerCursorAllowed() const
{
	for(auto it = windowsStack.rbegin(); it != windowsStack.rend(); ++it)
		if(auto * receiver = dynamic_cast<IControllerAxisReceiver *>(it->get()))
			return receiver->controllerCursorAllowed();
	return true;
}

void WindowHandler::clear()
{
	if(!windowsStack.empty())
		windowsStack.back()->deactivate();

	windowsStack.clear();
	disposed.clear();
	refreshControllerCursorPolicy();
}

void WindowHandler::setOverlay(std::shared_ptr<IShowActivatable> newOverlay)
{
	if(overlay)
		overlay->deactivate();

	overlay = std::move(newOverlay);

	if(overlay)
		overlay->activate();

	totalRedraw();
}

std::vector<std::shared_ptr<IShowActivatable>> WindowHandler::detachAll()
{
	if(!windowsStack.empty())
		windowsStack.back()->deactivate();

	auto result = std::move(windowsStack);
	windowsStack.clear();
	disposed.clear();
	return result;
}

void WindowHandler::attachAll(std::vector<std::shared_ptr<IShowActivatable>> windows)
{
	clear();

	windowsStack = std::move(windows);

	if(!windowsStack.empty())
		windowsStack.back()->activate();

	totalRedraw();
}
