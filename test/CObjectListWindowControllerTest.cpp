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
#include "../client/gui/CursorHandler.h"
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
#include "../client/widgets/Buttons.h"
#include "../client/widgets/TextControls.h"
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

#include <fstream>

#include <SDL.h>
#include <SDL_gamecontroller.h>
#include <SDL_joystick.h>
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

class CursorFallbackWindow final : public CWindowObject
{
public:
	CursorFallbackWindow()
		: CWindowObject(CWindowObject::HeadlessTestTag())
	{
	}
};

class CObjectListWindowControllerTest : public testing::Test
{
protected:
	bool resourceHandlerInitialized = false;
	Uint32 initializedSdlSubsystems = 0;
	std::vector<int> virtualControllerDeviceIndices;
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
		ENGINE->input().gameControllerHandler.reset();
		for(const int deviceIndex : virtualControllerDeviceIndices)
			EXPECT_EQ(SDL_JoystickDetachVirtual(deviceIndex), 0);
		ENGINE.reset();
		if(initializedSdlSubsystems)
			SDL_QuitSubSystem(initializedSdlSubsystems);
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
		keyBindingsConfig.init("config/keyBindingsConfig.json", "");
		ENGINE->shortcuts().reloadShortcuts();

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

		JsonNode bigFontConfig;
		bigFontConfig["file"].String() = "NotoSerif-Bold.ttf";
		bigFontConfig["size"].Integer() = 18;
		auto bigFont = std::make_shared<FontChain>();
		bigFont->addTrueTypeFont(bigFontConfig, true);
		renderer.fonts[FONT_BIG] = bigFont;

		JsonNode smallFontConfig;
		smallFontConfig["file"].String() = "NotoSerif-Medium.ttf";
		smallFontConfig["size"].Integer() = 11;
		auto smallFont = std::make_shared<FontChain>();
		smallFont->addTrueTypeFont(smallFontConfig, true);
		renderer.fonts[FONT_SMALL] = smallFont;

		JsonNode tinyFontConfig;
		tinyFontConfig["file"].String() = "NotoSans-Medium.ttf";
		tinyFontConfig["size"].Integer() = 9;
		tinyFontConfig["noShadow"].Bool() = true;
		auto tinyFont = std::make_shared<FontChain>();
		tinyFont->addTrueTypeFont(tinyFontConfig, true);
		renderer.fonts[FONT_TINY] = tinyFont;

		addProductionImage(renderer, "TPGATE", EImageBlitMode::OPAQUE, Point(320, 460));
		addProductionImage(renderer, "TPGATES", EImageBlitMode::COLORKEY, Point(256, 25));
		addProductionImage(renderer, "TownPortalBackgroundBlue", EImageBlitMode::OPAQUE, Point(320, 460));
		auto icon = addProductionImage(renderer, "controller-list-test-icon", EImageBlitMode::OPAQUE, Point(35, 23));
		addProductionImage(renderer, "controller-list-test-button", EImageBlitMode::COLORKEY, Point(54, 30));
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

	std::string acceptGlyphText(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->acceptGlyph->getText();
	}

	std::string cancelGlyphText(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->cancelGlyph->getText();
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

	void setControllerPresentation(ControllerPresentation presentation)
	{
		ENGINE->input().gameControllerHandler = std::unique_ptr<InputSourceGameController>(
			new InputSourceGameController(InputSourceGameController::HeadlessTestTag()));
		ENGINE->input().gameControllerHandler->activePresentation = presentation;
	}

	SDL_JoystickID attachVirtualDualSense()
	{
		const Uint32 requiredSubsystems = SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER;
		initializedSdlSubsystems = requiredSubsystems & ~SDL_WasInit(requiredSubsystems);
		if(initializedSdlSubsystems && SDL_InitSubSystem(initializedSdlSubsystems) != 0)
			throw std::runtime_error(SDL_GetError());

		SDL_VirtualJoystickDesc descriptor{};
		descriptor.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
		descriptor.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
		descriptor.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
		descriptor.vendor_id = 1356;
		descriptor.product_id = 3302;
		descriptor.name = "VCMI test DualSense";

		const int deviceIndex = SDL_JoystickAttachVirtualEx(&descriptor);
		if(deviceIndex < 0)
			throw std::runtime_error(SDL_GetError());
		virtualControllerDeviceIndices.push_back(deviceIndex);

		char guid[33]{};
		SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(deviceIndex), guid, sizeof(guid));
		const std::string mapping = std::string(guid)
			+ ",VCMI test DualSense,a:b0,b:b1,dpdown:b12,platform:Mac OS X,type:ps5";
		if(SDL_GameControllerAddMapping(mapping.c_str()) < 0)
			throw std::runtime_error(SDL_GetError());
		if(!SDL_IsGameController(deviceIndex))
			throw std::runtime_error("Virtual DualSense is not a game controller.");

		SDL_Joystick * joystick = SDL_JoystickOpen(deviceIndex);
		if(!joystick)
			throw std::runtime_error(SDL_GetError());
		const SDL_JoystickID instanceId = SDL_JoystickInstanceID(joystick);
		SDL_JoystickClose(joystick);
		if(instanceId < 0)
			throw std::runtime_error(SDL_GetError());

		auto & testSettings = const_cast<JsonNode &>(settings.toJsonNode());
		testSettings["input"]["enableController"].Bool() = true;
		ENGINE->inputHandlerInstance = std::make_unique<InputHandler>();
		return instanceId;
	}

