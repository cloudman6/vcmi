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
#include "../client/render/CAnimation.h"
#include "../client/render/Canvas.h"
#include "../client/render/ColorFilter.h"
#include "../client/render/IImage.h"
#include "../client/render/IRenderHandler.h"
#include "../client/renderSDL/RenderHandler.h"
#include "../client/renderSDL/SDLImage.h"
#include "../client/renderSDL/ScreenHandler.h"
#include "../client/render/hdEdition/HdImageLoader.h"
#include "../lib/battle/BattleHex.h"
#include "../lib/battle/BattleHexArray.h"
#include "../lib/CConfigHandler.h"
#include "../lib/GameLibrary.h"
#include "../lib/filesystem/CFilesystemLoader.h"
#include "../lib/filesystem/Filesystem.h"
#include "../lib/filesystem/ResourcePath.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
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

/// Bounds-check-free variant for full-surface scans
ColorRGBA rawPixel(SDL_Surface * surface, int x, int y)
{
	SDL_Color color{0, 0, 0, 0};
	if(surface->format->palette)
		color = surface->format->palette->colors[
			static_cast<const uint8_t *>(surface->pixels)[y * surface->pitch + x]];
	else
	{
		Uint8 alpha = 0;
		SDL_GetRGBA(
			static_cast<const uint32_t *>(surface->pixels)[y * surface->w + x],
			surface->format, &color.r, &color.g, &color.b, &alpha);
	}
	return ColorRGBA(color.r, color.g, color.b, color.a);
}

int luminance(const ColorRGBA & color)
{
	return (299 * color.r + 587 * color.g + 114 * color.b) / 1000;
}

/// Statistics over one hex cell region. The official per-hex highlight is a
/// gradient that can be transparent at the cell center, so perceivability
/// must be judged by the strongest tint pixel in the cell, not one sample.
struct RegionStats
{
	int maxGreenDominance = -1000;
	int maxRedDominance = -1000;
	long luminanceSum = 0;
	int pixels = 0;
	int maxBackgroundDistance = 0;
	int strongGreenPixels = 0;

	int averageLuminance() const { return static_cast<int>(luminanceSum / pixels); }
};

