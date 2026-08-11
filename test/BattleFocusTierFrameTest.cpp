/*
 * BattleFocusTierFrameTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "../client/StdInc.h"

#include "../client/battle/BattleAttackDirection.h"
#include "../client/battle/BattleDirectionArrow.h"
#include "../client/battle/BattleFocusHighlights.h"
#include "../client/battle/BattleFocusTier.h"
#include "../client/GameEngine.h"
#include "../client/eventsSDL/InputHandler.h"
#include "../client/render/Canvas.h"
#include "../client/render/ColorFilter.h"
#include "../client/render/IImage.h"
#include "../client/render/IRenderHandler.h"
#include "../client/renderSDL/RenderHandler.h"
#include "../client/renderSDL/SDLImage.h"
#include "../client/renderSDL/ScreenHandler.h"
#include "../client/render/hdEdition/HdImageLoader.h"
#include "../lib/battle/BattleHex.h"
#include "../lib/CConfigHandler.h"
#include "../lib/GameLibrary.h"
#include "../lib/filesystem/CFilesystemLoader.h"
#include "../lib/filesystem/Filesystem.h"
#include "../lib/filesystem/ResourcePath.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <SDL.h>
#include <SDL_surface.h>

/// Replacement for the clientapp entry-point symbol; test binaries do not
/// link EntryPoint.cpp
[[noreturn]] void handleFatalError(const std::string & message, bool)
{
	throw std::runtime_error(message);
}

namespace
{
/// Same hex geometry as BattleFieldController::hexPositionLocal
Point hexTopLeft(const BattleHex & hex)
{
	int x = 14 + ((hex.getY()) % 2 == 0 ? 22 : 0) + 44 * hex.getX();
	int y = 86 + 42 * hex.getY();
	return Point(x, y);
}

ColorRGBA pixelAt(SDL_Surface * surface, Point position)
{
	EXPECT_GT(position.x, 0);
	EXPECT_GT(position.y, 0);
	EXPECT_LT(position.x, surface->w);
	EXPECT_LT(position.y, surface->h);

	SDL_Color color{0, 0, 0, 0};
	if(surface->format->palette)
		color = surface->format->palette->colors[
			static_cast<const uint8_t *>(surface->pixels)[position.y * surface->pitch + position.x]];
	else
	{
		Uint8 alpha = 0;
		SDL_GetRGBA(
			static_cast<const uint32_t *>(surface->pixels)[position.y * surface->w + position.x],
			surface->format, &color.r, &color.g, &color.b, &alpha);
	}
	return ColorRGBA(color.r, color.g, color.b, color.a);
}

int luminance(const ColorRGBA & color)
{
	return (299 * color.r + 587 * color.g + 114 * color.b) / 1000;
}
}

/// Composes the four controller focus tiers on a real battlefield background
/// through the production rendering pipeline (real VFS assets, real tinting,
/// real Canvas blitting) and exports full-frame and crop evidence. Requires
/// installed H3 game data; skips otherwise so upstream CI without data stays
/// green.
class BattleFocusTierFrameTest : public testing::Test
{
protected:
	std::unique_ptr<GameLibrary> library;
	JsonNode settingsSnapshot;

	void SetUp() override
	{
		library = std::make_unique<GameLibrary>();
		LIBRARY = library.get();
		// production filesystem: real user data root with H3 archives and
		// mods; config/filesystem.json comes from the source checkout because
		// a bare test binary has no installed bundle data paths
		CResourceHandler::initialize();
		const auto sourceRoot = boost::filesystem::path(__FILE__).parent_path().parent_path();
		CResourceHandler::addFilesystem(
			"initial",
			"battle-frame-repo-config",
			std::make_unique<CFilesystemLoader>("config/", sourceRoot / "config", 1));
		library->initializeFilesystem(false, /*useTestPreset*/ false);

		// same probe order as RenderHandler::loadImageFromFileUncached; battle
		// cell art lives in DATA/ (H3ab_bmp.lod), not SPRITES/
		const auto probeData = ImagePath::builtin("CCELLGRD.BMP").addPrefix("DATA/");
		if(!CResourceHandler::get()->existsResource(probeData))
			GTEST_SKIP() << "H3 game data not installed; frame evidence requires real assets";

		ENGINE = std::unique_ptr<GameEngine>(new GameEngine(GameEngine::HeadlessTestTag()));
		ENGINE->input().setCurrentInputMode(InputMode::CONTROLLER);
		// ScreenHandler::validateSettings mutates the in-memory settings node
		// (e.g. rewrites video resolution against the desktop display mode).
		// Snapshot the node first so TearDown can restore it and a later
		// fixture's ScreenHandler computes its upscaling filter from the same
		// baseline instead of the mutated state.
		settingsSnapshot = settings.toJsonNode();
		// evidence frames are captured at the logical 1x resolution drawn in
		// game; pin the filter through the production settings path so the
		// fixture never touches the async upscaling path (the test engine has
		// no AsyncRunner) and never rewrites the player's video settings
		auto & testSettings = const_cast<JsonNode &>(settings.toJsonNode());
		testSettings["video"]["upscalingFilter"].String() = "none";
		ENGINE->screenHandlerInstance = std::make_unique<ScreenHandler>();
		ENGINE->renderHandlerInstance = std::make_unique<RenderHandler>();
		static_cast<RenderHandler &>(ENGINE->renderHandler()).hdImageLoader = std::make_shared<HdImageLoader>();
	}

	void TearDown() override
	{
		ENGINE.reset();
		LIBRARY = nullptr;
		library.reset();
		CResourceHandler::destroy();
		const_cast<JsonNode &>(settings.toJsonNode()) = settingsSnapshot;
	}

	boost::filesystem::path captureDir() const
	{
		if(const char * overrideDir = std::getenv("VCMI_CAPTURE_DIR"))
			return boost::filesystem::path(overrideDir) / "m3-1-focus-tiers";

		// test/ -> vcmi worktree root -> task root -> .worktrees -> workspace root
		const auto sourceRoot = boost::filesystem::path(__FILE__).parent_path().parent_path();
		return sourceRoot.parent_path().parent_path().parent_path() / "captures" / "m3-1-focus-tiers";
	}
};

