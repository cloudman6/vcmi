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
#include "../client/gui/InterfaceObjectConfigurable.h"
#include "../client/gui/Shortcut.h"
#include "../client/gui/ShortcutHandler.h"
#include "../client/gui/WindowHandler.h"
#include "../client/lobby/BattleOnlyModeTab.h"
#include "../client/render/Canvas.h"
#include "../client/render/EFont.h"
#include "../client/render/IFont.h"
#include "../client/renderSDL/RenderHandler.h"
#include "../client/renderSDL/SDLImage.h"
#include "../client/renderSDL/ScalableImage.h"
#include "../client/renderSDL/ScreenHandler.h"
#include "../client/render/hdEdition/HdImageLoader.h"
#include "../client/widgets/GraphicalPrimitiveCanvas.h"
#include "../client/widgets/ObjectLists.h"
#include "../client/widgets/Buttons.h"
#include "../client/widgets/TextControls.h"
#include "../client/windows/GUIClasses.h"
#include "../client/eventsSDL/InputSourceGameController.h"
#include "../lib/CConfigHandler.h"
#include "../lib/GameLibrary.h"
#include "../lib/filesystem/CFilesystemLoader.h"
#include "../lib/filesystem/Filesystem.h"
#include "../lib/filesystem/ResourcePath.h"
#include "../lib/modding/IdentifierStorage.h"
#include "../lib/modding/ModScope.h"
#include "../lib/spells/CSpellHandler.h"
#include "../lib/spells/SpellSchoolHandler.h"
#include "../lib/texts/Languages.h"
#include "../lib/texts/CGeneralTextHandler.h"
#include "../lib/texts/Languages.h"
#include "../lib/texts/TextOperations.h"

#include <gtest/gtest.h>

#include <cstdlib>
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
		if(onShowAll)
			onShowAll();
		if(showAllCount == 1)
			CObjectListWindow::showAll(to);
	}

	Canvas * redrawCanvas = nullptr;
	size_t showAllCount = 0;
	std::function<void()> onShowAll;
};

class TestFont final : public IFont
{
public:
	size_t getLineHeightScaled() const override
	{
		return 7;
	}

	size_t getGlyphWidthScaled(const char * data) const override
	{
		return canRepresentCharacter(data) ? 5 : 0;
	}

	size_t getFontAscentScaled() const override
	{
		return 6;
	}

