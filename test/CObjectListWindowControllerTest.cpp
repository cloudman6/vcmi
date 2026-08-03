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
#include "../client/GameInstance.h"
#include "../client/eventsSDL/InputHandler.h"
#include "../client/gui/EventDispatcher.h"
#include "../client/gui/Shortcut.h"
#include "../client/gui/ShortcutHandler.h"
#include "../client/gui/WindowHandler.h"
#include "../client/lobby/BattleOnlyModeTab.h"
#include "../client/render/Canvas.h"
#include "../client/render/EFont.h"
#include "../client/renderSDL/FontChain.h"
#include "../client/renderSDL/RenderHandler.h"
#include "../client/renderSDL/SDLImage.h"
#include "../client/renderSDL/ScalableImage.h"
#include "../client/renderSDL/ScreenHandler.h"
#include "../client/render/hdEdition/HdImageLoader.h"
#include "../client/widgets/ObjectLists.h"
#include "../client/windows/GUIClasses.h"
#include "../client/eventsSDL/InputSourceGameController.h"
#include "../lib/CConfigHandler.h"
#include "../lib/GameLibrary.h"
#include "../lib/filesystem/CFilesystemLoader.h"
#include "../lib/filesystem/Filesystem.h"
#include "../lib/modding/IdentifierStorage.h"
#include "../lib/modding/ModScope.h"
#include "../lib/spells/CSpellHandler.h"
#include "../lib/spells/SpellSchoolHandler.h"
#include "../lib/texts/CGeneralTextHandler.h"

#include <gtest/gtest.h>

#include <SDL_surface.h>

[[noreturn]] void handleFatalError(const std::string & message, bool)
{
	throw std::runtime_error(message);
}

class GlyphRefreshProbeWindow final : public CObjectListWindow
{
public:
	using CObjectListWindow::CObjectListWindow;

	void redraw() override
	{
		if(redrawCanvas)
			showAll(*redrawCanvas);
	}

	void showAll(Canvas & to) override
	{
		++showAllCount;
		if(showAllCount == 1)
			CObjectListWindow::showAll(to);
	}

	Canvas * redrawCanvas = nullptr;
	size_t showAllCount = 0;
};

class CObjectListWindowControllerTest : public testing::Test
{
protected:
	bool resourceHandlerInitialized = false;
	std::vector<std::shared_ptr<ScalableImageShared>> productionImages;
	std::unique_ptr<GameLibrary> localizationLibrary;