TEST_F(BattleFocusTierFrameTest, FourTierMarkersComposeOnRealBattlefield)
{
	auto & renderer = ENGINE->renderHandler();

	auto background = renderer.loadImage(ImagePath::builtin("CMBKDES.BMP"), EImageBlitMode::OPAQUE);
	auto cellBorder = renderer.loadImage(ImagePath::builtin("CCELLGRD.BMP"), EImageBlitMode::COLORKEY);
	auto cellShade = renderer.loadImage(ImagePath::builtin("CCELLSHD.BMP"), EImageBlitMode::SIMPLE);

	ASSERT_GT(background->width(), 0);
	ASSERT_GT(background->height(), 0);

	SDL_Surface * surface = SDL_CreateRGBSurfaceWithFormat(
		0, background->width(), background->height(), 32, SDL_PIXELFORMAT_ARGB8888);
	ASSERT_NE(surface, nullptr);
	std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surfaceOwner(surface, SDL_FreeSurface);

	Canvas canvas = Canvas::createFromSurface(surface, CanvasScalingPolicy::IGNORE);
	canvas.draw(background, Point(0, 0));

	// cell borders over the interior grid, mirroring the production setting
	for(int hex = 0; hex < GameConstants::BFIELD_SIZE; ++hex)
	{
		if(hex % GameConstants::BFIELD_WIDTH == 0)
			continue;
		if(hex % GameConstants::BFIELD_WIDTH == GameConstants::BFIELD_WIDTH - 1)
			continue;
		canvas.draw(cellBorder, hexTopLeft(hex));
	}

	// one marker per tier on the same row so tints are directly comparable
	const BattleHex neutralHex(3, 5);
	const BattleHex movableHex(5, 5);
	const BattleHex attackableHex(7, 5);
	const BattleHex illegalHex(9, 5);

	auto drawTier = [&](BattleFocusTier::Tier tier, const BattleHex & hex)
	{
		const auto render = BattleFocusHighlights::loadTierRender(tier);
		const Point position = hexTopLeft(hex);
		if(render.shadeOverlay)
			canvas.draw(cellShade, position);
		canvas.draw(render.highlight, position);
		if(render.borderOverlay)
			canvas.draw(cellBorder, position);
	};

	drawTier(BattleFocusTier::Tier::NEUTRAL, neutralHex);
	drawTier(BattleFocusTier::Tier::MOVABLE, movableHex);
	drawTier(BattleFocusTier::Tier::ATTACKABLE, attackableHex);
	drawTier(BattleFocusTier::Tier::ILLEGAL, illegalHex);

	// evidence: full frame plus the marker row crop
	const auto outputDir = captureDir();
	boost::filesystem::create_directories(outputDir);

	SDLImageShared frame(surface);
	frame.exportBitmap(outputDir / "focus-tiers-full.png", nullptr);

	SDL_Surface * crop = SDL_CreateRGBSurfaceWithFormat(
		0, (illegalHex.getX() - neutralHex.getX()) * 44 + 45, 60, 32, SDL_PIXELFORMAT_ARGB8888);
	ASSERT_NE(crop, nullptr);
	SDL_Rect cropSource{
		hexTopLeft(neutralHex).x, hexTopLeft(neutralHex).y - 4, crop->w, crop->h};
	SDL_BlitSurface(surface, &cropSource, crop, nullptr);
	SDLImageShared cropImage(crop);
	cropImage.exportBitmap(outputDir / "focus-tiers-crop.png", nullptr);

	// PLAYER_PERCEIVABLE sampling at hex interiors (highlight gradient core)
	auto interiorPixel = [&](const BattleHex & hex)
	{
		return pixelAt(surface, hexTopLeft(hex) + Point(22, 22));
	};

	const auto neutralPixel = interiorPixel(neutralHex);
	const auto movablePixel = interiorPixel(movableHex);
	const auto attackablePixel = interiorPixel(attackableHex);
	const auto illegalPixel = interiorPixel(illegalHex);

	logGlobal->info(
		"focus tier frame evidence at %s: neutral=(%d,%d,%d) movable=(%d,%d,%d) attackable=(%d,%d,%d) illegal=(%d,%d,%d)",
		outputDir.string(),
		neutralPixel.r, neutralPixel.g, neutralPixel.b,
		movablePixel.r, movablePixel.g, movablePixel.b,
		attackablePixel.r, attackablePixel.g, attackablePixel.b,
		illegalPixel.r, illegalPixel.g, illegalPixel.b);

	// hue dominance of the frozen tier tints
	EXPECT_GT(movablePixel.g, movablePixel.r);
	EXPECT_GT(movablePixel.g, movablePixel.b);
	EXPECT_GT(attackablePixel.r, attackablePixel.g);
	EXPECT_GT(attackablePixel.r, attackablePixel.b);

	// non-color cue: illegal is dimmer than the neutral highlight on the same background
	EXPECT_LT(luminance(illegalPixel), luminance(neutralPixel));

	// every tier must differ from its background enough to be perceivable
	const auto backgroundPixel = pixelAt(surface, hexTopLeft(BattleHex(12, 5)) + Point(22, 22));
	for(const auto & pixel : {neutralPixel, movablePixel, attackablePixel, illegalPixel})
	{
		const int distance = std::abs(pixel.r - backgroundPixel.r)
			+ std::abs(pixel.g - backgroundPixel.g)
			+ std::abs(pixel.b - backgroundPixel.b);
		EXPECT_GT(distance, 48);
	}
}

