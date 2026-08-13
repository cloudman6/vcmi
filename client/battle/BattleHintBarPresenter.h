/*
 * BattleHintBarPresenter.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../gui/CIntObject.h"
#include "../../lib/Color.h"

#include <vector>

struct BattleHintEntry;
class BattleInterface;
class Canvas;

/// Frozen layout constants of the battle controller hint bar (D6). The strip
/// renders between the embedded turn queue (y 10..59) and the top hex row
/// (y 86), so it never covers a playable hex, the D5 status host in the
/// bottom panel, or the bottom-panel controls.
namespace BattleHintBarLayout
{
	constexpr int TOP = 59;
	constexpr int HEIGHT = 27;
	constexpr int GLYPH_SIZE = 24;
	constexpr int GLYPH_TEXT_SPACING = 4;
	constexpr int ENTRY_SPACING = 14;
	constexpr int ACTION_PROMPT_WIDTH = 138;
	constexpr int ACTION_PROMPT_GAP = 3;
	constexpr int UNOBSCURED_LEFT = 79;
	constexpr int UNOBSCURED_TOP = 86;
	constexpr int UNOBSCURED_RIGHT = 721;
	constexpr int UNOBSCURED_BOTTOM = 555;
}

/// Direct-canvas renderer of the contextual hint bar, shared by the live
/// battle widget and the frame-evidence tests so the evidence frames use the
/// exact production drawing code.
class BattleHintBarPresenter
{
public:
	/// Frozen brown background of the bar (D6 freeze increment)
	static ColorRGBA backgroundColor();

	/// Draws the prompt strip into barRect; empty entries hide the bar
	static void draw(Canvas & to, const std::vector<BattleHintEntry> & entries, const Rect & barRect, bool acceptPressed);

	/// Places the focus-local action prompt without crossing sticky hero
	/// panels, the turn queue, or the bottom battle controls.
	static Rect actionPromptRect(const Rect & anchorRect, const Rect & unobscuredBattlefield);
};

/// Battle window child that recomputes the D6 hint entries from the live
/// interaction state each frame and draws them through the presenter.
class BattleHintBarWidget : public CIntObject
{
	BattleInterface & owner;
	bool acceptPressed = false;

public:
	explicit BattleHintBarWidget(BattleInterface & owner);

	/// M2 pressed convention: show the pressed glyph while accept is held
	void setAcceptPressed(bool on);

	void showAll(Canvas & to) override;
};