	void dispatchVirtualControllerButton(SDL_JoystickID instanceId, SDL_GameControllerButton button)
	{
		SDL_Event event{};
		event.type = SDL_CONTROLLERBUTTONDOWN;
		event.cbutton.type = SDL_CONTROLLERBUTTONDOWN;
		event.cbutton.which = instanceId;
		event.cbutton.button = static_cast<Uint8>(button);
		event.cbutton.state = SDL_PRESSED;
		if(SDL_PushEvent(&event) != 1)
			throw std::runtime_error(SDL_GetError());

		ENGINE->input().fetchEvents();
		ENGINE->input().processEvents();
	}

	SDL_GameControllerType openedControllerType(SDL_JoystickID instanceId) const
	{
		const auto & controllers = ENGINE->input().gameControllerHandler->gameControllerMap;
		const auto found = controllers.find(instanceId);
		if(found == controllers.end())
			return SDL_CONTROLLER_TYPE_UNKNOWN;
		return SDL_GameControllerGetType(found->second.get());
	}

	void drawGlyphForCalibration(const std::shared_ptr<CObjectListWindow> & window, Canvas & canvas, bool accept) const
	{
		(accept ? window->acceptGlyph : window->cancelGlyph)->showAll(canvas);
	}

	std::pair<Point, Point> glyphCalibrationAnchors(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return {window->acceptGlyph->pos.topLeft(), window->cancelGlyph->pos.topLeft()};
	}

	bool pixelsDiffer(const ColorRGBA & before, const ColorRGBA & after) const
	{
		return before.r != after.r || before.g != after.g || before.b != after.b || before.a != after.a;
	}

	std::vector<Point> changedPixelMask(const Canvas & before, const Canvas & after, const Rect & region) const
	{
		std::vector<Point> result;
		for(int y = region.y; y < region.bottom(); ++y)
		{
			for(int x = region.x; x < region.right(); ++x)
			{
				const Point position(x, y);
				if(pixelsDiffer(before.getPixel(position), after.getPixel(position)))
					result.push_back(position);
			}
		}
		return result;
	}

	size_t composedGlyphDeltaPixels(const Canvas & withoutGlyphs, const Canvas & withGlyphs, const std::vector<Point> & mask) const
	{
		size_t result = 0;
		for(const auto & position : mask)
		{
			if(pixelsDiffer(withoutGlyphs.getPixel(position), withGlyphs.getPixel(position)))
				++result;
		}
		return result;
	}

	struct GlyphCompositionState
	{
		Point windowOrigin;
		Point acceptGlyphOrigin;
		Point cancelGlyphOrigin;
		Point acceptButtonOrigin;
		Point cancelButtonOrigin;
		InputMode inputMode;
		ControllerPresentation presentation;
		std::optional<size_t> focusedItem;
		std::optional<size_t> selectedItem;
		bool controllerFocusVisible;
		size_t windowStackSize;
		bool isTopWindow;
	};

	GlyphCompositionState glyphCompositionState(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return {
			window->pos.topLeft(),
			window->acceptGlyph->pos.topLeft(),
			window->cancelGlyph->pos.topLeft(),
			window->ok->pos.topLeft(),
			window->exit->pos.topLeft(),
			ENGINE->input().getCurrentInputMode(),
			ENGINE->input().getControllerPresentation(),
			window->focusedItem,
			window->selectedItem,
			window->controllerFocusVisible,
			ENGINE->windows().count(),
			ENGINE->windows().isTopWindow(window)
		};
	}