/// Composes the D2 controller movement preview for an attacker-side
/// double-wide stack through the same production primitives the battlefield
/// uses (UnitMovementHighlight range, CCELLSHD landing shadow with dark
/// border, MOVABLE tier focus marker on the head hex) and exports evidence.
TEST_F(BattleFocusTierFrameTest, WideUnitMovePreviewLandingShadowCoversBothHexes)
{
	auto & renderer = ENGINE->renderHandler();

	auto background = renderer.loadImage(ImagePath::builtin("CMBKDES.BMP"), EImageBlitMode::OPAQUE);
	auto cellBorder = renderer.loadImage(ImagePath::builtin("CCELLGRD.BMP"), EImageBlitMode::COLORKEY);
	auto cellShade = renderer.loadImage(ImagePath::builtin("CCELLSHD.BMP"), EImageBlitMode::SIMPLE);
	auto movementHighlight = renderer.loadImage(ImagePath::builtin("UnitMovementHighlight.PNG"), EImageBlitMode::COLORKEY);

	ASSERT_GT(background->width(), 0);
	ASSERT_GT(background->height(), 0);

	SDL_Surface * surface = SDL_CreateRGBSurfaceWithFormat(
		0, background->width(), background->height(), 32, SDL_PIXELFORMAT_ARGB8888);
	ASSERT_NE(surface, nullptr);
	std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surfaceOwner(surface, SDL_FreeSurface);

	Canvas canvas = Canvas::createFromSurface(surface, CanvasScalingPolicy::IGNORE);
	canvas.draw(background, Point(0, 0));

	for(int hex = 0; hex < GameConstants::BFIELD_SIZE; ++hex)
	{
		if(hex % GameConstants::BFIELD_WIDTH == 0)
			continue;
		if(hex % GameConstants::BFIELD_WIDTH == GameConstants::BFIELD_WIDTH - 1)
			continue;
		canvas.draw(cellBorder, hexTopLeft(hex));
	}

	// attacker-side double-wide landing: the head hex plus the tail hex one
	// column to the left, exactly as battle::Unit::occupiedHex computes it
	const BattleHex headHex(7, 5);
	const BattleHex tailHex(headHex.toInt() - 1);
	ASSERT_EQ(tailHex, BattleHex(6, 5));

	// movement range highlight on a few representative reachable hexes
	for(const BattleHex & hex : {BattleHex(4, 5), BattleHex(5, 4), BattleHex(9, 5)})
		canvas.draw(movementHighlight, hexTopLeft(hex));

	// landing shadow with dark border on both occupied hexes, mirroring the
	// official mouse-shadow path (cellShade drawn with darkBorder = true)
	for(const BattleHex & hex : {headHex, tailHex})
	{
		canvas.draw(cellShade, hexTopLeft(hex));
		canvas.draw(cellBorder, hexTopLeft(hex));
	}

	// controller focus marker sits on the head hex only (D8)
	const auto render = BattleFocusHighlights::loadTierRender(BattleFocusTier::Tier::MOVABLE);
	if(render.shadeOverlay)
		canvas.draw(cellShade, hexTopLeft(headHex));
	canvas.draw(render.highlight, hexTopLeft(headHex));
	if(render.borderOverlay)
		canvas.draw(cellBorder, hexTopLeft(headHex));

	const auto outputDir = captureDir().parent_path() / "m3-1-move-preview";
	boost::filesystem::create_directories(outputDir);

	SDLImageShared frame(surface);
	frame.exportBitmap(outputDir / "move-preview-full.png", nullptr);

	SDL_Surface * crop = SDL_CreateRGBSurfaceWithFormat(0, 2 * 44 + 45, 60, 32, SDL_PIXELFORMAT_ARGB8888);
	ASSERT_NE(crop, nullptr);
	SDL_Rect cropSource{hexTopLeft(tailHex).x, hexTopLeft(tailHex).y - 4, crop->w, crop->h};
	SDL_BlitSurface(surface, &cropSource, crop, nullptr);
	SDLImageShared cropImage(crop);
	cropImage.exportBitmap(outputDir / "move-preview-crop.png", nullptr);

	auto interiorPixel = [&](const BattleHex & hex)
	{
		return pixelAt(surface, hexTopLeft(hex) + Point(22, 22));
	};

	const auto headPixel = interiorPixel(headHex);
	const auto tailPixel = interiorPixel(tailHex);
	const auto backgroundPixel = interiorPixel(BattleHex(12, 5));
	const auto unshadedNeighborPixel = interiorPixel(BattleHex(8, 5));

	logGlobal->info(
		"move preview frame evidence at %s: head=(%d,%d,%d) tail=(%d,%d,%d) background=(%d,%d,%d)",
		outputDir.string(),
		headPixel.r, headPixel.g, headPixel.b,
		tailPixel.r, tailPixel.g, tailPixel.b,
		backgroundPixel.r, backgroundPixel.g, backgroundPixel.b);

	// head hex carries the frozen green movable focus marker
	EXPECT_GT(headPixel.g, headPixel.r);
	EXPECT_GT(headPixel.g, headPixel.b);

	// both landing hexes are shaded darker than the unshaded background
	EXPECT_LT(luminance(tailPixel), luminance(backgroundPixel));
	EXPECT_LT(luminance(tailPixel), luminance(unshadedNeighborPixel));

	// tail and head must not look identical: only the head has the marker
	const int headTailDistance = std::abs(headPixel.r - tailPixel.r)
		+ std::abs(headPixel.g - tailPixel.g)
		+ std::abs(headPixel.b - tailPixel.b);
	EXPECT_GT(headTailDistance, 24);
}