	bool canRepresentCharacter(const char * data) const override
	{
		if(!data || !*data)
			return false;
		const size_t characterSize = TextOperations::getUnicodeCharacterSize(*data);
		return TextOperations::isValidUnicodeCharacter(data, characterSize);
	}

private:
	void renderText(SDL_Surface *, const std::string &, const ColorRGBA &, const Point &) const override
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
		// Production startup populates session language/encoding through
		// CGeneralTextHandler detection; the headless fixture must provide the
		// same session state or Debug assertions in getInstalledLanguage fire
		testSettings["session"]["language"].String() = "english";
		testSettings["session"]["languageDeviation"].Float() = 1.0;
		testSettings["session"]["encoding"].String() = Languages::getLanguageOptions("english").encoding;
		// CursorHandler asserts the software cursor contract in Debug builds
		testSettings["video"]["cursor"].String() = "software";
		testSettings["input"]["enableMouse"].Bool() = true;
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
		RenderHandler & renderer, const std::string & name, EImageBlitMode mode, const Point & dimensions,
		ColorRGBA color = ColorRGBA(100, 100, 100, 255))
	{
		auto surface = SDL_CreateRGBSurfaceWithFormat(0, dimensions.x, dimensions.y, 32, SDL_PIXELFORMAT_ARGB8888);
		if(!surface)
			throw std::runtime_error(SDL_GetError());

		SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, color.r, color.g, color.b, color.a));
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

	void addControllerActionSprites(RenderHandler & renderer)
	{
		static constexpr std::array families = {"playstation", "generic"};
		static constexpr std::array actions = {"add", "cancel"};
		static constexpr std::array states = {"normal", "pressed", "disabled"};
		for(const auto * family : families)
			for(const auto * action : actions)
				for(const auto * state : states)
					addProductionImage(
						renderer,
							"controllerActionBar/" + std::string(family) + "-" + action + "-" + state + ".png",
							EImageBlitMode::COLORKEY,
							Point(24, 24),
							std::string(family) == "playstation"
								? (std::string(state) == "disabled" ? ColorRGBA(24, 24, 24, 255) : Colors::BLACK)
								: ColorRGBA(24, 24, 24, 255));
	}

	void initializeProductionListConstruction()
	{
		CResourceHandler::initialize();
		resourceHandlerInitialized = true;

		const auto sourceRoot = boost::filesystem::path(__FILE__).parent_path().parent_path();
		CResourceHandler::addFilesystem(
			"data",
			"controller-list-test-config",
			std::make_unique<CFilesystemLoader>("config/", sourceRoot / "config", 0));
		CResourceHandler::addFilesystem(
			"data",
			"controller-list-test-button-config",
			std::make_unique<CFilesystemLoader>("config/widgets/buttons/", sourceRoot / "config" / "widgets" / "buttons", 0));
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
		const std::string preferredLanguage = CGeneralTextHandler::getPreferredLanguage();
		if(preferredLanguage != "english")
		{
			const JsonNode selectedTranslation(JsonPath::builtin("config/translations/" + preferredLanguage + ".json"));
			localizationLibrary->generaltexth->loadTranslationOverrides("vcmi", preferredLanguage, selectedTranslation);
		}
		ENGINE->screenHandlerInstance = std::make_unique<ScreenHandler>();
		ENGINE->renderHandlerInstance = std::make_unique<RenderHandler>();
		auto & renderer = static_cast<RenderHandler &>(ENGINE->renderHandler());
		renderer.hdImageLoader = std::make_shared<HdImageLoader>();

		auto testFont = std::make_shared<TestFont>();
		renderer.fonts[FONT_BIG] = testFont;
		renderer.fonts[FONT_SMALL] = testFont;
		renderer.fonts[FONT_TINY] = testFont;

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
		addControllerActionSprites(renderer);

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

	ColorRGBA surfacePixel(SDL_Surface * surface, Point position) const
	{
		EXPECT_GE(position.x, 0);
		EXPECT_GE(position.y, 0);
		EXPECT_LT(position.x, surface->w);
		EXPECT_LT(position.y, surface->h);

		const auto * pixels = static_cast<const uint32_t *>(surface->pixels);
		const uint32_t pixel = pixels[position.y * surface->w + position.x];
		Uint8 r = 0;
		Uint8 g = 0;
		Uint8 b = 0;
		Uint8 a = 0;
		SDL_GetRGBA(pixel, surface->format, &r, &g, &b, &a);
		return ColorRGBA(r, g, b, a);
	}

	std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> renderWindowFrame(
		const std::shared_ptr<CObjectListWindow> & window) const
	{
		auto surface = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>(
			SDL_CreateRGBSurfaceWithFormat(0, 320, 460, 32, SDL_PIXELFORMAT_ARGB8888), SDL_FreeSurface);
		EXPECT_NE(surface, nullptr);
		SDL_FillRect(surface.get(), nullptr, SDL_MapRGBA(surface->format, 0, 0, 0, 255));
		Canvas canvas = Canvas::createFromSurface(surface.get(), CanvasScalingPolicy::AUTO);
		window->showAll(canvas);
		return surface;
	}

	size_t countDifferentPixels(SDL_Surface * lhs, SDL_Surface * rhs, const Rect & region) const
	{
		size_t result = 0;
		for(int y = std::max(region.y, 0); y < std::min(region.y + region.h, lhs->h); ++y)
			for(int x = std::max(region.x, 0); x < std::min(region.x + region.w, lhs->w); ++x)
			{
				const auto left = surfacePixel(lhs, Point(x, y));
				const auto right = surfacePixel(rhs, Point(x, y));
				if(left.r != right.r || left.g != right.g || left.b != right.b || left.a != right.a)
					++result;
			}
		return result;
	}

	void expectBackingDiffers(SDL_Surface * normal, SDL_Surface * state, const Rect & actionRect) const
	{
		const Rect backingProbe(actionRect.topLeft() + Point(112, 5), Point(8, 22));
		EXPECT_GE(countDifferentPixels(normal, state, backingProbe), 6);
	}

	void moveWindowIntoFrame(const std::shared_ptr<CObjectListWindow> & window) const
	{
		const Point offset(std::max(0, -window->pos.x), std::max(0, -window->pos.y));
		if(offset.x || offset.y)
			window->moveBy(offset);
	}

	Rect acceptActionRect(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->ok->pos;
	}

	Rect cancelActionRect(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->exit->pos;
	}

	void expectActionRectAt(const Rect & actual, const Point & topLeft) const
	{
		EXPECT_EQ(actual.x, topLeft.x);
		EXPECT_EQ(actual.y, topLeft.y);
		EXPECT_EQ(actual.w, 124);
		EXPECT_EQ(actual.h, 32);
	}

	bool actionButtonVisualsApplied(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->acceptControllerButtonVisual && window->cancelControllerButtonVisual;
	}

	bool setActionButtonPressed(const std::shared_ptr<CObjectListWindow> & window, bool accept, bool pressed) const
	{
		const auto & button = accept ? window->ok : window->exit;
		if(pressed)
		{
			button->setSoundDisabled(true);
			button->clickPressed(button->pos.center());
		}
		else
			button->clickCancel(button->pos.center());
		return button->isPressed();
	}

	bool acceptActionPressed(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->ok->isPressed();
	}

	bool acceptActionBlocked(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->ok->isBlocked();
	}

	bool cancelActionPressed(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->exit->isPressed();
	}

	std::string acceptPromptSpriteName(const std::shared_ptr<CObjectListWindow> & window) const
	{
		return window->acceptControllerPromptSprite ? window->acceptControllerPromptSprite->getOriginalName() : "";
	}

	void blockAcceptAction(const std::shared_ptr<CObjectListWindow> & window) const
	{
		window->ok->block(true);
	}

	void silenceActionButtons(const std::shared_ptr<CObjectListWindow> & window) const
	{
		window->ok->setSoundDisabled(true);
		window->exit->setSoundDisabled(true);
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

	/// TextOperations locale resolution dereferences LIBRARY->generaltexth on
	/// first use; headless fixtures without production startup must provide it
	void ensureLocalizationLibrary()
	{
		if(LIBRARY)
			return;
		localizationLibrary = std::make_unique<GameLibrary>();
		LIBRARY = localizationLibrary.get();
		localizationLibrary->generaltexth = std::unique_ptr<CGeneralTextHandler>(
			new CGeneralTextHandler(CGeneralTextHandler::TestConstructionTag()));
	}

	void setInputMode(InputMode mode)
	{
		ENGINE->input().setCurrentInputMode(mode);
	}

	void enableLiveScreenRedraw()
	{
		ENGINE->headlessForTests = false;
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

#if SDL_VERSION_ATLEAST(2, 24, 0)
		SDL_VirtualJoystickDesc descriptor{};
		descriptor.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
		descriptor.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
		descriptor.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
		descriptor.vendor_id = 1356;
		descriptor.product_id = 3302;
		descriptor.name = "VCMI test DualSense";

		const int deviceIndex = SDL_JoystickAttachVirtualEx(&descriptor);
#else
		// SDL < 2.24 cannot attach virtual devices with vendor/product identity;
		// use the 2.0.14 API plus an explicit GameController mapping instead.
		// Note: the SDL 2.0.14 - 2.23 implementation consumes the counts in
		// (naxes, nbuttons) order; this was fixed in 2.24 together with
		// SDL_JoystickAttachVirtualEx, so the arguments are passed swapped here.
		const int deviceIndex = SDL_JoystickAttachVirtual(
			SDL_JOYSTICK_TYPE_GAMECONTROLLER,
			SDL_CONTROLLER_AXIS_MAX,
			SDL_CONTROLLER_BUTTON_MAX,
			0);
#endif
		if(deviceIndex < 0)
			throw std::runtime_error(SDL_GetError());
		virtualControllerDeviceIndices.push_back(deviceIndex);

		char guid[33]{};
		SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(deviceIndex), guid, sizeof(guid));
		const std::string mapping = std::string(guid)
			+ ",VCMI test DualSense,a:b0,b:b1,dpup:b11,dpdown:b12,platform:" + SDL_GetPlatform() + ",type:ps5";
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
		testSettings["input"]["controllerAxisDeadZone"].Float() = 0.2;
		testSettings["input"]["controllerAxisFullZone"].Float() = 1.0;
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

	void dispatchVirtualControllerAxis(SDL_JoystickID instanceId, SDL_GameControllerAxis axis, Sint16 value)
	{
		SDL_Event event{};
		event.type = SDL_CONTROLLERAXISMOTION;
		event.caxis.type = SDL_CONTROLLERAXISMOTION;
		event.caxis.which = instanceId;
		event.caxis.axis = static_cast<Uint8>(axis);
		event.caxis.value = value;
		if(SDL_PushEvent(&event) != 1)
			throw std::runtime_error(SDL_GetError());

		ENGINE->input().fetchEvents();
		ENGINE->input().processEvents();
	}

	void dispatchMouseMotion(const Point & position)
	{
		SDL_Event event{};
		event.type = SDL_MOUSEMOTION;
		event.motion.type = SDL_MOUSEMOTION;
		event.motion.x = position.x * ENGINE->screenHandler().getScalingFactor();
		event.motion.y = position.y * ENGINE->screenHandler().getScalingFactor();
		if(SDL_PushEvent(&event) != 1)
			throw std::runtime_error(SDL_GetError());

		ENGINE->input().fetchEvents();
		ENGINE->input().processEvents();
	}

	void dispatchMouseButtonDown(const Point & position)
	{
		SDL_Event event{};
		event.type = SDL_MOUSEBUTTONDOWN;
		event.button.type = SDL_MOUSEBUTTONDOWN;
		event.button.button = SDL_BUTTON_LEFT;
		event.button.state = SDL_PRESSED;
		event.button.x = position.x * ENGINE->screenHandler().getScalingFactor();
		event.button.y = position.y * ENGINE->screenHandler().getScalingFactor();
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

	/// SDL >= 2.24 attaches virtual devices with vendor/product identity, so SDL
	/// classifies them as PlayStation controllers. Older SDL cannot expose that
	/// identity, so identity oracles verify the generic opened-device contract
	/// there instead of fabricating a classification.
	static constexpr bool virtualControllerHasIdentity = SDL_VERSION_ATLEAST(2, 24, 0);

	void expectAttachedDualSenseIdentity(SDL_JoystickID instanceId) const
	{
#if SDL_VERSION_ATLEAST(2, 24, 0)
		ASSERT_EQ(openedControllerType(instanceId), SDL_CONTROLLER_TYPE_PS5);
#else
		ASSERT_EQ(ENGINE->input().gameControllerHandler->gameControllerMap.count(instanceId), 1);
#endif
	}

	ControllerPresentation expectedAttachedPresentation() const
	{
#if SDL_VERSION_ATLEAST(2, 24, 0)
		return ControllerPresentation::PLAYSTATION;
#else
		return ControllerPresentation::UNKNOWN;
#endif
	}

	void initializeCursorPresentation()
	{
		ENGINE->cursorHandlerInstance = std::make_unique<CursorHandler>();
		ENGINE->cursor().show();
	}

	std::array<ControllerPresentation, 3> controllerPresentationsAfterRemap()
	{
		InputSourceGameController controller{InputSourceGameController::HeadlessTestTag()};
		controller.controllerPresentations.emplace(17, ControllerPresentation::PLAYSTATION);
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

TEST_F(ShortcutGlyphQueryTest, ControllerGlyphRefreshDoesNotReenterShowAll)
{
	initializeProductionListConstruction();
	setJoystickBindings({
		{"a", EShortcut::GLOBAL_ACCEPT},
		{"b", EShortcut::GLOBAL_CANCEL}
	});

	std::vector<CObjectListWindow::ListItem> items{{"Spell", true, ""}};
	std::vector<std::shared_ptr<IImage>> images{productionListIcon};
	auto window = std::make_shared<GlyphRefreshProbeWindow>(
		items, nullptr, "Add spell", "Select a spell", [](int)
		{
		}, 0, images, true, true);
	window->setBattleOnlySpellActionPrompts();

	ENGINE->windows().pushWindow(window);
	enableLiveScreenRedraw();
	auto surface = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>(
		SDL_CreateRGBSurfaceWithFormat(0, 320, 460, 32, SDL_PIXELFORMAT_ARGB8888), SDL_FreeSurface);
	ASSERT_NE(surface, nullptr);
	Canvas canvas = Canvas::createFromSurface(surface.get(), CanvasScalingPolicy::AUTO);
	window->redrawCanvas = &canvas;
	std::vector<bool> reentrantAppliedStates;
	window->onShowAll = [&]
	{
		if(window->showAllCount > 1)
			reentrantAppliedStates.push_back(actionButtonVisualsApplied(window));
	};
	const SDL_JoystickID controllerInstance = attachVirtualDualSense();
	dispatchVirtualControllerButton(controllerInstance, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
	ASSERT_EQ(ENGINE->input().getCurrentInputMode(), InputMode::CONTROLLER);
	ASSERT_EQ(ENGINE->input().getControllerPresentation(), expectedAttachedPresentation());

	window->showAll(canvas);

	EXPECT_EQ(window->showAllCount, 1);
	EXPECT_TRUE(reentrantAppliedStates.empty());
	EXPECT_TRUE(actionButtonVisualsApplied(window));
	expectActionRectAt(acceptActionRect(window), window->pos.topLeft() + Point(15, 402));
	expectActionRectAt(cancelActionRect(window), window->pos.topLeft() + Point(158, 402));

	const auto stableShowAllCount = window->showAllCount;
	window->showAll(canvas);
	EXPECT_EQ(window->showAllCount, stableShowAllCount + 1);
	window->redrawCanvas = nullptr;
	ENGINE->windows().popWindow(window);
}

TEST_F(ShortcutGlyphQueryTest, DualSenseBindingsRefreshGlyphsAfterControllerActivation)
{
	initializeProductionListConstruction();
	initializeCursorPresentation();
	setJoystickBindings({
		{"a", EShortcut::GLOBAL_ACCEPT},
		{"b", EShortcut::GLOBAL_CANCEL}
	});

	const auto acceptBindings = ENGINE->shortcuts().getJoystickBindings(EShortcut::GLOBAL_ACCEPT);
	const auto cancelBindings = ENGINE->shortcuts().getJoystickBindings(EShortcut::GLOBAL_CANCEL);
	ASSERT_FALSE(acceptBindings.empty());
	ASSERT_FALSE(cancelBindings.empty());
	const SDL_JoystickID controllerInstance = attachVirtualDualSense();

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
	window->setBattleOnlySpellActionPrompts();

	ENGINE->windows().pushWindow(window);

	ASSERT_EQ(ENGINE->input().getCurrentInputMode(), InputMode::KEYBOARD_AND_MOUSE);
	EXPECT_EQ(ENGINE->input().getControllerPresentation(), ControllerPresentation::UNKNOWN);
	EXPECT_FALSE(ENGINE->input().getControllerGlyphToken(acceptBindings));
	EXPECT_FALSE(ENGINE->input().getControllerGlyphToken(cancelBindings));
	EXPECT_EQ(acceptGlyphText(window), "");
	EXPECT_EQ(cancelGlyphText(window), "");

	dispatchVirtualControllerButton(controllerInstance, SDL_CONTROLLER_BUTTON_DPAD_DOWN);

	expectAttachedDualSenseIdentity(controllerInstance);
	ASSERT_EQ(ENGINE->input().getCurrentInputMode(), InputMode::CONTROLLER);
	ASSERT_EQ(ENGINE->input().getControllerPresentation(), expectedAttachedPresentation());
	const auto acceptToken = ENGINE->input().getControllerGlyphToken(acceptBindings);
	const auto cancelToken = ENGINE->input().getControllerGlyphToken(cancelBindings);
	if(virtualControllerHasIdentity)
	{
		ASSERT_TRUE(acceptToken.has_value());
		ASSERT_TRUE(cancelToken.has_value());
	}
	else
	{
		EXPECT_FALSE(acceptToken.has_value());
		EXPECT_FALSE(cancelToken.has_value());
	}

	EXPECT_EQ(acceptGlyphText(window), localizationLibrary->generaltexth->translate("vcmi.lobby.battleOnlySpellAdd.actionAdd"));
	EXPECT_EQ(cancelGlyphText(window), localizationLibrary->generaltexth->translate("vcmi.lobby.battleOnlySpellAdd.actionCancel"));

	dispatchMouseMotion(window->pos.center());
	EXPECT_EQ(ENGINE->input().getCurrentInputMode(), InputMode::KEYBOARD_AND_MOUSE);
	EXPECT_FALSE(actionButtonVisualsApplied(window));
	EXPECT_EQ(acceptGlyphText(window), "");
	EXPECT_EQ(cancelGlyphText(window), "");

	dispatchVirtualControllerAxis(controllerInstance, SDL_CONTROLLER_AXIS_LEFTX, 1);
	EXPECT_EQ(ENGINE->input().getCurrentInputMode(), InputMode::KEYBOARD_AND_MOUSE);
	EXPECT_FALSE(actionButtonVisualsApplied(window));
	EXPECT_EQ(acceptGlyphText(window), "");
	EXPECT_EQ(cancelGlyphText(window), "");

	dispatchVirtualControllerAxis(controllerInstance, SDL_CONTROLLER_AXIS_LEFTX, SDL_JOYSTICK_AXIS_MAX);
	EXPECT_EQ(ENGINE->input().getCurrentInputMode(), InputMode::CONTROLLER);
	EXPECT_TRUE(actionButtonVisualsApplied(window));
	EXPECT_EQ(acceptGlyphText(window), localizationLibrary->generaltexth->translate("vcmi.lobby.battleOnlySpellAdd.actionAdd"));
	EXPECT_EQ(cancelGlyphText(window), localizationLibrary->generaltexth->translate("vcmi.lobby.battleOnlySpellAdd.actionCancel"));
}

TEST_F(ShortcutGlyphQueryTest, DualSenseActionControlsUseAcceptedGeometryAndRestoreMouseButtons)
{
	initializeProductionListConstruction();
	setJoystickBindings({
		{"a", EShortcut::GLOBAL_ACCEPT},
		{"b", EShortcut::GLOBAL_CANCEL}
	});

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
	window->setBattleOnlySpellActionPrompts();

	ENGINE->windows().pushWindow(window);
	moveWindowIntoFrame(window);
	setInputMode(InputMode::KEYBOARD_AND_MOUSE);
	Canvas screen = ENGINE->screenHandler().getScreenCanvas();
	window->showAll(screen);
	const Rect acceptMouseRect = acceptActionRect(window);
	const Rect cancelMouseRect = cancelActionRect(window);
	EXPECT_EQ(acceptMouseRect.topLeft(), window->pos.topLeft() + Point(15, 402));
	EXPECT_EQ(cancelMouseRect.topLeft(), window->pos.topLeft() + Point(228, 402));
	ASSERT_TRUE(acceptGlyphText(window).empty());
	ASSERT_TRUE(cancelGlyphText(window).empty());
	const SDL_JoystickID controllerInstance = attachVirtualDualSense();
	dispatchVirtualControllerButton(controllerInstance, SDL_CONTROLLER_BUTTON_DPAD_DOWN);

	expectAttachedDualSenseIdentity(controllerInstance);
	ASSERT_EQ(ENGINE->input().getCurrentInputMode(), InputMode::CONTROLLER);
	ASSERT_EQ(ENGINE->input().getControllerPresentation(), expectedAttachedPresentation());
	const auto acceptLabel = localizationLibrary->generaltexth->translate("vcmi.lobby.battleOnlySpellAdd.actionAdd");
	const auto cancelLabel = localizationLibrary->generaltexth->translate("vcmi.lobby.battleOnlySpellAdd.actionCancel");
	ASSERT_EQ(acceptGlyphText(window), acceptLabel);
	ASSERT_EQ(cancelGlyphText(window), cancelLabel);
	expectActionRectAt(acceptActionRect(window), window->pos.topLeft() + Point(15, 402));
	expectActionRectAt(cancelActionRect(window), window->pos.topLeft() + Point(158, 402));
	EXPECT_EQ(acceptActionRect(window).x, acceptMouseRect.x);
	EXPECT_EQ(cancelActionRect(window).x + cancelActionRect(window).w, cancelMouseRect.x + cancelMouseRect.w);
	EXPECT_LE(acceptActionRect(window).x + acceptActionRect(window).w, cancelActionRect(window).x);

	setInputMode(InputMode::KEYBOARD_AND_MOUSE);
	window->showAll(screen);
	EXPECT_EQ(acceptGlyphText(window), "");
	EXPECT_EQ(cancelGlyphText(window), "");
	EXPECT_FALSE(actionButtonVisualsApplied(window));
	EXPECT_EQ(acceptActionRect(window).topLeft(), window->pos.topLeft() + Point(15, 402));
	EXPECT_EQ(cancelActionRect(window).topLeft(), window->pos.topLeft() + Point(228, 402));
}

TEST_F(ShortcutGlyphQueryTest, DualSenseActionBarComposesButtonStatesAndRestoresMouseMode)
{
	initializeProductionListConstruction();
	setJoystickBindings({{"a", EShortcut::GLOBAL_ACCEPT}, {"b", EShortcut::GLOBAL_CANCEL}});
	auto window = std::make_shared<CObjectListWindow>(
		std::vector<CObjectListWindow::ListItem>{{"Enabled", true, ""}, {"Disabled", false, "Already selected"}},
		nullptr, "Add spell", "Select a spell", [](int){}, 0,
		std::vector<std::shared_ptr<IImage>>{productionListIcon, productionListIcon}, true, true);
	window->setBattleOnlySpellActionPrompts();
	ENGINE->windows().pushWindow(window);
	moveWindowIntoFrame(window);
	setControllerPresentation(ControllerPresentation::PLAYSTATION);
	setInputMode(InputMode::CONTROLLER);
	Canvas screen = ENGINE->screenHandler().getScreenCanvas();
	window->showAll(screen);
	ASSERT_TRUE(actionButtonVisualsApplied(window));
	auto normalFrame = renderWindowFrame(window);
	const Rect normalAcceptRect = acceptActionRect(window);
	const Rect normalCancelRect = cancelActionRect(window);
	window->changeSelection(1);
	window->showAll(screen);
	auto disabledFrame = renderWindowFrame(window);
	expectBackingDiffers(normalFrame.get(), disabledFrame.get(), normalAcceptRect);
	window->changeSelection(0);
	ASSERT_TRUE(setActionButtonPressed(window, true, true));
	window->showAll(screen);
	auto acceptPressedFrame = renderWindowFrame(window);
	expectBackingDiffers(normalFrame.get(), acceptPressedFrame.get(), normalAcceptRect);
	setActionButtonPressed(window, true, false);
	ASSERT_TRUE(setActionButtonPressed(window, false, true));
	window->showAll(screen);
	auto cancelPressedFrame = renderWindowFrame(window);
	expectBackingDiffers(normalFrame.get(), cancelPressedFrame.get(), normalCancelRect);
	setActionButtonPressed(window, false, false);
	setInputMode(InputMode::KEYBOARD_AND_MOUSE);
	window->showAll(screen);
	EXPECT_FALSE(actionButtonVisualsApplied(window));
	EXPECT_EQ(acceptGlyphText(window), "");
	EXPECT_EQ(cancelGlyphText(window), "");
	EXPECT_EQ(acceptActionRect(window).topLeft(), window->pos.topLeft() + Point(15, 402));
	EXPECT_EQ(cancelActionRect(window).topLeft(), window->pos.topLeft() + Point(228, 402));
}

TEST_F(ShortcutGlyphQueryTest, RemoveSpellPromptsUseParameterizedAcceptLabel)
{
	initializeProductionListConstruction();
	setJoystickBindings({{"a", EShortcut::GLOBAL_ACCEPT}, {"b", EShortcut::GLOBAL_CANCEL}});
	auto window = std::make_shared<CObjectListWindow>(
		std::vector<CObjectListWindow::ListItem>{{"Magic Arrow", true, ""}},
		nullptr, "Remove spell", "Select a spell", [](int){}, 0,
		std::vector<std::shared_ptr<IImage>>{productionListIcon}, true, true);
	window->setBattleOnlySpellActionPrompts("vcmi.lobby.battleOnlySpellRemove.actionRemove");
	ENGINE->windows().pushWindow(window);
	setControllerPresentation(ControllerPresentation::PLAYSTATION);
	setInputMode(InputMode::CONTROLLER);
	EXPECT_EQ(acceptGlyphText(window), "Remove");
	EXPECT_EQ(cancelGlyphText(window), "Cancel");
	setInputMode(InputMode::KEYBOARD_AND_MOUSE);
	EXPECT_EQ(acceptGlyphText(window), "");
	EXPECT_EQ(cancelGlyphText(window), "");
}

TEST_F(ShortcutGlyphQueryTest, UnknownControllerUsesGenericRasterActionControlsWithoutPublicGlyphToken)
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
	window->setBattleOnlySpellActionPrompts();

	ENGINE->windows().pushWindow(window);
	moveWindowIntoFrame(window);
	setControllerPresentation(ControllerPresentation::UNKNOWN);
	setInputMode(InputMode::CONTROLLER);
	Canvas screen = ENGINE->screenHandler().getScreenCanvas();
	window->showAll(screen);

	EXPECT_FALSE(ENGINE->input().getControllerGlyphToken(acceptBindings));
	EXPECT_FALSE(ENGINE->input().getControllerGlyphToken(cancelBindings));
	EXPECT_EQ(acceptGlyphText(window), localizationLibrary->generaltexth->translate("vcmi.lobby.battleOnlySpellAdd.actionAdd"));
	EXPECT_EQ(cancelGlyphText(window), localizationLibrary->generaltexth->translate("vcmi.lobby.battleOnlySpellAdd.actionCancel"));
	expectActionRectAt(acceptActionRect(window), window->pos.topLeft() + Point(15, 402));
	expectActionRectAt(cancelActionRect(window), window->pos.topLeft() + Point(158, 402));
	EXPECT_TRUE(actionButtonVisualsApplied(window));
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

TEST_F(CObjectListWindowControllerTest, ChineseBattleOnlySelectedReasonDoesNotFallBackToEnglish)
{
	auto & testSettings = const_cast<JsonNode &>(settings.toJsonNode());
	testSettings["general"]["language"].String() = "chinese";
	initializeProductionListConstruction();
	initializeProductionSpellData();

	std::vector<SpellID> selectedSpells;
	for(size_t index = 4; index < GameConstants::SPELLS_QUANTITY; index += 5)
		selectedSpells.emplace_back(static_cast<si32>(index));

	const auto allowedSpells = localizationLibrary->spellh->getDefaultAllowed();
	const std::vector<SpellID> allSpells(allowedSpells.begin(), allowedSpells.end());
	const auto payload = observeBattleOnlyAddPayload(allSpells, selectedSpells);
	const auto disabledReason = localizationLibrary->generaltexth->translate(
		"vcmi.lobby.battleOnlySpellAlreadySelected");

	ASSERT_EQ(disabledReason, "已选择");
	ASSERT_NE(disabledReason, "Already selected");
	ASSERT_FALSE(payload.items.empty());
	EXPECT_TRUE(std::any_of(payload.items.begin(), payload.items.end(), [&](const auto & item)
	{
		return !item.enabled && item.disabledReason == disabledReason;
	}));
}

TEST_F(CObjectListWindowControllerTest, DisabledItemRejectsAllActivationPaths)
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
	window->changeSelection(1);
	ENGINE->events().dispatchShortcutPressed({EShortcut::GLOBAL_ACCEPT});

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
	initializeProductionListConstruction();
	auto makeProductionWindow = [this](std::function<void(int)> callback)
	{
		auto window = std::make_shared<CObjectListWindow>(
			std::vector<CObjectListWindow::ListItem>{{"Magic Arrow", true, ""}},
			nullptr, "Add spell", "Select a spell", std::move(callback), 0,
			std::vector<std::shared_ptr<IImage>>{productionListIcon}, true, true);
		ENGINE->windows().pushWindow(window);
		silenceActionButtons(window);
		return window;
	};

	auto acceptedWindow = makeProductionWindow([&accepted](int)
	{
		++accepted;
	});
	ENGINE->events().dispatchShortcutPressed({EShortcut::GLOBAL_ACCEPT});
	ENGINE->events().dispatchShortcutReleased({EShortcut::GLOBAL_ACCEPT});

	EXPECT_EQ(accepted, 1);
	EXPECT_FALSE(ENGINE->windows().isTopWindow(acceptedWindow));

	auto canceledWindow = makeProductionWindow([](int)
	{
	});
	canceledWindow->onExit = [&exited]
	{
		++exited;
	};
	ENGINE->events().dispatchShortcutPressed({EShortcut::GLOBAL_CANCEL});
	ENGINE->events().dispatchShortcutReleased({EShortcut::GLOBAL_CANCEL});

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
	EXPECT_EQ(clicked, 99);
	EXPECT_EQ(opened, 99);

	window->elementSelected();
	EXPECT_EQ(accepted, 99);
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

	ensureLocalizationLibrary();
	searchItems(window, "beta");

	EXPECT_EQ(visibleItems(window), std::vector<size_t>({1, 2}));
	EXPECT_EQ(focusedItem(window), 2);
	EXPECT_EQ(enabledSelection(window), 1);
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
	EXPECT_FALSE(InputSourceGameController::getGlyphToken(ControllerPresentation::PLAYSTATION, acceptBindings));
	EXPECT_EQ(InputSourceGameController::getGlyphToken(ControllerPresentation::PLAYSTATION, cancelBindings), "○");

	setJoystickBindings({
		{"a", EShortcut::GLOBAL_ACCEPT},
		{"b", EShortcut::GLOBAL_CANCEL}
	});

	const auto remappedAcceptBindings = ENGINE->shortcuts().getJoystickBindings(EShortcut::GLOBAL_ACCEPT);
	EXPECT_EQ(InputSourceGameController::getGlyphToken(ControllerPresentation::PLAYSTATION, remappedAcceptBindings), "×");
	EXPECT_FALSE(InputSourceGameController::getGlyphToken(ControllerPresentation::UNKNOWN, {"a"}));
	EXPECT_FALSE(InputSourceGameController::getGlyphToken(ControllerPresentation::PLAYSTATION, {}));
	EXPECT_FALSE(InputSourceGameController::getGlyphToken(ControllerPresentation::PLAYSTATION, {"a", "b"}));
	EXPECT_FALSE(InputSourceGameController::getGlyphToken(ControllerPresentation::PLAYSTATION, {"leftshoulder"}));
}

TEST_F(ShortcutGlyphQueryTest, ControllerRemapInvalidatesActivePresentation)
{
	const auto presentations = controllerPresentationsAfterRemap();
	EXPECT_EQ(presentations[0], ControllerPresentation::PLAYSTATION);
	EXPECT_EQ(presentations[1], ControllerPresentation::UNKNOWN);
	EXPECT_EQ(presentations[2], ControllerPresentation::UNKNOWN);
}

TEST_F(ShortcutGlyphQueryTest, ControllerAcceptDefersExecutionUntilButtonRelease)
{
	int accepted = 0;
	initializeProductionListConstruction();
	setJoystickBindings({{"a", EShortcut::GLOBAL_ACCEPT}, {"b", EShortcut::GLOBAL_CANCEL}});
	auto window = std::make_shared<CObjectListWindow>(
		std::vector<CObjectListWindow::ListItem>{{"Magic Arrow", true, ""}},
		nullptr, "Add spell", "Select a spell", [&accepted](int)
		{
			++accepted;
		}, 0,
		std::vector<std::shared_ptr<IImage>>{productionListIcon}, true, true);
	ENGINE->windows().pushWindow(window);
	setControllerPresentation(ControllerPresentation::PLAYSTATION);
	setInputMode(InputMode::CONTROLLER);
	silenceActionButtons(window);

	ENGINE->events().dispatchShortcutPressed({EShortcut::GLOBAL_ACCEPT});

	EXPECT_EQ(accepted, 0);
	EXPECT_TRUE(ENGINE->windows().isTopWindow(window));
	EXPECT_TRUE(acceptActionPressed(window));

	ENGINE->events().dispatchShortcutReleased({EShortcut::GLOBAL_ACCEPT});

	EXPECT_EQ(accepted, 1);
	EXPECT_FALSE(ENGINE->windows().isTopWindow(window));
}

TEST_F(ShortcutGlyphQueryTest, ControllerCancelDefersExecutionUntilButtonRelease)
{
	int exited = 0;
	initializeProductionListConstruction();
	setJoystickBindings({{"a", EShortcut::GLOBAL_ACCEPT}, {"b", EShortcut::GLOBAL_CANCEL}});
	auto window = std::make_shared<CObjectListWindow>(
		std::vector<CObjectListWindow::ListItem>{{"Magic Arrow", true, ""}},
		nullptr, "Remove spell", "Select a spell", [](int)
		{
		}, 0,
		std::vector<std::shared_ptr<IImage>>{productionListIcon}, true, true);
	window->onExit = [&exited]
	{
		++exited;
	};
	ENGINE->windows().pushWindow(window);
	setControllerPresentation(ControllerPresentation::PLAYSTATION);
	setInputMode(InputMode::CONTROLLER);
	silenceActionButtons(window);

	ENGINE->events().dispatchShortcutPressed({EShortcut::GLOBAL_CANCEL});

	EXPECT_EQ(exited, 0);
	EXPECT_TRUE(ENGINE->windows().isTopWindow(window));
	EXPECT_TRUE(cancelActionPressed(window));

	ENGINE->events().dispatchShortcutReleased({EShortcut::GLOBAL_CANCEL});

	EXPECT_EQ(exited, 1);
	EXPECT_FALSE(ENGINE->windows().isTopWindow(window));
}

TEST_F(ShortcutGlyphQueryTest, ControllerAcceptShowsUnifiedPressedVisualWhileHeld)
{
	initializeProductionListConstruction();
	setJoystickBindings({{"a", EShortcut::GLOBAL_ACCEPT}, {"b", EShortcut::GLOBAL_CANCEL}});
	auto window = std::make_shared<CObjectListWindow>(
		std::vector<CObjectListWindow::ListItem>{{"Magic Arrow", true, ""}},
		nullptr, "Add spell", "Select a spell", [](int){}, 0,
		std::vector<std::shared_ptr<IImage>>{productionListIcon}, true, true);
	window->setBattleOnlySpellActionPrompts();
	ENGINE->windows().pushWindow(window);
	setControllerPresentation(ControllerPresentation::PLAYSTATION);
	setInputMode(InputMode::CONTROLLER);
	silenceActionButtons(window);

	ASSERT_NE(acceptPromptSpriteName(window).find("-normal"), std::string::npos);

	ENGINE->events().dispatchShortcutPressed({EShortcut::GLOBAL_ACCEPT});

	EXPECT_TRUE(acceptActionPressed(window));
	EXPECT_NE(acceptPromptSpriteName(window).find("-pressed"), std::string::npos);

	ENGINE->events().dispatchShortcutReleased({EShortcut::GLOBAL_ACCEPT});

	EXPECT_FALSE(ENGINE->windows().isTopWindow(window));
}

TEST_F(CObjectListWindowControllerTest, KeyboardAcceptStillExecutesOnPress)
{
	int accepted = 0;
	setInputMode(InputMode::KEYBOARD_AND_MOUSE);
	auto window = createWindow({{"Enabled", true, ""}}, [&accepted](int)
	{
		++accepted;
	});
	ENGINE->windows().pushWindow(window);

	ENGINE->events().dispatchShortcutPressed({EShortcut::GLOBAL_ACCEPT});

	EXPECT_EQ(accepted, 1);
	EXPECT_FALSE(ENGINE->windows().isTopWindow(window));
}

TEST_F(ShortcutGlyphQueryTest, BlockedAcceptStaysInactiveThroughPressAndRelease)
{
	int accepted = 0;
	initializeProductionListConstruction();
	setJoystickBindings({{"a", EShortcut::GLOBAL_ACCEPT}, {"b", EShortcut::GLOBAL_CANCEL}});
	auto window = std::make_shared<CObjectListWindow>(
		std::vector<CObjectListWindow::ListItem>{{"Magic Arrow", true, ""}},
		nullptr, "Add spell", "Select a spell", [&accepted](int)
		{
			++accepted;
		}, 0,
		std::vector<std::shared_ptr<IImage>>{productionListIcon}, true, true);
	ENGINE->windows().pushWindow(window);
	setControllerPresentation(ControllerPresentation::PLAYSTATION);
	setInputMode(InputMode::CONTROLLER);
	silenceActionButtons(window);
	blockAcceptAction(window);
	ASSERT_TRUE(acceptActionBlocked(window));

	ENGINE->events().dispatchShortcutPressed({EShortcut::GLOBAL_ACCEPT});

	EXPECT_FALSE(acceptActionPressed(window));

	ENGINE->events().dispatchShortcutReleased({EShortcut::GLOBAL_ACCEPT});

	EXPECT_EQ(accepted, 0);
	EXPECT_TRUE(ENGINE->windows().isTopWindow(window));
}
