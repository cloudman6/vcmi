/*
 * CObjectListWindowControllerTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../client/StdInc.h"

#include "../client/GameEngine.h"
#include "../client/eventsSDL/InputHandler.h"
#include "../client/gui/EventDispatcher.h"
#include "../client/gui/Shortcut.h"
#include "../client/gui/ShortcutHandler.h"
#include "../client/gui/WindowHandler.h"
#include "../client/widgets/ObjectLists.h"
#include "../client/windows/GUIClasses.h"
#include "../client/eventsSDL/InputSourceGameController.h"
#include "../lib/CConfigHandler.h"

#include <gtest/gtest.h>

[[noreturn]] void handleFatalError(const std::string & message, bool)
{
	throw std::runtime_error(message);
}

class CObjectListWindowControllerTest : public testing::Test
{
protected:
	void SetUp() override
	{
		auto & testSettings = const_cast<JsonNode &>(settings.toJsonNode());
		testSettings["general"]["language"].String() = "english";
		ENGINE = std::unique_ptr<GameEngine>(new GameEngine(GameEngine::HeadlessTestTag()));
		ENGINE->input().setCurrentInputMode(InputMode::CONTROLLER);
	}

	void TearDown() override
	{
		ENGINE->windows().clear();
		ENGINE.reset();
	}

	std::shared_ptr<CObjectListWindow> createWindow(
		std::vector<CObjectListWindow::ListItem> items, std::function<void(int)> callback)
	{
		return CObjectListWindow::createForTesting(std::move(items), {}, std::move(callback));
	}

	std::shared_ptr<CObjectListWindow> createWindow(
		std::vector<int> itemIds, std::function<void(int)> callback)
	{
		return CObjectListWindow::createForTesting(std::move(itemIds), std::move(callback));
	}

	std::optional<size_t> focusedItem(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->focusedItem;
	}

	std::optional<size_t> enabledSelection(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->selectedItem;
	}

	const std::string & disabledReason(
		const std::shared_ptr<CObjectListWindow> & window, size_t index) const
	{
		return window->items.at(index).disabledReason;
	}

	bool isControllerFocusVisible(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->controllerFocusVisible;
	}

	void clickItem(const std::shared_ptr<CObjectListWindow> & window, size_t visibleIndex)
	{
		window->genItem(visibleIndex)->clickPressed(Point());
	}

	size_t listPosition(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->list->getPos();
	}

	void setVisibleItems(const std::shared_ptr<CObjectListWindow> & window, std::vector<size_t> visibleItems)
	{
		window->itemsVisible = std::move(visibleItems);
	}

	std::vector<size_t> visibleItems(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->itemsVisible;
	}

	void searchItems(const std::shared_ptr<CObjectListWindow> & window, const std::string & text)
	{
		window->itemsSearchCallback(text);
	}

	void setInputMode(InputMode mode)
	{
		ENGINE->input().setCurrentInputMode(mode);
	}

	void setLifecycleTrace(
		const std::shared_ptr<CObjectListWindow> & window,
		std::vector<std::string> & trace,
		std::string name)
	{
		window->lifecycleTrace = &trace;
		window->lifecycleTraceName = std::move(name);
	}

	std::array<ControllerPresentation, 3> controllerPresentationsAfterRemap()
	{
		InputSourceGameController controller{InputSourceGameController::HeadlessTestTag()};
		controller.controllerPresentations.emplace(17, ControllerPresentation::DUALSENSE);
		controller.setActiveController(17);
		const auto beforeRemap = controller.getActivePresentation();

		controller.invalidateControllerPresentation(17);
		const auto afterInvalidation = controller.getActivePresentation();

		controller.controllerPresentations.emplace(17, ControllerPresentation::UNKNOWN);
		controller.setActiveController(17);
		return {beforeRemap, afterInvalidation, controller.getActivePresentation()};
	}

};

class FocusScopeContractTest : public CObjectListWindowControllerTest
{
};

class WindowHandlerFocusLifecycleTest : public CObjectListWindowControllerTest
{
};

class ShortcutGlyphQueryTest : public CObjectListWindowControllerTest
{
protected:
	void setJoystickBindings(std::multimap<std::string, EShortcut> bindings)
	{
		ENGINE->shortcuts().mappedJoystickShortcuts = std::move(bindings);
	}
};

TEST_F(CObjectListWindowControllerTest, DisabledFocusRetainsEnabledSelection)
{
	auto window = createWindow({{"Enabled", true, ""}, {"Disabled", false, "Already selected"}}, [](int)
	{
	});

	ENGINE->windows().pushWindow(window);
	window->changeSelection(1);

	EXPECT_EQ(focusedItem(window), 1);
	EXPECT_EQ(window->selected(), 0);
}

TEST_F(CObjectListWindowControllerTest, DisabledItemDoesNotAccept)
{
	int acceptedItem = -1;
	auto window = createWindow({{"Enabled", true, ""}, {"Disabled", false, "Already selected"}}, [&acceptedItem](int item)
	{
		acceptedItem = item;
	});

	ENGINE->windows().pushWindow(window);
	window->changeSelection(1);
	ENGINE->events().dispatchShortcutPressed({EShortcut::GLOBAL_ACCEPT});

	EXPECT_EQ(acceptedItem, -1);
	EXPECT_TRUE(ENGINE->windows().isTopWindow(window));
}

TEST_F(CObjectListWindowControllerTest, DisabledItemDoesNotAcceptDoubleClickOrOpenPopup)
{
	int accepted = 0;
	int poppedUp = 0;
	int clicked = 0;
	auto window = createWindow({{"Enabled", true, ""}, {"Disabled", false, "Already selected"}}, [&accepted](int)
	{
		++accepted;
	});
	window->onPopup = [&poppedUp](int)
	{
		++poppedUp;
	};
	window->onClicked = [&clicked](int)
	{
		++clicked;
	};
	ENGINE->windows().pushWindow(window);

	auto disabledItem = window->genItem(1);
	disabledItem->clickPressed(Point());
	disabledItem->clickDouble(Point());
	disabledItem->showPopupWindow(Point());

	EXPECT_EQ(accepted, 0);
	EXPECT_EQ(poppedUp, 0);
	EXPECT_EQ(clicked, 0);
	EXPECT_TRUE(ENGINE->windows().isTopWindow(window));
	EXPECT_EQ(disabledReason(window, 1), "Already selected");
}

TEST_F(CObjectListWindowControllerTest, DisabledItemsRequireLocalizedReason)
{
	EXPECT_THROW(createWindow({{"Disabled", false, ""}}, [](int)
	{
	}), std::invalid_argument);
}

TEST_F(CObjectListWindowControllerTest, EnabledAcceptAndCancelKeepExistingClosePaths)
{
	int accepted = 0;
	int exited = 0;
	auto acceptedWindow = createWindow({{"Enabled", true, ""}}, [&accepted](int)
	{
		++accepted;
	});
	ENGINE->windows().pushWindow(acceptedWindow);
	ENGINE->events().dispatchShortcutPressed({EShortcut::GLOBAL_ACCEPT});

	EXPECT_EQ(accepted, 1);
	EXPECT_FALSE(ENGINE->windows().isTopWindow(acceptedWindow));

	auto canceledWindow = createWindow({{"Enabled", true, ""}}, [](int)
	{
	});
	canceledWindow->onExit = [&exited]
	{
		++exited;
	};
	ENGINE->windows().pushWindow(canceledWindow);
	ENGINE->events().dispatchShortcutPressed({EShortcut::GLOBAL_CANCEL});

	EXPECT_EQ(exited, 1);
	EXPECT_FALSE(ENGINE->windows().isTopWindow(canceledWindow));
}

TEST_F(FocusScopeContractTest, NavigationPreservesDisabledFocusAndEnabledSelection)
{
	std::vector<CObjectListWindow::ListItem> items;
	for(size_t index = 0; index < 12; ++index)
		items.push_back({std::to_string(index), index != 5, index == 5 ? "Already selected" : ""});

	auto window = createWindow(std::move(items), [](int)
	{
	});
	ENGINE->windows().pushWindow(window);

	for(size_t index = 0; index < 5; ++index)
		ENGINE->events().dispatchShortcutPressed({EShortcut::MOVE_DOWN});

	EXPECT_EQ(focusedItem(window), 5);
	EXPECT_EQ(enabledSelection(window), 4);
	EXPECT_EQ(listPosition(window), 0);

	for(size_t index = 0; index < 12; ++index)
		ENGINE->events().dispatchShortcutPressed({EShortcut::MOVE_DOWN});

	EXPECT_EQ(focusedItem(window), 11);
	EXPECT_EQ(enabledSelection(window), 11);
	EXPECT_EQ(listPosition(window), 3);
}

TEST_F(FocusScopeContractTest, NavigationShortcutsClampAndKeepFocusedItemVisible)
{
	std::vector<CObjectListWindow::ListItem> items;
	for(size_t index = 0; index < 12; ++index)
		items.push_back({std::to_string(index), true, ""});

	int accepted = 0;
	auto window = createWindow(std::move(items), [&accepted](int)
	{
		++accepted;
	});
	ENGINE->windows().pushWindow(window);

	ENGINE->events().dispatchShortcutPressed({EShortcut::MOVE_UP});
	EXPECT_EQ(focusedItem(window), 0);

	ENGINE->events().dispatchShortcutPressed({EShortcut::MOVE_PAGE_DOWN});
	EXPECT_EQ(focusedItem(window), 9);
	EXPECT_EQ(listPosition(window), 1);

	ENGINE->events().dispatchShortcutPressed({EShortcut::MOVE_LAST});
	EXPECT_EQ(focusedItem(window), 11);
	EXPECT_EQ(listPosition(window), 3);

	ENGINE->events().dispatchShortcutPressed({EShortcut::MOVE_DOWN});
	EXPECT_EQ(focusedItem(window), 11);

	ENGINE->events().dispatchShortcutPressed({EShortcut::MOVE_FIRST});
	EXPECT_EQ(focusedItem(window), 0);
	EXPECT_EQ(accepted, 0);
}

TEST_F(FocusScopeContractTest, EmptyListHasNoFocusOrEnabledSelection)
{
	int acceptedItem = -1;
	auto window = createWindow(std::vector<CObjectListWindow::ListItem>{}, [&acceptedItem](int item)
	{
		acceptedItem = item;
	});
	ENGINE->windows().pushWindow(window);

	ENGINE->events().dispatchShortcutPressed({EShortcut::MOVE_DOWN, EShortcut::GLOBAL_ACCEPT});

	EXPECT_FALSE(focusedItem(window));
	EXPECT_FALSE(enabledSelection(window));
	EXPECT_EQ(acceptedItem, -1);
	EXPECT_TRUE(ENGINE->windows().isTopWindow(window));
}

TEST_F(FocusScopeContractTest, VisibleItemDelegatesToCanonicalItem)
{
	auto window = createWindow({{"Alpha", true, ""}, {"Beta", true, ""}, {"Gamma", true, ""}}, [](int)
	{
	});
	ENGINE->windows().pushWindow(window);
	setVisibleItems(window, {1});

	window->genItem(0)->clickPressed(Point());

	EXPECT_EQ(focusedItem(window), 1);
	EXPECT_EQ(enabledSelection(window), 1);
}

TEST_F(FocusScopeContractTest, NonIdentityValuesSurviveFilteredProjectionAndItemActions)
{
	int accepted = -1;
	int clicked = -1;
	int opened = -1;
	auto window = createWindow(std::vector<int>({41, 73, 99}),
		[&accepted](int value)
		{
			accepted = value;
		});
	window->onClicked = [&clicked](int value)
	{
		clicked = value;
	};
	window->onPopup = [&opened](int value)
	{
		opened = value;
	};
	ENGINE->windows().pushWindow(window);
	setVisibleItems(window, {2, 0});

	auto item = window->genItem(0);
	item->clickPressed(Point());
	item->showPopupWindow(Point());

	EXPECT_EQ(focusedItem(window), 2);
	EXPECT_EQ(enabledSelection(window), 2);
	EXPECT_EQ(window->selected(), 0);
	EXPECT_EQ(clicked, 2);
	EXPECT_EQ(opened, 2);

	window->elementSelected();
	EXPECT_EQ(accepted, 2);
}

TEST_F(FocusScopeContractTest, RestoreUsesCanonicalVisibleFallback)
{
	auto window = createWindow(
		{{"Zero", true, ""}, {"One", false, "Already selected"}, {"Two", true, ""}, {"Three", true, ""}},
		[](int)
	{
	});
	ENGINE->windows().pushWindow(window);
	window->changeSelection(1);
	setVisibleItems(window, {2, 3});
	window->restoreFocus();

	EXPECT_EQ(focusedItem(window), 2);
	EXPECT_EQ(enabledSelection(window), 2);

	window->changeSelection(3);
	setVisibleItems(window, {0});
	window->restoreFocus();

	EXPECT_EQ(focusedItem(window), 0);
	EXPECT_EQ(enabledSelection(window), 0);

	setVisibleItems(window, {});
	window->restoreFocus();
	EXPECT_FALSE(focusedItem(window));
}

TEST_F(FocusScopeContractTest, MouseClickInvokesEnabledActionAndControllerReacquiresBeforeMoving)
{
	int clickedItem = -1;
	auto window = createWindow({{"Zero", true, ""}, {"One", true, ""}, {"Two", true, ""}}, [&clickedItem](int item)
	{
		clickedItem = item;
	});
	window->onClicked = [&clickedItem](int item)
	{
		clickedItem = item;
	};
	ENGINE->windows().pushWindow(window);

	clickItem(window, 1);

	EXPECT_EQ(focusedItem(window), 1);
	EXPECT_EQ(enabledSelection(window), 1);
	EXPECT_FALSE(isControllerFocusVisible(window));
	EXPECT_EQ(clickedItem, 1);

	ENGINE->events().dispatchShortcutPressed({EShortcut::MOVE_DOWN});
	EXPECT_EQ(focusedItem(window), 1);
	EXPECT_TRUE(isControllerFocusVisible(window));
	EXPECT_EQ(clickedItem, 1);

	ENGINE->events().dispatchShortcutPressed({EShortcut::MOVE_DOWN});
	EXPECT_EQ(focusedItem(window), 2);
	EXPECT_EQ(enabledSelection(window), 2);
}

TEST_F(FocusScopeContractTest, KeyboardNavigationMovesImmediatelyAfterMouseTakeover)
{
	auto window = createWindow(
		{{"Enabled", true, ""}, {"Disabled", false, "Already selected"}, {"Next", true, ""}},
		[](int)
	{
	});
	ENGINE->windows().pushWindow(window);

	clickItem(window, 1);
	setInputMode(InputMode::KEYBOARD_AND_MOUSE);
	ENGINE->events().dispatchShortcutPressed({EShortcut::MOVE_DOWN});

	EXPECT_EQ(focusedItem(window), 2);
	EXPECT_EQ(enabledSelection(window), 2);
}

TEST_F(FocusScopeContractTest, SearchCallbackKeepsFocusStateWithoutForcingControllerPresentation)
{
	auto window = createWindow(
		{{"Alpha", true, ""}, {"Beta", true, ""}, {"Beta disabled", false, "Already selected"}, {"Gamma", true, ""}},
		[](int)
	{
	});
	ENGINE->windows().pushWindow(window);
	window->changeSelection(2);
	setInputMode(InputMode::KEYBOARD_AND_MOUSE);

	searchItems(window, "beta");

	EXPECT_EQ(visibleItems(window), std::vector<size_t>({1, 2}));
	EXPECT_EQ(focusedItem(window), 2);
	EXPECT_EQ(enabledSelection(window), 1);
	EXPECT_FALSE(isControllerFocusVisible(window));
}

TEST_F(FocusScopeContractTest, TouchDerivedClickUsesExistingItemActivationPath)
{
	int clickedItem = -1;
	auto window = createWindow({{"Enabled", true, ""}}, [&clickedItem](int item)
	{
		clickedItem = item;
	});
	window->onClicked = [&clickedItem](int item)
	{
		clickedItem = item;
	};
	ENGINE->windows().pushWindow(window);

	window->genItem(0)->clickPressed(Point());

	EXPECT_EQ(clickedItem, 0);
	EXPECT_EQ(focusedItem(window), 0);
	EXPECT_EQ(enabledSelection(window), 0);
	EXPECT_FALSE(isControllerFocusVisible(window));
}

TEST_F(FocusScopeContractTest, CursorAxesRemainUncaptured)
{
	auto window = createWindow({{"Enabled", true, ""}}, [](int)
	{
	});

	EXPECT_FALSE(window->captureThisKey(EShortcut::MOUSE_CURSOR_X));
	EXPECT_FALSE(window->captureThisKey(EShortcut::MOUSE_CURSOR_Y));
}

TEST_F(WindowHandlerFocusLifecycleTest, PushSuspendsParentBeforeModalDispatchAndPopRestoresParentFocus)
{
	int parentAccepted = 0;
	int modalAccepted = 0;
	auto parent = createWindow({{"Enabled", true, ""}, {"Disabled", false, "Already selected"}}, [&parentAccepted](int)
	{
		++parentAccepted;
	});
	auto modal = createWindow({{"Modal item", true, ""}}, [&modalAccepted](int)
	{
		++modalAccepted;
	});

	ENGINE->windows().pushWindow(parent);
	parent->changeSelection(1);
	ENGINE->windows().pushWindow(modal);

	EXPECT_FALSE(isControllerFocusVisible(parent));
	ENGINE->events().dispatchShortcutPressed({EShortcut::GLOBAL_ACCEPT});

	EXPECT_EQ(modalAccepted, 1);
	EXPECT_EQ(parentAccepted, 0);
	EXPECT_TRUE(ENGINE->windows().isTopWindow(parent));
	EXPECT_TRUE(isControllerFocusVisible(parent));
	EXPECT_EQ(focusedItem(parent), 1);
	EXPECT_EQ(enabledSelection(parent), 0);
}

TEST_F(WindowHandlerFocusLifecycleTest, ClearSuspendsFocusedWindow)
{
	auto window = createWindow({{"Enabled", true, ""}}, [](int)
	{
	});
	ENGINE->windows().pushWindow(window);
	EXPECT_TRUE(isControllerFocusVisible(window));

	ENGINE->windows().clear();

	EXPECT_FALSE(isControllerFocusVisible(window));
}

TEST_F(WindowHandlerFocusLifecycleTest, PopWindowsKeepsInactiveWindowsSuspendedAndRestoresParent)
{
	auto parent = createWindow({{"Parent", true, ""}}, [](int)
	{
	});
	auto middle = createWindow({{"Middle", true, ""}}, [](int)
	{
	});
	auto top = createWindow({{"Top", true, ""}}, [](int)
	{
	});
	ENGINE->windows().pushWindow(parent);
	ENGINE->windows().pushWindow(middle);
	ENGINE->windows().pushWindow(top);

	ENGINE->windows().popWindows(2);

	EXPECT_FALSE(isControllerFocusVisible(middle));
	EXPECT_FALSE(isControllerFocusVisible(top));
	EXPECT_TRUE(isControllerFocusVisible(parent));
	EXPECT_TRUE(ENGINE->windows().isTopWindow(parent));
}

TEST_F(WindowHandlerFocusLifecycleTest, LifecycleOrderingUsesTopScopeAndNeverRestoresDisposedScopes)
{
	std::vector<std::string> trace;
	auto parent = createWindow({{"Parent", true, ""}}, [](int)
	{
	});
	auto modal = createWindow({{"Modal", true, ""}}, [](int)
	{
	});
	auto nested = createWindow({{"Nested", true, ""}}, [](int)
	{
	});
	setLifecycleTrace(parent, trace, "parent");
	setLifecycleTrace(modal, trace, "modal");
	setLifecycleTrace(nested, trace, "nested");

	ENGINE->windows().pushWindow(parent);
	EXPECT_EQ(trace, std::vector<std::string>({"parent.activate", "parent.restore"}));

	trace.clear();
	ENGINE->windows().pushWindow(modal);
	EXPECT_EQ(trace, std::vector<std::string>({
		"parent.suspend", "parent.deactivate", "modal.activate", "modal.restore"}));

	trace.clear();
	ENGINE->windows().popWindow(modal);
	EXPECT_EQ(trace, std::vector<std::string>({
		"modal.suspend", "modal.deactivate", "parent.activate", "parent.restore"}));

	trace.clear();
	ENGINE->windows().pushWindow(modal);
	ENGINE->windows().pushWindow(nested);
	EXPECT_EQ(trace, std::vector<std::string>({
		"parent.suspend", "parent.deactivate", "modal.activate", "modal.restore",
		"modal.suspend", "modal.deactivate", "nested.activate", "nested.restore"}));

	trace.clear();
	ENGINE->windows().popWindows(2);
	EXPECT_EQ(trace, std::vector<std::string>({
		"nested.suspend", "nested.deactivate", "parent.activate", "parent.restore"}));
	EXPECT_EQ(std::find(trace.begin(), trace.end(), "modal.restore"), trace.end());

	trace.clear();
	ENGINE->windows().clear();
	EXPECT_EQ(trace, std::vector<std::string>({"parent.suspend", "parent.deactivate"}));
}

TEST_F(ShortcutGlyphQueryTest, ReverseQuerySortsDeduplicatesAndRemapsBindings)
{
	setJoystickBindings({
		{"a", EShortcut::GLOBAL_ACCEPT},
		{"a", EShortcut::GLOBAL_ACCEPT},
		{"b", EShortcut::GLOBAL_CANCEL},
		{"x", EShortcut::GLOBAL_ACCEPT}
	});
	const auto acceptBindings = ENGINE->shortcuts().getJoystickBindings(EShortcut::GLOBAL_ACCEPT);
	const auto cancelBindings = ENGINE->shortcuts().getJoystickBindings(EShortcut::GLOBAL_CANCEL);

	EXPECT_EQ(acceptBindings, std::vector<std::string>({"a", "x"}));
	EXPECT_EQ(cancelBindings, std::vector<std::string>({"b"}));
	EXPECT_FALSE(InputSourceGameController::getGlyphToken(ControllerPresentation::DUALSENSE, acceptBindings));
	EXPECT_EQ(InputSourceGameController::getGlyphToken(ControllerPresentation::DUALSENSE, cancelBindings), "○");

	setJoystickBindings({
		{"a", EShortcut::GLOBAL_ACCEPT},
		{"b", EShortcut::GLOBAL_CANCEL}
	});

	const auto remappedAcceptBindings = ENGINE->shortcuts().getJoystickBindings(EShortcut::GLOBAL_ACCEPT);
	EXPECT_EQ(InputSourceGameController::getGlyphToken(ControllerPresentation::DUALSENSE, remappedAcceptBindings), "×");
}

TEST_F(ShortcutGlyphQueryTest, UnknownUnboundAndAmbiguousBindingsHaveNoGlyph)
{
	EXPECT_FALSE(InputSourceGameController::getGlyphToken(ControllerPresentation::UNKNOWN, {"a"}));
	EXPECT_FALSE(InputSourceGameController::getGlyphToken(ControllerPresentation::DUALSENSE, {}));
	EXPECT_FALSE(InputSourceGameController::getGlyphToken(ControllerPresentation::DUALSENSE, {"a", "b"}));
	EXPECT_FALSE(InputSourceGameController::getGlyphToken(ControllerPresentation::DUALSENSE, {"leftshoulder"}));
}

TEST_F(ShortcutGlyphQueryTest, ControllerRemapInvalidatesActivePresentation)
{
	const auto presentations = controllerPresentationsAfterRemap();
	EXPECT_EQ(presentations[0], ControllerPresentation::DUALSENSE);
	EXPECT_EQ(presentations[1], ControllerPresentation::UNKNOWN);
	EXPECT_EQ(presentations[2], ControllerPresentation::UNKNOWN);
}