/// Composes the D3 melee approach choice through the same production
/// primitives the battlefield uses (ATTACKABLE tier render on every
/// approach hex, white BattleDirectionArrow from the chosen origin to the
/// target focus) and exports evidence.
TEST_F(BattleFocusTierFrameTest, MeleeDirectionArrowPointsAtTheFocusedTarget)
{
	auto & renderer = ENGINE->renderHandler();

	auto background = renderer.loadImage(ImagePath::builtin("CMBKDES.BMP"), EImageBlitMode::OPAQUE);
	auto cellBorder = renderer.loadImage(ImagePath::builtin("CCELLGRD.BMP"), EImageBlitMode::COLORKEY);
	auto cellShade = renderer.loadImage(ImagePath::builtin("CCELLSHD.BMP"), EImageBlitMode::SIMPLE);

	ASSERT_GT(background->width(), 0);
	ASSERT_GT(background->height(), 0);

	SDL_Surface * surface = SDL_CreateRGBSurfaceWithFormat(
		0, background->width(), background->height(), 32, SDL_PIXELFORMAT_ARGB8888);
	ASSERT_NE(surface, nullptr);
	std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surfaceOwner(surface, SDL_FreeSurface);

	Canvas canvas = Canvas::createFromSurface(surface, CanvasScalingPolicy::IGNORE);
	canvas.draw(background, Point(0, 0));

	for(int hex = 0; hex < GameConstants::BFIELD_SIZE; ++hex)
	{
		if(hex % GameConstants::BFIELD_WIDTH == 0)
			continue;
		if(hex % GameConstants::BFIELD_WIDTH == GameConstants::BFIELD_WIDTH - 1)
			continue;
		canvas.draw(cellBorder, hexTopLeft(hex));
	}

	// enemy target focus on the right, own stack approaches from the left;
	// two alternative approach hexes flank the chosen one above and below
	const BattleHex targetHex(7, 5);
	const BattleHex chosenOrigin(5, 5);
	const std::vector<BattleHex> origins = {chosenOrigin, BattleHex(6, 4), BattleHex(5, 6)};

	// ATTACKABLE tier render marks every approach hex
	const auto render = BattleFocusHighlights::loadTierRender(BattleFocusTier::Tier::ATTACKABLE);
	for(const auto & origin : origins)
	{
		const Point originPos = hexTopLeft(origin);
		if(render.shadeOverlay)
			canvas.draw(cellShade, originPos);
		canvas.draw(render.highlight, originPos);
		if(render.borderOverlay)
			canvas.draw(cellBorder, originPos);
	}

	// direction arrow from the chosen origin center toward the target center
	const Point from = hexTopLeft(chosenOrigin) + Point(22, 22);
	const Point to = hexTopLeft(targetHex) + Point(22, 22);
	BattleDirectionArrow::draw(canvas, from, to, ColorRGBA(255, 255, 255, 255));

	// recommendation contract sanity used by the wiring
	ASSERT_EQ(BattleAttackDirection::recommend(origins), chosenOrigin);

	const auto outputDir = captureDir().parent_path() / "m3-1-attack-direction";
	boost::filesystem::create_directories(outputDir);

	SDLImageShared frame(surface);
	frame.exportBitmap(outputDir / "attack-direction-full.png", nullptr);

	// crop spans from the chosen origin through the target hex
	SDL_Surface * crop = SDL_CreateRGBSurfaceWithFormat(
		0, (targetHex.getX() - chosenOrigin.getX()) * 44 + 45, 60, 32, SDL_PIXELFORMAT_ARGB8888);
	ASSERT_NE(crop, nullptr);
	SDL_Rect cropSource{hexTopLeft(chosenOrigin).x, hexTopLeft(chosenOrigin).y - 4, crop->w, crop->h};
	SDL_BlitSurface(surface, &cropSource, crop, nullptr);
	SDLImageShared cropImage(crop);
	cropImage.exportBitmap(outputDir / "attack-direction-crop.png", nullptr);

	// shaft midpoint sits inside the empty hex between origin and target
	const auto shaftPixel = pixelAt(surface, Point((from.x + to.x) / 2, (from.y + to.y) / 2));
	// lower wing tip computed with the exact production geometry
	const double wingAngle = std::atan2(static_cast<double>(from.y - to.y), static_cast<double>(from.x - to.x)) - 0.5;
	const Point wingTip(to.x + static_cast<int>(std::cos(wingAngle) * 9), to.y + static_cast<int>(std::sin(wingAngle) * 9));
	const auto wingPixel = pixelAt(surface, wingTip);
	const auto untouchedPixel = pixelAt(surface, hexTopLeft(BattleHex(12, 5)) + Point(22, 22));

	logGlobal->info(
		"attack direction frame evidence at %s: shaft=(%d,%d,%d) wing=(%d,%d,%d) background=(%d,%d,%d)",
		outputDir.string(),
		shaftPixel.r, shaftPixel.g, shaftPixel.b,
		wingPixel.r, wingPixel.g, wingPixel.b,
		untouchedPixel.r, untouchedPixel.g, untouchedPixel.b);

	// the arrow is white and clearly brighter than the battlefield it crosses
	EXPECT_GT(luminance(shaftPixel), luminance(untouchedPixel));
	EXPECT_GT(luminance(shaftPixel), 200);
	EXPECT_GT(luminance(wingPixel), 200);

	// approach hexes carry the frozen red attackable tint; sample the flank
	// origin because the arrow shaft crosses the chosen origin's center
	const auto originPixel = pixelAt(surface, hexTopLeft(BattleHex(6, 4)) + Point(22, 22));
	EXPECT_GT(originPixel.r, originPixel.g);
	EXPECT_GT(originPixel.r, originPixel.b);
}
