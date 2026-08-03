/*
 * ModalFocusScopeSpikeTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "../Global.h"

#include "../client/CMT.h"
#include "../client/GameEngine.h"
#include "../client/gui/EventDispatcher.h"
#include "../client/gui/WindowHandler.h"
#include "../client/widgets/ObjectLists.h"
#include "../client/windows/GUIClasses.h"
#include "../client/windows/ModalFocusScopeSpike.h"
#include "../lib/CConfigHandler.h"
#include "../lib/filesystem/Filesystem.h"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>

#include <array>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

[[noreturn]] void handleFatalError(const std::string & message, bool)
{
	throw std::runtime_error(message);
}

class ModalFocusScopeSpikeHarness
{
public:
	static std::unique_ptr<GameEngine> makeHeadlessEngine()
	{
		return std::unique_ptr<GameEngine>(new GameEngine(GameEngine::ModalFocusScopeSpikeTestTag{}));
	}

	class Window final : public CObjectListWindow
	{
	public:
		explicit Window(std::vector<std::string> items)
			: CObjectListWindow(ModalFocusScopeSpikeTestTag{}, std::move(items))
		{
		}

		void close() override
		{
			closed = true;
			deactivate();
		}

		bool closed = false;
	};

	static std::shared_ptr<Window> makeWindow(std::vector<std::string> items)
	{
		return std::make_shared<Window>(std::move(items));
	}

	static std::string focusedId(const Window & window)
	{
		return std::string(window.modalFocusScope->focusedId());
	}

	static bool isSuspended(const Window & window)
	{
		return window.modalFocusScope->isSuspended();
	}

	static std::string disabledReason(const Window & window)
	{
		return std::string(window.modalFocusScope->disabledReason());
	}

	static size_t scrollPosition(const Window & window)
	{
		return window.list->getPos();
	}

	static void clickItem(Window & window, size_t index)
	{
		CObjectListWindow::CItem item(&window, index, CObjectListWindow::ModalFocusScopeSpikeTestTag{});
		item.clickPressed(Point());
	}

	static void setSelectionCallback(Window & window, std::function<void(int)> callback)
	{
		window.onSelect = std::move(callback);
	}
};

namespace
{
namespace bfs = boost::filesystem;

using Entry = ModalFocusScopeSpike::Entry;

class ModalFocusScopeSpikeEnvironment final : public ::testing::Environment
{
public:
	void SetUp() override
	{
		CResourceHandler::initialize();
		CResourceHandler::load("config/filesystem.json");
		settings.init("config/settings.json", "vcmi:settings");
	}

	void TearDown() override
	{
		CResourceHandler::destroy();
	}
};

bool copyFixtureConfig(const char * root)
{
	const bfs::path sourceRoot = bfs::path(__FILE__).parent_path().parent_path();
	const bfs::path destinationRoot = bfs::path(root) / "data" / "config";
	const std::array<bfs::path, 2> fixtureFiles = {
		bfs::path("filesystem.json"),
		bfs::path("schemas") / "settings.json",
	};

	boost::system::error_code error;
	for(const bfs::path & fixtureFile : fixtureFiles)
	{
		bfs::create_directories((destinationRoot / fixtureFile).parent_path(), error);
		if(error)
			return false;
		std::ifstream source((sourceRoot / "config" / fixtureFile).string(), std::ios::binary);
		std::ofstream destination((destinationRoot / fixtureFile).string(), std::ios::binary | std::ios::trunc);
		if(!source || !destination)
			return false;
		destination << source.rdbuf();
		if(source.bad() || !destination)
			return false;
	}
	return true;
}

ModalFocusScopeSpike makeScope()
{
	return ModalFocusScopeSpike(std::vector<Entry>{
		{"first", true, ""},
		{"unavailable", false, "Requires an unavailable capability"},
		{"third", true, ""},
	}, "first");
}

TEST(ModalFocusScopeSpikeTest, directionalShortcutsMoveFocusAfterDeviceMapping)
{
	auto scope = makeScope();

	// The current D-pad mapping and any future normalized stick mapping both
	// reach an ordinary window as MOVE_* shortcuts.
	EXPECT_EQ(scope.handleShortcut(EShortcut::MOVE_DOWN), EModalFocusScopeSpikeResult::FOCUS_CHANGED);
	EXPECT_EQ(scope.focusedId(), "unavailable");
	EXPECT_EQ(scope.handleShortcut(EShortcut::MOVE_RIGHT), EModalFocusScopeSpikeResult::FOCUS_CHANGED);
	EXPECT_EQ(scope.focusedId(), "third");
	EXPECT_EQ(scope.handleShortcut(EShortcut::MOVE_LEFT), EModalFocusScopeSpikeResult::FOCUS_CHANGED);
	EXPECT_EQ(scope.focusedId(), "unavailable");
}

TEST(ModalFocusScopeSpikeTest, cursorAxisShortcutDoesNotNavigateFocus)
{
	auto scope = makeScope();

	EXPECT_EQ(scope.handleShortcut(EShortcut::MOUSE_CURSOR_X), EModalFocusScopeSpikeResult::IGNORED);
	EXPECT_EQ(scope.focusedId(), "first");
	EXPECT_EQ(scope.handleShortcut(EShortcut::MOUSE_CURSOR_Y), EModalFocusScopeSpikeResult::IGNORED);
	EXPECT_EQ(scope.focusedId(), "first");
}

TEST(ModalFocusScopeSpikeTest, acceptReportsDisabledReasonAndCancelDoesNotAccept)
{
	auto scope = makeScope();
	scope.handleShortcut(EShortcut::MOVE_DOWN);

	EXPECT_EQ(scope.handleShortcut(EShortcut::GLOBAL_ACCEPT), EModalFocusScopeSpikeResult::DISABLED);
	EXPECT_EQ(scope.disabledReason(), "Requires an unavailable capability");
	EXPECT_EQ(scope.handleShortcut(EShortcut::GLOBAL_CANCEL), EModalFocusScopeSpikeResult::CANCELED);
}

TEST(ModalFocusScopeSpikeTest, stackedModalSuppressesInputAndRestoresStableFocusOnClose)
{
	auto background = makeScope();
	background.handleShortcut(EShortcut::MOVE_DOWN);
	background.suspend();

	auto modal = ModalFocusScopeSpike(std::vector<Entry>{{"child", true, ""}}, "child");
	EXPECT_EQ(background.handleShortcut(EShortcut::MOVE_DOWN), EModalFocusScopeSpikeResult::IGNORED);
	EXPECT_EQ(background.focusedId(), "unavailable");
	EXPECT_EQ(modal.handleShortcut(EShortcut::GLOBAL_ACCEPT), EModalFocusScopeSpikeResult::ACCEPTED);

	background.resume();
	EXPECT_EQ(background.focusedId(), "unavailable");
	EXPECT_EQ(background.handleShortcut(EShortcut::MOVE_DOWN), EModalFocusScopeSpikeResult::FOCUS_CHANGED);
	EXPECT_EQ(background.focusedId(), "third");
}

TEST(ModalFocusScopeSpikeTest, removedFocusFallsBackToInitialFocus)
{
	auto scope = makeScope();
	scope.handleShortcut(EShortcut::MOVE_DOWN);
	scope.replaceEntries(std::vector<Entry>{{"first", true, ""}, {"third", true, ""}});

	EXPECT_EQ(scope.focusedId(), "first");
}

TEST(ModalFocusScopeSpikeTest, mouseClickTakesFocusWithoutAccepting)
{
	auto scope = makeScope();

	EXPECT_TRUE(scope.focusFromMouse("third"));
	EXPECT_EQ(scope.focusedId(), "third");
	EXPECT_NE(scope.handleShortcut(EShortcut::MOVE_UP), EModalFocusScopeSpikeResult::ACCEPTED);
}

class CObjectListWindowModalFocusFixture : public ::testing::Test
{
protected:
	static void SetUpTestSuite()
	{
		ENGINE = ModalFocusScopeSpikeHarness::makeHeadlessEngine();
	}

	static void TearDownTestSuite()
	{
		ENGINE.reset();
	}

};

TEST_F(CObjectListWindowModalFocusFixture, actualWindowSynchronizesFocusMouseAndModalLifecycle)
{
	auto background = ModalFocusScopeSpikeHarness::makeWindow({"first", "second", "third"});
	background->activate();
	background->keyPressed(EShortcut::MOVE_DOWN);
	EXPECT_EQ(background->selected, 1u);
	EXPECT_EQ(ModalFocusScopeSpikeHarness::focusedId(*background), "1");
	EXPECT_EQ(ModalFocusScopeSpikeHarness::scrollPosition(*background), 1u);

	background->deactivate();
	EXPECT_TRUE(ModalFocusScopeSpikeHarness::isSuspended(*background));
	background->keyPressed(EShortcut::MOVE_DOWN);
	EXPECT_EQ(background->selected, 1u);

	auto modal = ModalFocusScopeSpikeHarness::makeWindow({"child"});
	modal->activate();
	modal->close();
	EXPECT_TRUE(modal->closed);

	background->activate();
	EXPECT_FALSE(ModalFocusScopeSpikeHarness::isSuspended(*background));
	EXPECT_EQ(ModalFocusScopeSpikeHarness::focusedId(*background), "1");

	ModalFocusScopeSpikeHarness::clickItem(*background, 2);
	EXPECT_EQ(background->selected, 2u);
	EXPECT_EQ(ModalFocusScopeSpikeHarness::focusedId(*background), "2");
}

TEST_F(CObjectListWindowModalFocusFixture, actualWindowRoutesAcceptCancelAndDisabledReason)
{
	auto selectable = ModalFocusScopeSpikeHarness::makeWindow({"first"});
	int acceptedSelection = -1;
	ModalFocusScopeSpikeHarness::setSelectionCallback(*selectable, [&acceptedSelection](int selected)
	{
		acceptedSelection = selected;
	});
	selectable->activate();
	ENGINE->events().dispatchShortcutPressed({EShortcut::GLOBAL_ACCEPT});
	ENGINE->events().dispatchShortcutReleased({EShortcut::GLOBAL_ACCEPT});
	EXPECT_TRUE(selectable->closed);
	EXPECT_EQ(acceptedSelection, 0);

	auto cancelable = ModalFocusScopeSpikeHarness::makeWindow({"first"});
	bool canceled = false;
	cancelable->onExit = [&canceled]()
	{
		canceled = true;
	};
	cancelable->activate();
	ENGINE->events().dispatchShortcutPressed({EShortcut::GLOBAL_CANCEL});
	ENGINE->events().dispatchShortcutReleased({EShortcut::GLOBAL_CANCEL});
	EXPECT_TRUE(cancelable->closed);
	EXPECT_TRUE(canceled);

	auto empty = ModalFocusScopeSpikeHarness::makeWindow({});
	empty->activate();
	ENGINE->events().dispatchShortcutPressed({EShortcut::GLOBAL_ACCEPT});
	ENGINE->events().dispatchShortcutReleased({EShortcut::GLOBAL_ACCEPT});
	EXPECT_FALSE(empty->closed);
	EXPECT_EQ(ModalFocusScopeSpikeHarness::disabledReason(*empty), "No selectable item is available");
}
}

int main(int argc, char * argv[])
{
	const char * const temporaryDirectory = std::getenv("TMPDIR");
	if(!temporaryDirectory || std::string(temporaryDirectory).empty())
		return 64;

	std::string rootTemplate = std::string(temporaryDirectory) + "/vcmi-modal-focus-spike.XXXXXX";
	std::vector<char> mutableRoot(rootTemplate.begin(), rootTemplate.end());
	mutableRoot.push_back('\0');
	if(!mkdtemp(mutableRoot.data()) || setenv("VCMI_USER_ROOT", mutableRoot.data(), 1) != 0 || !copyFixtureConfig(mutableRoot.data()))
		return 64;

	::testing::InitGoogleTest(&argc, argv);
	::testing::AddGlobalTestEnvironment(new ModalFocusScopeSpikeEnvironment);
	return RUN_ALL_TESTS();
}
