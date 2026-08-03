/*
 * ModalFocusScopeSpikeTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "../client/windows/ModalFocusScopeSpike.h"

#include <gtest/gtest.h>

namespace
{
using Entry = ModalFocusScopeSpike::Entry;

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
}

int main(int argc, char * argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