RegionStats regionStats(SDL_Surface * surface, Point topLeft, const ColorRGBA & backgroundPixel, int interiorMargin = 0)
{
	RegionStats stats;
	for(int y = interiorMargin; y < 42 - interiorMargin; ++y)
		for(int x = interiorMargin; x < 44 - interiorMargin; ++x)
		{
			const auto pixel = pixelAt(surface, topLeft + Point(x, y));
			const int greenDominance = pixel.g - std::max(pixel.r, pixel.b);
			stats.maxGreenDominance = std::max(stats.maxGreenDominance, greenDominance);
			// above the green CCELLGRD grid (dominance 36) so only a real
			// MOVABLE tint ring counts
			if(greenDominance > 40)
				++stats.strongGreenPixels;
			stats.maxRedDominance = std::max(stats.maxRedDominance, pixel.r - std::max(pixel.g, pixel.b));
			stats.luminanceSum += luminance(pixel);
			++stats.pixels;
			const int distance = std::abs(pixel.r - backgroundPixel.r)
				+ std::abs(pixel.g - backgroundPixel.g)
				+ std::abs(pixel.b - backgroundPixel.b);
			stats.maxBackgroundDistance = std::max(stats.maxBackgroundDistance, distance);
		}
	return stats;
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
		// core vcmi mod sprites (range limit edge highlights) live in the
		// source checkout; a bare test binary has no installed mod mounts.
		// Depth matches the loader default so battle/rangeHighlights/*.json
		// one level down is indexed like a real mod install.
		CResourceHandler::addFilesystem(
			"initial",
			"battle-frame-repo-sprites",
			std::make_unique<CFilesystemLoader>("SPRITES/", sourceRoot / "Mods" / "vcmi" / "Content" / "Sprites", 16));
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

	// PLAYER_PERCEIVABLE: region statistics over each hex cell, robust to the
	// official highlight gradient being transparent at the cell center
	const auto backgroundPixel = pixelAt(surface, hexTopLeft(BattleHex(12, 5)) + Point(22, 22));
	const auto neutralStats = regionStats(surface, hexTopLeft(neutralHex), backgroundPixel);
	const auto movableStats = regionStats(surface, hexTopLeft(movableHex), backgroundPixel);
	const auto attackableStats = regionStats(surface, hexTopLeft(attackableHex), backgroundPixel);
	const auto illegalStats = regionStats(surface, hexTopLeft(illegalHex), backgroundPixel);

	logGlobal->info(
		"focus tier frame evidence at %s: movable greenDom=%d attackable redDom=%d illegalLum=%d neutralLum=%d",
		outputDir.string(),
		movableStats.maxGreenDominance, attackableStats.maxRedDominance,
		illegalStats.averageLuminance(), neutralStats.averageLuminance());

	// hue dominance of the frozen tier tints, above the green CCELLGRD grid
	// (dominance 36) so the grid alone can never satisfy the oracle
	EXPECT_GT(movableStats.maxGreenDominance, 40);
	EXPECT_GT(attackableStats.maxRedDominance, 40);

	// non-color cue: illegal is dimmer than the neutral highlight on the same background
	EXPECT_LT(illegalStats.averageLuminance(), neutralStats.averageLuminance());

	// every tier must differ from its background enough to be perceivable
	for(const auto & stats : {neutralStats, movableStats, attackableStats, illegalStats})
		EXPECT_GT(stats.maxBackgroundDistance, 48);
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

	const auto backgroundPixel = interiorPixel(BattleHex(12, 5));
	const auto headStats = regionStats(surface, hexTopLeft(headHex), backgroundPixel);
	const auto tailStats = regionStats(surface, hexTopLeft(tailHex), backgroundPixel);
	const auto backgroundStats = regionStats(surface, hexTopLeft(BattleHex(12, 5)), backgroundPixel);
	const auto unshadedNeighborStats = regionStats(surface, hexTopLeft(BattleHex(8, 5)), backgroundPixel);

	logGlobal->info(
		"move preview frame evidence at %s: headGreenDom=%d headRingPx=%d tailRingPx=%d tailLum=%d backgroundLum=%d",
		outputDir.string(),
		headStats.maxGreenDominance, headStats.strongGreenPixels, tailStats.strongGreenPixels,
		tailStats.averageLuminance(), backgroundStats.averageLuminance());

	// head hex carries the frozen green movable focus marker
	EXPECT_GT(headStats.maxGreenDominance, 40);
	EXPECT_GT(headStats.strongGreenPixels, 60);

	// both landing hexes are shaded darker than the unshaded background
	EXPECT_LT(tailStats.averageLuminance(), backgroundStats.averageLuminance());
	EXPECT_LT(tailStats.averageLuminance(), unshadedNeighborStats.averageLuminance());

	// tail and head must not look identical: only the head has the marker
	EXPECT_LT(tailStats.strongGreenPixels, headStats.strongGreenPixels / 4);
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

	// approach hexes carry the frozen red attackable tint; scan the flank
	// origin because the arrow shaft crosses the chosen origin's center
	const auto originStats = regionStats(surface, hexTopLeft(BattleHex(6, 4)), untouchedPixel);
	EXPECT_GT(originStats.maxRedDominance, 30);
}