	struct BattleOnlyAddPayloadObservation
	{
		std::shared_ptr<const std::vector<SpellID>> callbackValues;
		std::vector<CObjectListWindow::ListItem> items;
		std::vector<std::shared_ptr<IImage>> images;
		bool searchBoxEnabled = false;
		bool blue = false;
		const std::vector<SpellID> * callbackCaptureIdentity = nullptr;
	};

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
		GAME.reset();
		ENGINE.reset();
		localizationLibrary.reset();
		LIBRARY = nullptr;
		if(resourceHandlerInitialized)
			CResourceHandler::destroy();
	}

	std::shared_ptr<IImage> addProductionImage(
		RenderHandler & renderer, const std::string & name, EImageBlitMode mode, const Point & dimensions)
	{
		auto surface = SDL_CreateRGBSurfaceWithFormat(0, dimensions.x, dimensions.y, 32, SDL_PIXELFORMAT_ARGB8888);
		if(!surface)
			throw std::runtime_error(SDL_GetError());

		SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 100, 100, 100, 255));
		auto baseImage = std::make_shared<SDLImageShared>(surface);
		SDL_FreeSurface(surface);

		ImageLocator locator(ImagePath::builtin(name), mode);
		locator.scalingFactor = ENGINE->screenHandler().getScalingFactor();
		auto image = std::make_shared<ScalableImageShared>(locator, baseImage);
		productionImages.push_back(image);
		renderer.storeCachedImage(locator, image);
		return image->createImageReference();
	}

	void addProductionAnimation(RenderHandler & renderer, const std::string & name)
	{
		auto & frames = renderer.animationLayouts[AnimationPath::builtin("SPRITES/" + name)][0];
		frames.assign(4, ImageLocator(ImagePath::builtin("controller-list-test-button"), EImageBlitMode::COLORKEY));
	}

	void addProductionSpellIcons(RenderHandler & renderer)
	{
		auto & frames = renderer.animationLayouts[AnimationPath::builtin("SpellInt")][0];
		frames.assign(
			GameConstants::SPELLS_QUANTITY + 1,
			ImageLocator(ImagePath::builtin("controller-list-test-icon"), EImageBlitMode::OPAQUE));
	}

	void initializeProductionListConstruction()
	{
		CResourceHandler::initialize();
		resourceHandlerInitialized = true;

		const auto sourceRoot = boost::filesystem::path(__FILE__).parent_path().parent_path();
		CResourceHandler::addFilesystem(
			"data",
			"controller-list-test-fonts",
			std::make_unique<CFilesystemLoader>("Data/", sourceRoot / "Mods" / "vcmi" / "Content" / "Data", 0));
		CResourceHandler::addFilesystem(
			"data",
			"controller-list-test-translations",
			std::make_unique<CFilesystemLoader>("config/", sourceRoot / "Mods" / "vcmi" / "Content" / "config", 1));

		localizationLibrary = std::make_unique<GameLibrary>();
		LIBRARY = localizationLibrary.get();
		localizationLibrary->generaltexth = std::unique_ptr<CGeneralTextHandler>(
			new CGeneralTextHandler(CGeneralTextHandler::TestConstructionTag()));
		const JsonNode englishTranslation(JsonPath::builtin("config/translations/english.json"));
		localizationLibrary->generaltexth->loadTranslationOverrides("vcmi", "english", englishTranslation);

		ENGINE->screenHandlerInstance = std::make_unique<ScreenHandler>();
		ENGINE->renderHandlerInstance = std::make_unique<RenderHandler>();
		auto & renderer = static_cast<RenderHandler &>(ENGINE->renderHandler());
		renderer.hdImageLoader = std::make_shared<HdImageLoader>();

		JsonNode fontConfig;
		fontConfig["file"].String() = "NotoSerif-Medium.ttf";
		fontConfig["size"].Integer() = 12;
		auto font = std::make_shared<FontChain>();
		font->addTrueTypeFont(fontConfig, true);
		renderer.fonts[FONT_BIG] = font;
		renderer.fonts[FONT_SMALL] = font;
		renderer.fonts[FONT_TINY] = font;

		addProductionImage(renderer, "TPGATE", EImageBlitMode::OPAQUE, Point(320, 460));
		addProductionImage(renderer, "TPGATES", EImageBlitMode::COLORKEY, Point(256, 25));
		addProductionImage(renderer, "TownPortalBackgroundBlue", EImageBlitMode::OPAQUE, Point(320, 460));
		auto icon = addProductionImage(renderer, "controller-list-test-icon", EImageBlitMode::OPAQUE, Point(35, 23));
		addProductionImage(renderer, "controller-list-test-button", EImageBlitMode::COLORKEY, Point(20, 20));
		addProductionAnimation(renderer, "ICANCEL.DEF");
		addProductionAnimation(renderer, "IOKAY.DEF");
		addProductionAnimation(renderer, "MuBcanc");
		addProductionAnimation(renderer, "MuBchck");
		addProductionAnimation(renderer, "SCNRBUP.DEF");
		addProductionAnimation(renderer, "SCNRBDN.DEF");
		addProductionAnimation(renderer, "SCNRBSL.DEF");
		addProductionSpellIcons(renderer);

		GAME = std::make_unique<GameInstance>();
		productionListIcon = std::move(icon);
	}

	void initializeProductionSpellData()
	{
		localizationLibrary->identifiersHandler = std::make_unique<CIdentifierStorage>();
		localizationLibrary->spellh = std::make_unique<CSpellHandler>();
		localizationLibrary->spellSchoolHandler = std::make_unique<SpellSchoolHandler>();

		JsonNode school;
		school["name"].String() = "Test school";
		school.setModScope(ModScope::scopeBuiltin());
		localizationLibrary->spellSchoolHandler->loadObject(ModScope::scopeBuiltin(), "testSchool", school);

		for(size_t index = 0; index < GameConstants::SPELLS_QUANTITY; ++index)
		{
			JsonNode spell;
			spell["type"].String() = "combat";
			spell["name"].String() = "Combat spell " + std::to_string(index);
			spell["level"].Integer() = static_cast<si32>(index / 14 + 1);
			spell["power"].Integer() = 1;
			spell["targetType"].String() = "NO_TARGET";
			spell["flags"]["indifferent"].Bool() = true;
			spell["school"]["testSchool"].Bool() = true;
			spell.setModScope(ModScope::scopeBuiltin());
			localizationLibrary->spellh->loadObject(
				ModScope::scopeBuiltin(), "testCombatSpell" + std::to_string(index), spell);
		}

		localizationLibrary->identifiersHandler->finalize();
	}

	BattleOnlyAddPayloadObservation observeBattleOnlyAddPayload(
		const std::vector<SpellID> & allSpells,
		const std::vector<SpellID> & selectedSpells)
	{
		auto payload = BattleOnlyModeHeroSelector::prepareAddSpellList(allSpells, selectedSpells);
		const auto * callbackCaptureIdentity = payload.values.get();
		return {
			std::move(payload.values),
			std::move(payload.items),
			std::move(payload.images),
			payload.searchBoxEnabled,
			payload.blue,
			callbackCaptureIdentity
		};
	}

	std::shared_ptr<IImage> productionListIcon;

	bool hasProductionListWindowTree(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->list && window->ok && window->exit;
	}

	size_t productionListItemCount(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->list->size();
	}

	bool hasProductionListItem(const std::shared_ptr<CObjectListWindow> & window, size_t index) const
	{
		return window->list->getItem(index) != nullptr;
	}

	bool hasProductionSearchBox(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->searchBox && window->searchBoxDescription && window->searchBoxRectangle;
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

TEST_F(CObjectListWindowControllerTest, ProductionBattleOnlyAddConstructorBuildsAndDestroysWindowTree)
{
	initializeProductionListConstruction();

	constexpr size_t combatSpellCount = GameConstants::SPELLS_QUANTITY;
	std::vector<CObjectListWindow::ListItem> items;
	std::vector<std::shared_ptr<IImage>> images;
	items.reserve(combatSpellCount);
	images.reserve(combatSpellCount);
	const auto disabledReason = localizationLibrary->generaltexth->translate(
		"vcmi.lobby.battleOnlySpellAlreadySelected");
	ASSERT_FALSE(disabledReason.empty());

	for(size_t index = 0; index < combatSpellCount; ++index)
	{
		const bool enabled = index % 5 != 4;
		items.push_back({"Combat spell " + std::to_string(index), enabled, enabled ? "" : disabledReason});
		images.push_back(productionListIcon);
	}

	const auto disabledCount = std::count_if(items.begin(), items.end(), [](const auto & item)
	{
		return !item.enabled;
	});

	EXPECT_EQ(combatSpellCount, 70);
	EXPECT_EQ(disabledCount, 14);
	EXPECT_EQ(items.size() - disabledCount, 56);

	{
		auto window = std::make_shared<CObjectListWindow>(
			items, nullptr, "Add spell", "Select a spell", [](int)
			{
			}, 0, images, true, true);

		EXPECT_GT(window->pos.w, 0);
		EXPECT_GT(window->pos.h, 0);
		EXPECT_TRUE(hasProductionListWindowTree(window));
		EXPECT_EQ(productionListItemCount(window), items.size());
		EXPECT_TRUE(hasProductionListItem(window, 0));
		EXPECT_FALSE(hasProductionListItem(window, items.size() - 1));
		EXPECT_TRUE(hasProductionSearchBox(window));

		ENGINE->windows().pushWindow(window);
		EXPECT_TRUE(ENGINE->windows().isTopWindow(window));
		for(size_t index = 0; index < items.size() - 1; ++index)
			ENGINE->events().dispatchShortcutPressed({EShortcut::MOVE_DOWN});

		EXPECT_EQ(focusedItem(window), items.size() - 1);
		EXPECT_EQ(listPosition(window), items.size() - 9);
		EXPECT_TRUE(hasProductionListItem(window, items.size() - 1));
		EXPECT_EQ(this->disabledReason(window, items.size() - 1), disabledReason);
		ENGINE->windows().popWindow(window);
	}
}

TEST_F(CObjectListWindowControllerTest, ControllerGlyphRefreshDoesNotReenterShowAll)
{
	initializeProductionListConstruction();

	std::vector<CObjectListWindow::ListItem> items{{"Spell", true, ""}};
	std::vector<std::shared_ptr<IImage>> images{productionListIcon};
	auto window = std::make_shared<GlyphRefreshProbeWindow>(
		items, nullptr, "Add spell", "Select a spell", [](int)
		{
		}, 0, images, true, true);

	ENGINE->windows().pushWindow(window);
	auto surface = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>(
		SDL_CreateRGBSurfaceWithFormat(0, 320, 460, 32, SDL_PIXELFORMAT_ARGB8888), SDL_FreeSurface);
	ASSERT_NE(surface, nullptr);
	Canvas canvas = Canvas::createFromSurface(surface.get(), CanvasScalingPolicy::AUTO);
	window->redrawCanvas = &canvas;
	window->showAll(canvas);

	EXPECT_EQ(window->showAllCount, 1);
	window->redrawCanvas = nullptr;
	ENGINE->windows().popWindow(window);
}

TEST_F(CObjectListWindowControllerTest, ProductionBattleOnlyAddPayloadPreparesControlledSpells)
{
	initializeProductionListConstruction();
	initializeProductionSpellData();

	std::vector<SpellID> selectedSpells;
	for(size_t index = 4; index < GameConstants::SPELLS_QUANTITY; index += 5)
		selectedSpells.emplace_back(static_cast<si32>(index));

	const auto allowedSpells = localizationLibrary->spellh->getDefaultAllowed();
	const std::vector<SpellID> allSpells(allowedSpells.begin(), allowedSpells.end());
	auto payload = observeBattleOnlyAddPayload(allSpells, selectedSpells);
	const auto disabledReason = localizationLibrary->generaltexth->translate(
		"vcmi.lobby.battleOnlySpellAlreadySelected");

	ASSERT_FALSE(disabledReason.empty());
	ASSERT_NE(payload.callbackValues, nullptr);
	ASSERT_NE(payload.callbackCaptureIdentity, nullptr);
	EXPECT_EQ(payload.callbackValues.get(), payload.callbackCaptureIdentity);
	EXPECT_EQ(payload.callbackValues->size(), GameConstants::SPELLS_QUANTITY);
	EXPECT_EQ(payload.items.size(), payload.callbackValues->size());
	EXPECT_EQ(payload.images.size(), payload.callbackValues->size());
	EXPECT_TRUE(payload.searchBoxEnabled);
	EXPECT_TRUE(payload.blue);

	const auto disabledCount = std::count_if(payload.items.begin(), payload.items.end(), [](const auto & item)
	{
		return !item.enabled;
	});
	EXPECT_EQ(disabledCount, selectedSpells.size());
	EXPECT_EQ(payload.items.size() - disabledCount, 56);

	for(size_t index = 0; index < payload.items.size(); ++index)
	{
		const auto spell = payload.callbackValues->at(index);
		const bool selected = vstd::contains(selectedSpells, spell);
		EXPECT_EQ(payload.items[index].text, spell.toSpell()->getNameTranslated());
		EXPECT_EQ(payload.items[index].enabled, !selected);
		EXPECT_EQ(payload.items[index].disabledReason, selected ? disabledReason : "");
		EXPECT_NE(payload.images[index], nullptr);
	}

	auto callbackValues = payload.callbackValues;
	const auto capturedSpell = callbackValues->at(4);
	payload = {};
	EXPECT_EQ(callbackValues->size(), GameConstants::SPELLS_QUANTITY);
	EXPECT_EQ(callbackValues->at(4), capturedSpell);
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