	void expectSameGlyphCompositionState(const GlyphCompositionState & expected, const GlyphCompositionState & actual) const
	{
		EXPECT_EQ(actual.windowOrigin, expected.windowOrigin);
		EXPECT_EQ(actual.acceptGlyphOrigin, expected.acceptGlyphOrigin);
		EXPECT_EQ(actual.cancelGlyphOrigin, expected.cancelGlyphOrigin);
		EXPECT_EQ(actual.acceptButtonOrigin, expected.acceptButtonOrigin);
		EXPECT_EQ(actual.cancelButtonOrigin, expected.cancelButtonOrigin);
		EXPECT_EQ(actual.inputMode, expected.inputMode);
		EXPECT_EQ(actual.presentation, expected.presentation);
		EXPECT_EQ(actual.focusedItem, expected.focusedItem);
		EXPECT_EQ(actual.selectedItem, expected.selectedItem);
		EXPECT_EQ(actual.controllerFocusVisible, expected.controllerFocusVisible);
		EXPECT_EQ(actual.windowStackSize, expected.windowStackSize);
		EXPECT_EQ(actual.isTopWindow, expected.isTopWindow);
	}

	void setGlyphTexts(const std::shared_ptr<CObjectListWindow> & window, const std::string & acceptText, const std::string & cancelText) const
	{
		window->acceptGlyph->setText(acceptText);
		window->cancelGlyph->setText(cancelText);
	}

	bool glyphsComposeAfterActionControls(const std::shared_ptr<CObjectListWindow> & window) const
	{
		const auto acceptGlyph = std::find(window->children.begin(), window->children.end(), window->acceptGlyph.get());
		const auto cancelGlyph = std::find(window->children.begin(), window->children.end(), window->cancelGlyph.get());
		const auto acceptButton = std::find(window->children.begin(), window->children.end(), window->ok.get());
		const auto cancelButton = std::find(window->children.begin(), window->children.end(), window->exit.get());
		return acceptGlyph > acceptButton && cancelGlyph > cancelButton;
	}

	void writeRenderedGlyphArtifact(const Canvas & canvas, const Point & dimensions, bool candidateComposition) const
	{
		const std::string filename = candidateComposition
			? "../../evidence/m2-rendered-glyph-mask-candidate.ppm"
			: "../../evidence/m2-rendered-glyph-mask-baseline.ppm";
		std::ofstream output(filename, std::ios::binary | std::ios::trunc);
		if(!output)
			throw std::runtime_error("Unable to create rendered glyph artifact: " + filename);

		output << "P6\n" << dimensions.x << ' ' << dimensions.y << "\n255\n";
		for(int y = 0; y < dimensions.y; ++y)
		{
			for(int x = 0; x < dimensions.x; ++x)
			{
				const auto pixel = canvas.getPixel(Point(x, y));
				output.put(static_cast<char>(pixel.r));
				output.put(static_cast<char>(pixel.g));
				output.put(static_cast<char>(pixel.b));
			}
		}
		if(!output)
			throw std::runtime_error("Unable to write rendered glyph artifact: " + filename);
		std::cout << "Rendered glyph artifact: " << filename << std::endl;
	}

	void initializeCursorPresentation()
	{
		ENGINE->cursorHandlerInstance = std::make_unique<CursorHandler>();
		ENGINE->cursor().show();
	}