/// Composes the D4 shooter range limits through the same production assets
/// and edge-mask algorithm BattleFieldController uses (green full-damage
/// limit ring, red maximum shooting range ring) and exports evidence.
TEST_F(BattleFocusTierFrameTest, ShooterRangeLimitRingsComposeOnRealBattlefield)
{
	auto & renderer = ENGINE->renderHandler();

	auto background = renderer.loadImage(ImagePath::builtin("CMBKDES.BMP"), EImageBlitMode::OPAQUE);
	auto cellBorder = renderer.loadImage(ImagePath::builtin("CCELLGRD.BMP"), EImageBlitMode::COLORKEY);
	auto fullDamageLimit = renderer.loadAnimation(AnimationPath::builtin("battle/rangeHighlights/rangeHighlightsGreen.json"), EImageBlitMode::COLORKEY);
	auto shootingRangeLimit = renderer.loadAnimation(AnimationPath::builtin("battle/rangeHighlights/rangeHighlightsRed.json"), EImageBlitMode::COLORKEY);

	ASSERT_GT(background->width(), 0);
	// the edge mask table used by the production composer has 19 frames
	ASSERT_GE(static_cast<int>(fullDamageLimit->size()), 19);
	ASSERT_GE(static_cast<int>(shootingRangeLimit->size()), 19);

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

	// same edge masks as BattleFieldController's HexMasks table
	const std::map<int, int> maskToFrame =
	{
		{0b000001, 1}, {0b000010, 2}, {0b000100, 3}, {0b001000, 4}, {0b010000, 5}, {0b100000, 6},
		{0b000011, 7}, {0b011000, 8}, {0b000110, 9}, {0b001100, 10}, {0b110000, 11}, {0b100001, 12},
		{0b001010, 13}, {0b010001, 14}, {0b001110, 13}, {0b110001, 14},
		{0b000111, 15}, {0b011100, 16}, {0b111000, 17}, {0b100011, 18}
	};

	// mirrors BattleFieldController::calculateRangeLimitAndHighlightImages
	auto drawLimitRing = [&](const BattleHex & source, uint8_t distance, const std::shared_ptr<CAnimation> & images)
	{
		BattleHexArray rangeHexes;
		for(int i = 0; i < GameConstants::BFIELD_SIZE; ++i)
		{
			BattleHex hex(i);
			if(hex.isAvailable() && BattleHex::getDistance(source, hex) <= distance)
				rangeHexes.insert(hex);
		}

		for(const auto & hex : rangeHexes)
		{
			if(BattleHex::getDistance(source, hex) != distance)
				continue;

			int mask = 0;
			const BattleHexArray & neighbours = hex.getAllNeighbouringTiles();
			for(int direction = 0; direction < 6; ++direction)
			{
				if(!neighbours[direction].isAvailable())
					continue;
				if(!rangeHexes.contains(neighbours[direction]))
					mask |= 1 << direction;
			}

			if(mask == 0)
				continue;

			canvas.draw(images->getImage(maskToFrame.at(mask)), hexTopLeft(hex));
		}
	};

	// representative archer distances: full damage up to 3, maximum range 5
	const BattleHex shooterHex(5, 5);
	drawLimitRing(shooterHex, 3, fullDamageLimit);
	drawLimitRing(shooterHex, 5, shootingRangeLimit);

	const auto outputDir = captureDir().parent_path() / "m3-1-range-limits";
	boost::filesystem::create_directories(outputDir);

	SDLImageShared frame(surface);
	frame.exportBitmap(outputDir / "range-limits-full.png", nullptr);

	SDL_Surface * crop = SDL_CreateRGBSurfaceWithFormat(0, 11 * 44 + 45, 9 * 42 + 8, 32, SDL_PIXELFORMAT_ARGB8888);
	ASSERT_NE(crop, nullptr);
	SDL_Rect cropSource{hexTopLeft(BattleHex(1, 1)).x, hexTopLeft(BattleHex(1, 1)).y - 4, crop->w, crop->h};
	SDL_BlitSurface(surface, &cropSource, crop, nullptr);
	SDLImageShared cropImage(crop);
	cropImage.exportBitmap(outputDir / "range-limits-crop.png", nullptr);

	// PLAYER_PERCEIVABLE: both rings must contribute clearly tinted edge
	// pixels inside the crop region
	int greenEdgePixels = 0;
	int redEdgePixels = 0;
	SDL_LockSurface(surface);
	for(int y = 0; y < surface->h; ++y)
		for(int x = 0; x < surface->w; ++x)
		{
			const auto pixel = rawPixel(surface, x, y);
			if(pixel.g > pixel.r + 40 && pixel.g > pixel.b + 40)
				++greenEdgePixels;
			if(pixel.r > pixel.g + 40 && pixel.r > pixel.b + 40)
				++redEdgePixels;
		}
	SDL_UnlockSurface(surface);

	logGlobal->info(
		"range limit frame evidence at %s: greenEdgePixels=%d redEdgePixels=%d",
		outputDir.string(), greenEdgePixels, redEdgePixels);

	EXPECT_GT(greenEdgePixels, 200);
	EXPECT_GT(redEdgePixels, 200);
}