	bool isCursorShown() const
	{
		return ENGINE->cursor().showing;
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

TEST_F(CObjectListWindowControllerTest, ControllerFocusHidesCursorUntilMouseTakeoverAndScopeRelease)
{
	initializeProductionListConstruction();
	initializeCursorPresentation();

	auto fallback = std::make_shared<CursorFallbackWindow>();
	auto window = std::make_shared<CObjectListWindow>(
		std::vector<CObjectListWindow::ListItem>{{"Enabled", true, ""}, {"Disabled", false, "Already selected"}},
		nullptr,
		"Add spell",
		"Select a spell",
		[](int)
		{
		},
		0,
		std::vector<std::shared_ptr<IImage>>{productionListIcon, productionListIcon},
		true,
		true);

	ENGINE->windows().pushWindow(fallback);
	ENGINE->windows().pushWindow(window);

	EXPECT_TRUE(isControllerFocusVisible(window));
	EXPECT_FALSE(isCursorShown());

	setInputMode(InputMode::KEYBOARD_AND_MOUSE);
	clickItem(window, 1);
	EXPECT_FALSE(isControllerFocusVisible(window));
	EXPECT_TRUE(isCursorShown());

	setInputMode(InputMode::CONTROLLER);
	ENGINE->events().dispatchShortcutPressed({EShortcut::MOVE_DOWN});
	EXPECT_TRUE(isControllerFocusVisible(window));
	EXPECT_FALSE(isCursorShown());

	ENGINE->windows().popWindow(window);
	EXPECT_TRUE(ENGINE->windows().isTopWindow(fallback));
	EXPECT_TRUE(isCursorShown());
}

TEST_F(ShortcutGlyphQueryTest, DualSenseBindingsRefreshGlyphsAfterControllerActivation)
{
	initializeProductionListConstruction();
	setJoystickBindings({
		{"a", EShortcut::GLOBAL_ACCEPT},
		{"b", EShortcut::GLOBAL_CANCEL}
	});

	const auto acceptBindings = ENGINE->shortcuts().getJoystickBindings(EShortcut::GLOBAL_ACCEPT);
	const auto cancelBindings = ENGINE->shortcuts().getJoystickBindings(EShortcut::GLOBAL_CANCEL);
	ASSERT_FALSE(acceptBindings.empty());
	ASSERT_FALSE(cancelBindings.empty());

	auto window = std::make_shared<CObjectListWindow>(
		std::vector<CObjectListWindow::ListItem>{{"Spell", true, ""}},
		nullptr,
		"Add spell",
		"Select a spell",
		[](int)
		{
		},
		0,
		std::vector<std::shared_ptr<IImage>>{productionListIcon},
		true,
		true);

	EXPECT_EQ(ENGINE->input().getControllerPresentation(), ControllerPresentation::UNKNOWN);
	EXPECT_FALSE(ENGINE->input().getControllerGlyphToken(acceptBindings));
	EXPECT_FALSE(ENGINE->input().getControllerGlyphToken(cancelBindings));
	EXPECT_EQ(acceptGlyphText(window), "");
	EXPECT_EQ(cancelGlyphText(window), "");

	setControllerPresentation(ControllerPresentation::DUALSENSE);
	EXPECT_EQ(ENGINE->input().getControllerPresentation(), ControllerPresentation::DUALSENSE);
	EXPECT_EQ(ENGINE->input().getControllerGlyphToken(acceptBindings), "×");
	EXPECT_EQ(ENGINE->input().getControllerGlyphToken(cancelBindings), "○");

	ENGINE->windows().pushWindow(window);
	ENGINE->events().dispatchShortcutPressed({EShortcut::MOVE_DOWN});

	EXPECT_EQ(acceptGlyphText(window), "×");
	EXPECT_EQ(cancelGlyphText(window), "○");
}

TEST_F(CObjectListWindowControllerTest, DualSenseGlyphsRenderAboveProductionActionControls)
{
	initializeProductionListConstruction();

	const auto acceptBindings = ENGINE->shortcuts().getJoystickBindings(EShortcut::GLOBAL_ACCEPT);
	const auto cancelBindings = ENGINE->shortcuts().getJoystickBindings(EShortcut::GLOBAL_CANCEL);
	ASSERT_EQ(acceptBindings, std::vector<std::string>({"a"}));
	ASSERT_EQ(cancelBindings, std::vector<std::string>({"b"}));

	auto window = std::make_shared<CObjectListWindow>(
		std::vector<CObjectListWindow::ListItem>{{"Enabled", true, ""}, {"Disabled", false, "Already selected"}},
		nullptr,
		"Add spell",
		"Select a spell",
		[](int)
		{
		},
		0,
		std::vector<std::shared_ptr<IImage>>{productionListIcon, productionListIcon},
		true,
		true);

	const Point canvasDimensions = window->pos.dimensions();
	ASSERT_EQ(canvasDimensions, Point(320, 460));
	window->moveTo(Point(), true);
	ASSERT_EQ(window->pos.topLeft(), Point());
	const auto [acceptGlyphAnchor, cancelGlyphAnchor] = glyphCalibrationAnchors(window);
	ASSERT_EQ(acceptGlyphAnchor, Point(42, 414));
	ASSERT_EQ(cancelGlyphAnchor, Point(254, 414));
	ENGINE->windows().pushWindow(window);
	ASSERT_TRUE(acceptGlyphText(window).empty());
	ASSERT_TRUE(cancelGlyphText(window).empty());
	const SDL_JoystickID controllerInstance = attachVirtualDualSense();
	dispatchVirtualControllerButton(controllerInstance, SDL_CONTROLLER_BUTTON_DPAD_DOWN);

	ASSERT_EQ(openedControllerType(controllerInstance), SDL_CONTROLLER_TYPE_PS5);
	ASSERT_EQ(ENGINE->input().getCurrentInputMode(), InputMode::CONTROLLER);
	ASSERT_EQ(ENGINE->input().getControllerPresentation(), ControllerPresentation::DUALSENSE);
	const auto acceptToken = ENGINE->input().getControllerGlyphToken(acceptBindings);
	const auto cancelToken = ENGINE->input().getControllerGlyphToken(cancelBindings);
	ASSERT_TRUE(acceptToken.has_value());
	ASSERT_TRUE(cancelToken.has_value());
	ASSERT_EQ(acceptGlyphText(window), *acceptToken);
	ASSERT_EQ(cancelGlyphText(window), *cancelToken);

	const Rect calibrationArea(Point(), canvasDimensions);
	Canvas calibrationBefore(canvasDimensions, CanvasScalingPolicy::IGNORE);
	Canvas calibrationAccept(canvasDimensions, CanvasScalingPolicy::IGNORE);
	ASSERT_TRUE(changedPixelMask(calibrationBefore, calibrationAccept, calibrationArea).empty());
	drawGlyphForCalibration(window, calibrationAccept, true);
	const auto acceptMask = changedPixelMask(calibrationBefore, calibrationAccept, calibrationArea);
	ASSERT_FALSE(acceptMask.empty());

	Canvas calibrationCancel(canvasDimensions, CanvasScalingPolicy::IGNORE);
	ASSERT_TRUE(changedPixelMask(calibrationBefore, calibrationCancel, calibrationArea).empty());
	drawGlyphForCalibration(window, calibrationCancel, false);
	const auto cancelMask = changedPixelMask(calibrationBefore, calibrationCancel, calibrationArea);
	ASSERT_FALSE(cancelMask.empty());

	const auto stateWithGlyphs = glyphCompositionState(window);
	setGlyphTexts(window, "", "");
	ASSERT_TRUE(acceptGlyphText(window).empty());
	ASSERT_TRUE(cancelGlyphText(window).empty());
	Canvas withoutGlyphs(canvasDimensions, CanvasScalingPolicy::IGNORE);
	window->CWindowObject::showAll(withoutGlyphs);
	ASSERT_TRUE(acceptGlyphText(window).empty());
	ASSERT_TRUE(cancelGlyphText(window).empty());
	expectSameGlyphCompositionState(stateWithGlyphs, glyphCompositionState(window));

	setGlyphTexts(window, *acceptToken, *cancelToken);
	ASSERT_EQ(acceptGlyphText(window), *acceptToken);
	ASSERT_EQ(cancelGlyphText(window), *cancelToken);
	Canvas withGlyphs(canvasDimensions, CanvasScalingPolicy::IGNORE);
	window->showAll(withGlyphs);
	ASSERT_EQ(acceptGlyphText(window), *acceptToken);
	ASSERT_EQ(cancelGlyphText(window), *cancelToken);
	expectSameGlyphCompositionState(stateWithGlyphs, glyphCompositionState(window));

	const size_t preservedAcceptMask = composedGlyphDeltaPixels(withoutGlyphs, withGlyphs, acceptMask);
	const size_t preservedCancelMask = composedGlyphDeltaPixels(withoutGlyphs, withGlyphs, cancelMask);
	writeRenderedGlyphArtifact(withGlyphs, canvasDimensions, glyphsComposeAfterActionControls(window));
	EXPECT_EQ(preservedAcceptMask, acceptMask.size())
		<< "Composed accept action control does not preserve the complete binding-derived glyph mask.";
	EXPECT_EQ(preservedCancelMask, cancelMask.size())
		<< "Composed cancel action control does not preserve the complete binding-derived glyph mask.";
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
