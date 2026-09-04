/*
 * CSpellWindow.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "CSpellWindow.h"

#include "../../lib/ScopeGuard.h"

#include "GUIClasses.h"
#include "InfoWindows.h"
#include "CCastleInterface.h"

#include "../CPlayerInterface.h"
#include "../PlayerLocalState.h"

#include "../battle/BattleFieldController.h"
#include "../battle/BattleInterface.h"
#include "../eventsSDL/ControllerPromptFamily.h"
#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../gui/Shortcut.h"
#include "../gui/ShortcutHandler.h"
#include "../gui/WindowHandler.h"
#include "../media/IVideoPlayer.h"
#include "../render/CAnimation.h"
#include "../render/Canvas.h"
#include "../render/Colors.h"
#include "../render/IFont.h"
#include "../render/IImage.h"
#include "../render/IRenderHandler.h"
#include "../widgets/GraphicalPrimitiveCanvas.h"
#include "../widgets/CComponent.h"
#include "../widgets/CTextInput.h"
#include "../widgets/TextControls.h"
#include "../widgets/Buttons.h"
#include "../widgets/VideoWidget.h"
#include "../adventureMap/AdventureMapInterface.h"
#include "../eventsSDL/InputHandler.h"

#include "../../lib/CConfigHandler.h"
#include "../../lib/GameConstants.h"
#include "../../lib/GameLibrary.h"
#include "../../lib/battle/CPlayerBattleCallback.h"
#include "../../lib/callback/CCallback.h"
#include "../../lib/spells/ISpellMechanics.h"
#include "../../lib/spells/adventure/AdventureSpellEffect.h"
#include "../../lib/spells/Problem.h"
#include "../../lib/spells/SpellSchoolHandler.h"
#include "../../lib/texts/CGeneralTextHandler.h"
#include "../../lib/texts/TextOperations.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/spells/CSpellHandler.h"

namespace
{
constexpr uint32_t CONTROLLER_SPELL_FOCUS_SETTLE_MS = 16;
constexpr double CONTROLLER_SPELL_FOCUS_MIN_ALIGNMENT = 0.5;
constexpr int SPELL_SLOT_WIDTH = 83;
constexpr int SPELL_SLOT_HEIGHT = 97;
constexpr int SPELL_SLOT_CONTENT_OFFSET_X = 9;
constexpr int SPELL_FOCUS_CORNER_LENGTH = 13;
constexpr int SPELL_FOCUS_CORNER_THICKNESS = 2;
constexpr int CONTROLLER_PROMPT_GLYPH_SIZE = 24;
constexpr int CONTROLLER_PROMPT_TEXT_SPACING = 4;
constexpr int CONTROLLER_PROMPT_TEXT_OUTLINE = 1;
constexpr int CONTROLLER_PROMPT_TARGET_INSET = 6;
constexpr ColorRGBA SPELL_FOCUS_COLOR(210, 170, 70, 255);
constexpr ColorRGBA SPELL_FOCUS_BACKGROUND_COLOR(24, 16, 8, 96);

void drawControllerPromptText(Canvas & to, Point position, const std::string & text)
{
	to.drawText(position + Point(-CONTROLLER_PROMPT_TEXT_OUTLINE, 0), FONT_SMALL, Colors::BLACK, ETextAlignment::CENTER, text);
	to.drawText(position + Point(CONTROLLER_PROMPT_TEXT_OUTLINE, 0), FONT_SMALL, Colors::BLACK, ETextAlignment::CENTER, text);
	to.drawText(position + Point(0, -CONTROLLER_PROMPT_TEXT_OUTLINE), FONT_SMALL, Colors::BLACK, ETextAlignment::CENTER, text);
	to.drawText(position + Point(0, CONTROLLER_PROMPT_TEXT_OUTLINE), FONT_SMALL, Colors::BLACK, ETextAlignment::CENTER, text);
	to.drawText(position, FONT_SMALL, Colors::WHITE, ETextAlignment::CENTER, text);
}
}

// Ordering of spell school tabs in SpelTab.def
static const std::array schoolTabOrder =
{
	SpellSchool::AIR,
	SpellSchool::FIRE,
	SpellSchool::WATER,
	SpellSchool::EARTH,
	SpellSchool::ANY
};

int getAnimFrameFromSchool(SpellSchool school)
{
	auto it = std::find(schoolTabOrder.begin(), schoolTabOrder.end(), school);
	if (it != schoolTabOrder.end())
		return std::distance(schoolTabOrder.begin(), it);
	else
		return -1;
}

bool isLegacySpellSchool(SpellSchool school)
{
	return getAnimFrameFromSchool(school) != -1;
}

CSpellWindow::InteractiveArea::InteractiveArea(const Rect & myRect, const std::function<void()> & funcL, int helpTextId, CSpellWindow * _owner)
{
	addUsedEvents(LCLICK | SHOW_POPUP | HOVER);
	pos = myRect;
	onLeft = funcL;
	hoverText = LIBRARY->generaltexth->zelp[helpTextId].first;
	helpText = LIBRARY->generaltexth->zelp[helpTextId].second;
	owner = _owner;
}

CSpellWindow::InteractiveArea::InteractiveArea(const Rect & myRect, const std::function<void()> & funcL, std::string textId, CSpellWindow * _owner)
{
	addUsedEvents(LCLICK | SHOW_POPUP | HOVER);
	pos = myRect;
	onLeft = funcL;
	auto hoverTextTmp = MetaString::createFromTextID("vcmi.spellBook.tab.hover");
	hoverTextTmp.replaceTextID(textId);
	hoverText = hoverTextTmp.toString(&GAME->translator());
	auto helpTextTmp = MetaString::createFromTextID("vcmi.spellBook.tab.help");
	helpTextTmp.replaceTextID(textId);
	helpText = helpTextTmp.toString(&GAME->translator());
	owner = _owner;
}

void CSpellWindow::InteractiveArea::clickPressed(const Point & cursorPosition)
{
	ENGINE->input().hapticFeedback();
	onLeft();
}

void CSpellWindow::InteractiveArea::showPopupWindow(const Point & cursorPosition)
{
	CRClickPopup::createAndPush(helpText);
}

void CSpellWindow::InteractiveArea::hover(bool on)
{
	if(on)
		owner->statusBar->write(hoverText);
	else
		owner->statusBar->clear();
}

class SpellbookSpellSorter
{
public:
	bool operator()(const CSpell * A, const CSpell * B)
	{
		if(A->getLevel() < B->getLevel())
			return true;
		if(A->getLevel() > B->getLevel())
			return false;

		for (const auto schoolId : LIBRARY->spellSchoolHandler->getAllObjects())
		{
			if(A->schools.count(schoolId) && !B->schools.count(schoolId))
				return true;
			if(!A->schools.count(schoolId) && B->schools.count(schoolId))
				return false;
		}

		return TextOperations::compareLocalizedStrings(A->getNameTranslated(), B->getNameTranslated());
	}
};

CSpellWindow::CSpellWindow(const CGHeroInstance * _myHero, CPlayerInterface * _myInt, bool openOnBattleSpells, const std::function<void(SpellID)> & onSpellSelect):
	CWindowObject(PLAYER_COLORED | (settings["gameTweaks"]["enableLargeSpellbook"].Bool() ? BORDERED : 0)),
	battleSpellsOnly(openOnBattleSpells),
	selectedTab(SpellSchool::ANY),
	currentPage(0),
	myHero(_myHero),
	myInt(_myInt),
	openOnBattleSpells(openOnBattleSpells),
	onSpellSelect(onSpellSelect),
	isBigSpellbook(settings["gameTweaks"]["enableLargeSpellbook"].Bool()),
	spellsPerPage(24),
	offL(-11),
	offR(195),
	offRM(110),
	offT(-37),
	offB(56)
{
	OBJECT_CONSTRUCTION;

	int maxCustomSchools = (isBigSpellbook ? MAX_CUSTOM_SPELL_SCHOOLS_BIG : MAX_CUSTOM_SPELL_SCHOOLS) * 2;
	int customSchoolsAvailable = 0;
	std::vector<SpellSchool> sortedSchools = LIBRARY->spellSchoolHandler->getAllObjects();
	std::ranges::sort(sortedSchools, [&](SpellSchool a, SpellSchool b) {
		auto cnt = [&](SpellSchool s) {
			return std::ranges::count_if(LIBRARY->spellh->objects, [&](auto const & sp) {
				return myHero->canCastThisSpell(sp.get()) && sp->schools.count(s);
			});
		};
		return cnt(a) > cnt(b);
	});
	for(const auto schoolId : sortedSchools)
		if(
			!isLegacySpellSchool(schoolId) &&
			!LIBRARY->spellSchoolHandler->getById(schoolId)->getSchoolBookmarkPath().empty() &&
			!LIBRARY->spellSchoolHandler->getById(schoolId)->getSchoolHeaderPath().empty()
		)
		{
			customSchoolsAvailable++;
			if(customSpellSchools.size() < maxCustomSchools)
				customSpellSchools.push_back(schoolId);
		}

	if(customSchoolsAvailable > maxCustomSchools)
		logGlobal->warn("Too many custom spell schools (%d) — showing only first %d", customSchoolsAvailable, maxCustomSchools);

	if(isBigSpellbook)
	{
		background = std::make_shared<CPicture>(ImagePath::builtin("SpellBookLarge"), 0, 0);
		updateShadow();
	}
	else
	{
		background = std::make_shared<CPicture>(ImagePath::builtin("SpelBack"), 0, 0);
		offL = offR = offT = offB = offRM = 0;
		spellsPerPage = 12;
	}

	background->setPlayerColor(GAME->interface()->playerID);

	pos = background->center(Point(pos.w/2 + pos.x, pos.h/2 + pos.y));

	Rect r(90, isBigSpellbook ? 480 : 420, isBigSpellbook ? 160 : 110, 16);
	if(settings["general"]["enableUiEnhancements"].Bool())
	{
		const ColorRGBA rectangleColor = ColorRGBA(0, 0, 0, 75);
		const ColorRGBA borderColor = ColorRGBA(128, 100, 75);
		const ColorRGBA grayedColor = ColorRGBA(158, 130, 105);
		searchBoxRectangle = std::make_shared<TransparentFilledRectangle>(r.resize(1), rectangleColor, borderColor);
		searchBoxDescription = std::make_shared<CLabel>(r.center().x, r.center().y, FONT_SMALL, ETextAlignment::CENTER, grayedColor, LIBRARY->generaltexth->translate("vcmi.spellBook.search"));

		searchBox = std::make_shared<CTextInput>(r, FONT_SMALL, ETextAlignment::CENTER, false);
		searchBox->setCallback(std::bind(&CSpellWindow::searchInput, this));
	}

	if(onSpellSelect)
	{
		Point boxPos = r.bottomLeft() + Point(-2, 5);
		showAllSpells = std::make_shared<CToggleButton>(boxPos, AnimationPath::builtin("sysopchk.def"), CButton::tooltip(LIBRARY->generaltexth->translate("core.help.458.hover"), LIBRARY->generaltexth->translate("core.help.458.hover")), [this](bool state){ searchInput(); });
		showAllSpellsDescription = std::make_shared<CLabel>(boxPos.x + 40, boxPos.y + 12, FONT_SMALL, ETextAlignment::CENTERLEFT, Colors::WHITE, LIBRARY->generaltexth->translate("core.help.458.hover"));
	}

	processSpells();

	//numbers of spell pages computed

	leftCorner = std::make_shared<CPicture>(ImagePath::builtin("SpelTrnL.bmp"), 97 + offL, 77 + offT);
	rightCorner = std::make_shared<CPicture>(ImagePath::builtin("SpelTrnR.bmp"), 487 + offR, 72 + offT);

	schoolTab = std::make_shared<CAnimImage>(AnimationPath::builtin("SpelTab"), getAnimFrameFromSchool(selectedTab), 0, 524 + offR, 88);
	int customSchoolCount = customSpellSchools.size();
	int yStart = 93;
	int yEnd = yStart + (std::min(customSchoolCount, isBigSpellbook ? MAX_CUSTOM_SPELL_SCHOOLS_BIG : MAX_CUSTOM_SPELL_SCHOOLS) - 1) * 62;
	int denom = std::max(customSchoolCount - 1, 1);
	for(int i = 0; i < customSchoolCount; i++)
		schoolTabCustom.push_back(std::make_shared<CAnimImage>(LIBRARY->spellSchoolHandler->getById(customSpellSchools[i])->getSchoolBookmarkPath(), i == 0 ? 0 : 1, 0, isBigSpellbook ? 0 : 15, yStart + ((yEnd - yStart) * i) / denom));
	schoolPicture = std::make_shared<CAnimImage>(AnimationPath::builtin("Schools"), 0, 0, 117 + offL, 74 + offT);

	mana = std::make_shared<CLabel>(435 + (isBigSpellbook ? 159 : 0), 426 + offB, FONT_SMALL, ETextAlignment::CENTER, Colors::YELLOW, std::to_string(myHero->mana));

	if(isBigSpellbook)
		statusBar = CGStatusBar::create(400, 587);
	else
		statusBar = CGStatusBar::create(7, 569, ImagePath::builtin("Spelroll.bmp"));

	Rect schoolRect( 549 + pos.x + offR, 94 + pos.y, 45, 35);
	interactiveAreas.push_back(std::make_shared<InteractiveArea>( Rect( 479 + pos.x + (isBigSpellbook ? 175 : 0), 405 + pos.y + offB, isBigSpellbook ? 60 : 36, 56), std::bind(&CSpellWindow::fexitb,         this),    460, this));
	interactiveAreas.push_back(std::make_shared<InteractiveArea>( Rect( 221 + pos.x + (isBigSpellbook ? 43 : 0), 405 + pos.y + offB, isBigSpellbook ? 60 : 36, 56), std::bind(&CSpellWindow::fbattleSpellsb, this),    453, this));
	interactiveAreas.push_back(std::make_shared<InteractiveArea>( Rect( 355 + pos.x + (isBigSpellbook ? 110 : 0), 405 + pos.y + offB, isBigSpellbook ? 60 : 36, 56), std::bind(&CSpellWindow::fadvSpellsb,    this),    452, this));
	interactiveAreas.push_back(std::make_shared<InteractiveArea>( Rect( 418 + pos.x + (isBigSpellbook ? 142 : 0), 405 + pos.y + offB, isBigSpellbook ? 60 : 36, 56), std::bind(&CSpellWindow::fmanaPtsb,      this),    459, this));
	interactiveAreas.push_back(std::make_shared<InteractiveArea>( schoolRect + Point(0, 0),   std::bind(&CSpellWindow::selectSchool,   this, SpellSchool::AIR), 454, this));
	interactiveAreas.push_back(std::make_shared<InteractiveArea>( schoolRect + Point(0, 57),  std::bind(&CSpellWindow::selectSchool,   this, SpellSchool::EARTH), 457, this));
	interactiveAreas.push_back(std::make_shared<InteractiveArea>( schoolRect + Point(0, 116), std::bind(&CSpellWindow::selectSchool,   this, SpellSchool::FIRE), 455, this));
	interactiveAreas.push_back(std::make_shared<InteractiveArea>( schoolRect + Point(0, 176), std::bind(&CSpellWindow::selectSchool,   this, SpellSchool::WATER), 456, this));
	interactiveAreas.push_back(std::make_shared<InteractiveArea>( schoolRect + Point(0, 236), std::bind(&CSpellWindow::selectSchool,   this, SpellSchool::ANY), 458, this));
	int iaHeight = customSchoolCount > 1 ? std::min((yEnd - yStart) / denom, 60) : 60;
	for(int i = 0; i < customSchoolCount; i++)
		interactiveAreas.push_back(std::make_shared<InteractiveArea>(Rect(schoolTabCustom[i]->pos.topLeft(), Point(80, iaHeight)), std::bind(&CSpellWindow::selectSchool, this, customSpellSchools[i]), LIBRARY->spellSchoolHandler->getById(customSpellSchools[i])->getNameTextID(), this));

	leftCornerArea = std::make_shared<InteractiveArea>( Rect(  97 + offL + pos.x, 77 + offT + pos.y, leftCorner->pos.h,  leftCorner->pos.w  ), std::bind(&CSpellWindow::fLcornerb, this), 450, this);
	rightCornerArea = std::make_shared<InteractiveArea>( Rect( 487 + offR + pos.x, 72 + offT + pos.y, rightCorner->pos.h, rightCorner->pos.w ), std::bind(&CSpellWindow::fRcornerb, this), 451, this);

	//areas for spells
	int xpos = 117 + offL + pos.x;
	int ypos = 90 + offT + pos.y;

	for(int v=0; v<spellsPerPage; ++v)
	{
		spellAreas[v] = std::make_shared<SpellArea>(
			Rect(xpos - SPELL_SLOT_CONTENT_OFFSET_X, ypos, SPELL_SLOT_WIDTH, SPELL_SLOT_HEIGHT),
			this);

		if(v == (spellsPerPage / 2) - 1) //to right page
		{
			xpos = offRM + 336 + pos.x; ypos = 90 + offT + pos.y;
		}
		else
		{
			if(v%(isBigSpellbook ? 3 : 2) == 0 || (v%3 == 1 && isBigSpellbook))
			{
				xpos+=85;
			}
			else
			{
				xpos -= (isBigSpellbook ? 2 : 1)*85; ypos+=97;
			}
		}
	}

	SpellSchool school = battleSpellsOnly ? myInt->localState->getSpellbookSettings().spellbookLastTabBattle : myInt->localState->getSpellbookSettings().spellbookLastTabAdvmap;
	bool schoolFound = isLegacySpellSchool(school) || std::find(customSpellSchools.begin(), customSpellSchools.end(), school) != customSpellSchools.end();
	if(schoolFound)
		selectedTab = school;
	setSchoolImages(selectedTab);
	int cp = battleSpellsOnly ? myInt->localState->getSpellbookSettings().spellbookLastPageBattle : myInt->localState->getSpellbookSettings().spellbookLastPageAdvmap;
	// spellbook last page battle index is not reset after battle, so this needs to stay here
	vstd::abetween(cp, 0, std::max(0, pagesWithinCurrentTab() - 1));
	if(!schoolFound)
		cp = 0;
	setCurrentPage(cp);
	computeSpellsPerArea();
	addUsedEvents(KEYBOARD | TIME | INPUT_MODE_CHANGE);
}

CSpellWindow::~CSpellWindow()
{
}

void CSpellWindow::searchInput()
{
	if(searchBox)
		searchBoxDescription->setEnabled(searchBox->getText().empty());

	processSpells();

	int cp = 0;
	// spellbook last page battle index is not reset after battle, so this needs to stay here
	vstd::abetween(cp, 0, std::max(0, pagesWithinCurrentTab() - 1));
	setCurrentPage(cp);
	computeSpellsPerArea();
}

void CSpellWindow::processSpells()
{
	mySpells.clear();
	sitesPerTabAdv.clear(); // hold page counts of previous run - would be added up otherwise
	sitesPerTabBattle.clear();

	//initializing castable spells
	mySpells.reserve(LIBRARY->spellh->objects.size());
	for(auto const & spell : LIBRARY->spellh->objects)
	{
		bool searchTextFound = !searchBox || TextOperations::isFuzzyMatch(searchBox->getText(), spell->getNameTranslated());

		if(onSpellSelect)
		{
			bool spellAvailable = myHero->canCastThisSpell(spell.get()) || (showAllSpells->isSelected() && !spell->isSpecial());

			if(spell->isCombat() == openOnBattleSpells
				&& !spell->isCreatureAbility()
				&& searchTextFound
				&& spellAvailable)
			{
				mySpells.push_back(spell.get());
			}
			continue;
		}

		if(!spell->isCreatureAbility() && myHero->canCastThisSpell(spell.get()) && searchTextFound)
			mySpells.push_back(spell.get());
	}

	SpellbookSpellSorter spellsorter;
	std::sort(mySpells.begin(), mySpells.end(), spellsorter);

	for(const auto spell : mySpells)
	{
		auto& sitesPerOurTab = spell->isCombat() ? sitesPerTabBattle : sitesPerTabAdv;

		++sitesPerOurTab[SpellSchool::ANY];

		spell->forEachSchool([&sitesPerOurTab](const SpellSchool & school, bool & stop)
		{
			++sitesPerOurTab[school];
		});
	}
	if(sitesPerTabAdv[SpellSchool::ANY] % spellsPerPage == 0)
		sitesPerTabAdv[SpellSchool::ANY]/=spellsPerPage;
	else
		sitesPerTabAdv[SpellSchool::ANY] = sitesPerTabAdv[SpellSchool::ANY]/spellsPerPage + 1;

	for(const auto v : LIBRARY->spellSchoolHandler->getAllObjects())
	{
		if(v == SpellSchool::ANY)
			continue;
		if(sitesPerTabAdv[v] <= spellsPerPage - 2)
			sitesPerTabAdv[v] = 1;
		else
		{
			if((sitesPerTabAdv[v] - (spellsPerPage - 2)) % spellsPerPage == 0)
				sitesPerTabAdv[v] = (sitesPerTabAdv[v] - (spellsPerPage - 2)) / spellsPerPage + 1;
			else
				sitesPerTabAdv[v] = (sitesPerTabAdv[v] - (spellsPerPage - 2)) / spellsPerPage + 2;
		}
	}

	if(sitesPerTabBattle[SpellSchool::ANY] % spellsPerPage == 0)
		sitesPerTabBattle[SpellSchool::ANY]/=spellsPerPage;
	else
		sitesPerTabBattle[SpellSchool::ANY] = sitesPerTabBattle[SpellSchool::ANY]/spellsPerPage + 1;

	for(const auto v : LIBRARY->spellSchoolHandler->getAllObjects())
	{
		if(v == SpellSchool::ANY)
			continue;
		if(sitesPerTabBattle[v] <= spellsPerPage - 2)
			sitesPerTabBattle[v] = 1;
		else
		{
			if((sitesPerTabBattle[v] - (spellsPerPage - 2)) % spellsPerPage == 0)
				sitesPerTabBattle[v] = (sitesPerTabBattle[v] - (spellsPerPage - 2)) / spellsPerPage + 1;
			else
				sitesPerTabBattle[v] = (sitesPerTabBattle[v] - (spellsPerPage - 2)) / spellsPerPage + 2;
		}
	}
}

void CSpellWindow::fexitb()
{
	auto spellBookState = myInt->localState->getSpellbookSettings();
	if(myInt->battleInt)
	{
		spellBookState.spellbookLastTabBattle = selectedTab;
		spellBookState.spellbookLastPageBattle = currentPage;
	}
	else
	{
		spellBookState.spellbookLastTabAdvmap = selectedTab;
		spellBookState.spellbookLastPageAdvmap = currentPage;
	}
	myInt->localState->setSpellbookSettings(spellBookState);

	if(onSpellSelect)
		onSpellSelect(SpellID::NONE);

	close();
}

void CSpellWindow::fadvSpellsb()
{
	if(battleSpellsOnly == true)
	{
		turnPageRight();
		battleSpellsOnly = false;
		setCurrentPage(0);
	}
	computeSpellsPerArea();
}

void CSpellWindow::fbattleSpellsb()
{
	if(battleSpellsOnly == false)
	{
		turnPageLeft();
		battleSpellsOnly = true;
		setCurrentPage(0);
	}
	computeSpellsPerArea();
}

void CSpellWindow::toggleSearchBoxFocus()
{
	if(searchBox != nullptr)
	{
		searchBox->hasFocus() ? searchBox->removeFocus() : searchBox->giveFocus();
	}
}

void CSpellWindow::fmanaPtsb()
{
}

void CSpellWindow::selectSchool(SpellSchool school)
{
	if(selectedTab != school)
	{
		if(selectedTab < school)
			turnPageLeft();
		else
			turnPageRight();
		selectedTab = school;
		setSchoolImages(selectedTab);
		setCurrentPage(0);
	}
	computeSpellsPerArea();
}

void CSpellWindow::fLcornerb()
{
	if(currentPage>0)
	{
		turnPageLeft();
		setCurrentPage(currentPage - 1);
	}
	computeSpellsPerArea();
}

void CSpellWindow::fRcornerb()
{
	if((currentPage + 1) < (pagesWithinCurrentTab()))
	{
		turnPageRight();
		setCurrentPage(currentPage + 1);
	}
	computeSpellsPerArea();
}

void CSpellWindow::show(Canvas & to)
{
	if(video)
		video->show(to);
	statusBar->show(to);
	drawControllerPrompts(to);
}

const std::shared_ptr<IImage> & CSpellWindow::controllerPromptSprite(const std::string & path)
{
	auto cached = controllerPromptSprites.find(path);
	if(cached == controllerPromptSprites.end())
	{
		auto sprite = ENGINE->renderHandler().loadImage(ImagePath::builtin(path), EImageBlitMode::COLORKEY);
		cached = controllerPromptSprites.emplace(path, std::move(sprite)).first;
	}
	return cached->second;
}

void CSpellWindow::drawControllerPrompts(Canvas & to)
{
	if(!usesNativeSpellbookNavigation() || !ENGINE->windows().isTopWindow(this))
		return;

	const auto family = ENGINE->input().getActiveControllerPromptFamily();
	auto bindingFor = [](EShortcut shortcut)
	{
		const auto bindings = ENGINE->shortcuts().getJoystickButtonBindings(shortcut);
		return bindings.size() == 1 ? bindings.front() : std::string();
	};
	auto drawBinding = [this, &to, family](Point center, const std::string & binding, const std::optional<std::string> & spritePath)
	{
		if(spritePath)
		{
			const auto & sprite = controllerPromptSprite(*spritePath);
			to.draw(sprite, center - sprite->dimensions() / 2);
		}
		else
			drawControllerPromptText(to, center, binding.empty() ? "--" : ControllerPrompt::buttonLabel(family, binding));
	};
	auto drawFacePrompt = [&to, family, &bindingFor, &drawBinding](Point topLeft, EShortcut shortcut, const std::string & text)
	{
		const std::string binding = bindingFor(shortcut);
		drawBinding(topLeft + Point(CONTROLLER_PROMPT_GLYPH_SIZE / 2, CONTROLLER_PROMPT_GLYPH_SIZE / 2), binding,
			ControllerPrompt::faceButtonSprite(family, binding, ControllerPrompt::State::NORMAL));
		if(!text.empty())
		{
			const auto & font = ENGINE->renderHandler().loadFont(FONT_SMALL);
			const int width = static_cast<int>(font->getStringWidth(text)) + CONTROLLER_PROMPT_TEXT_OUTLINE * 2;
			drawControllerPromptText(
				to,
				topLeft + Point(CONTROLLER_PROMPT_GLYPH_SIZE + CONTROLLER_PROMPT_TEXT_SPACING + width / 2, CONTROLLER_PROMPT_GLYPH_SIZE / 2),
				text);
		}
	};
	auto drawDpadPrompt = [&bindingFor, &drawBinding](Point center, EShortcut shortcut)
	{
		const std::string binding = bindingFor(shortcut);
		drawBinding(center, binding, ControllerPrompt::directionalPadSprite(binding));
	};
	auto drawShoulderPrompt = [this, &to, family, &bindingFor](Point center, EShortcut shortcut, bool previous)
	{
		const auto previousBindings = ENGINE->shortcuts().getJoystickButtonBindings(EShortcut::BATTLE_DEFEND);
		const auto nextBindings = ENGINE->shortcuts().getJoystickButtonBindings(EShortcut::BATTLE_WAIT);
		const auto pairSprite = ControllerPrompt::shoulderPairSprite(family, previousBindings, nextBindings);
		if(pairSprite)
		{
			const auto & sprite = controllerPromptSprite(*pairSprite);
			const int glyphWidth = sprite->dimensions().x / 2;
			to.draw(
				sprite,
				center - Point(glyphWidth / 2, sprite->dimensions().y / 2),
				Rect(previous ? 0 : glyphWidth, 0, glyphWidth, sprite->dimensions().y));
		}
		else
		{
			const std::string binding = bindingFor(shortcut);
			drawControllerPromptText(to, center, binding.empty() ? "--" : ControllerPrompt::buttonLabel(family, binding));
		}
	};

	if(currentPage > 0)
		drawShoulderPrompt(leftCorner->pos.center(), EShortcut::BATTLE_DEFEND, true);
	if(currentPage + 1 < pagesWithinCurrentTab())
		drawShoulderPrompt(rightCorner->pos.center(), EShortcut::BATTLE_WAIT, false);

	drawDpadPrompt(Point(schoolTab->pos.center().x, schoolTab->pos.top() - CONTROLLER_PROMPT_GLYPH_SIZE / 2 - CONTROLLER_PROMPT_TARGET_INSET), EShortcut::MOVE_UP);
	drawDpadPrompt(Point(schoolTab->pos.center().x, schoolTab->pos.bottom() + CONTROLLER_PROMPT_GLYPH_SIZE / 2), EShortcut::MOVE_DOWN);

	const int bookmarkPromptY = pos.y + 405 + offB + 56 + CONTROLLER_PROMPT_GLYPH_SIZE / 2 - CONTROLLER_PROMPT_TARGET_INSET;
	const int battleBookmarkCenterX = pos.x + 221 + (isBigSpellbook ? 43 : 0) + (isBigSpellbook ? 60 : 36) / 2;
	const int adventureBookmarkCenterX = pos.x + 355 + (isBigSpellbook ? 110 : 0) + (isBigSpellbook ? 60 : 36) / 2;
	const int exitBookmarkCenterX = pos.x + 479 + (isBigSpellbook ? 175 : 0) + (isBigSpellbook ? 60 : 36) / 2;
	if(!battleSpellsOnly)
		drawDpadPrompt(Point(battleBookmarkCenterX, bookmarkPromptY), EShortcut::MOVE_LEFT);
	if(battleSpellsOnly)
		drawDpadPrompt(Point(adventureBookmarkCenterX, bookmarkPromptY), EShortcut::MOVE_RIGHT);
	drawFacePrompt(Point(exitBookmarkCenterX - CONTROLLER_PROMPT_GLYPH_SIZE / 2, bookmarkPromptY - CONTROLLER_PROMPT_GLYPH_SIZE / 2),
		EShortcut::BATTLE_CONTROLLER_CAST_SPELL, "");

	if(controllerFocusIndex)
	{
		const int actionPromptY = pos.bottom() - 36;
		drawFacePrompt(
			Point(pos.center().x - 128, actionPromptY - CONTROLLER_PROMPT_GLYPH_SIZE / 2),
			EShortcut::GLOBAL_ACCEPT,
			spellAreas[*controllerFocusIndex]->controllerAcceptActionText());
		drawFacePrompt(
			Point(pos.center().x + 34, actionPromptY - CONTROLLER_PROMPT_GLYPH_SIZE / 2),
			EShortcut::GLOBAL_CANCEL,
			LIBRARY->generaltexth->translate("vcmi.battleWindow.controller.holdInspect"));
	}
}

void CSpellWindow::computeSpellsPerArea()
{
	std::vector<const CSpell *> spellsCurSite;
	spellsCurSite.reserve(mySpells.size());
	for(const CSpell * spell : mySpells)
	{
		if(spell->isCombat() ^ !battleSpellsOnly
		   && ((selectedTab == SpellSchool::ANY) || spell->schools.count(selectedTab))
			)
		{
			spellsCurSite.push_back(spell);
		}
	}

	if(selectedTab == SpellSchool::ANY)
	{
		if(spellsCurSite.size() > spellsPerPage)
		{
			spellsCurSite = std::vector<const CSpell *>(spellsCurSite.begin() + currentPage*spellsPerPage, spellsCurSite.end());
			if(spellsCurSite.size() > spellsPerPage)
			{
				spellsCurSite.erase(spellsCurSite.begin()+spellsPerPage, spellsCurSite.end());
			}
		}
	}
	else
	{
		if(spellsCurSite.size() > spellsPerPage - 2)
		{
			if(currentPage == 0)
			{
				spellsCurSite.erase(spellsCurSite.begin()+spellsPerPage-2, spellsCurSite.end());
			}
			else
			{
				spellsCurSite = std::vector<const CSpell *>(spellsCurSite.begin() + (currentPage-1)*spellsPerPage + spellsPerPage-2, spellsCurSite.end());
				if(spellsCurSite.size() > spellsPerPage)
				{
					spellsCurSite.erase(spellsCurSite.begin()+spellsPerPage, spellsCurSite.end());
				}
			}
		}
	}
	//applying
	if(selectedTab == SpellSchool::ANY || currentPage != 0)
	{
		for(size_t c=0; c<spellsPerPage; ++c)
		{
			if(c < spellsCurSite.size())
			{
				spellAreas[c]->setSpell(spellsCurSite[c]);
			}
			else
			{
				spellAreas[c]->setSpell(nullptr);
			}
		}
	}
	else
	{
		spellAreas[0]->setSpell(nullptr);
		spellAreas[1]->setSpell(nullptr);
		for(size_t c=0; c<spellsPerPage-2; ++c)
		{
			if(c < spellsCurSite.size())
				spellAreas[c+2]->setSpell(spellsCurSite[c]);
			else
				spellAreas[c+2]->setSpell(nullptr);
		}
	}
	revalidateControllerFocus();
	redraw();
}

void CSpellWindow::setSchoolImages(SpellSchool school)
{
	OBJECT_CONSTRUCTION;

	schoolTabAnyDisabled.reset();
	if(isLegacySpellSchool(school))
	{
		schoolTab->setFrame(getAnimFrameFromSchool(school), 0);
		schoolTab->visible = true;
	}
	else
	{
		schoolTabAnyDisabled = std::make_shared<CPicture>(ImagePath::builtin("SpelTabNone.png"), 524 + offR, 88);
		schoolTab->visible = false;
	}

	auto it = std::find(customSpellSchools.begin(), customSpellSchools.end(), school);
	int pos = (it == customSpellSchools.end()) ? -1 : std::distance(customSpellSchools.begin(), it);
	for(int i = 0; i < schoolTabCustom.size(); i++)
		schoolTabCustom[i]->setFrame(i == pos ? 0 : 1, 0);
	for(int i = 0; i < schoolTabCustom.size(); i++)
		moveChildForeground(schoolTabCustom[i].get());
	if(pos >= 0)
		moveChildForeground(schoolTabCustom[pos].get());

	schoolPicture->visible = school != SpellSchool::ANY && currentPage == 0 && isLegacySpellSchool(school);
	if(school != SpellSchool::ANY && isLegacySpellSchool(school))
		schoolPicture->setFrame(getAnimFrameFromSchool(school), 0);
	
	schoolPictureCustom.reset();
	if(!isLegacySpellSchool(school) && currentPage == 0) // on later pages the header would cover spells
		schoolPictureCustom = std::make_shared<CPicture>(LIBRARY->spellSchoolHandler->getById(school)->getSchoolHeaderPath(), 117 + offL, 74 + offT);
}

void CSpellWindow::setCurrentPage(int value)
{
	currentPage = value;
	setSchoolImages(selectedTab);

	bool canTurnLeft = currentPage != 0;
	bool canTurnRight = currentPage + 1 < pagesWithinCurrentTab();

	leftCorner->setEnabled(canTurnLeft);
	rightCorner->setEnabled(canTurnRight);
	leftCornerArea->setEnabled(canTurnLeft);
	rightCornerArea->setEnabled(canTurnRight);

	ENGINE->fakeMouseMove(); // refresh hover state so a stale page-turn hint clears when the corner is disabled under the cursor

	mana->setText(std::to_string(myHero->mana));//just in case, it will be possible to cast spell without closing book
}

void CSpellWindow::turnPageLeft()
{
	OBJECT_CONSTRUCTION;
	if(settings["video"]["spellbookAnimation"].Bool() && !isBigSpellbook)
		video = std::make_shared<VideoWidgetOnce>(Point(13, 14), VideoPath::builtin("PGTRNLFT.SMK"), false, this);
}

void CSpellWindow::turnPageRight()
{
	OBJECT_CONSTRUCTION;
	if(settings["video"]["spellbookAnimation"].Bool() && !isBigSpellbook)
		video = std::make_shared<VideoWidgetOnce>(Point(13, 14), VideoPath::builtin("PGTRNRGH.SMK"), false, this);
}

void CSpellWindow::onVideoPlaybackFinished()
{
	video.reset();
	redraw();
}

void CSpellWindow::keyPressed(EShortcut key)
{
	if(usesNativeSpellbookNavigation())
	{
		switch(key)
		{
			case EShortcut::GLOBAL_ACCEPT:
				if(controllerFocusIndex)
					spellAreas[*controllerFocusIndex]->clickPressed(spellAreas[*controllerFocusIndex]->pos.center());
				return;
			case EShortcut::GLOBAL_CANCEL:
				if(controllerFocusIndex)
					spellAreas[*controllerFocusIndex]->showPopupWindow(spellAreas[*controllerFocusIndex]->pos.center());
				return;
			case EShortcut::BATTLE_CONTROLLER_CAST_SPELL:
				fexitb();
				return;
			case EShortcut::BATTLE_DEFEND:
				fLcornerb();
				return;
			case EShortcut::BATTLE_WAIT:
				fRcornerb();
				return;
			case EShortcut::MOVE_LEFT:
				fbattleSpellsb();
				return;
			case EShortcut::MOVE_RIGHT:
				fadvSpellsb();
				return;
			default:
				break;
		}
	}

	switch(key)
	{
		case EShortcut::GLOBAL_RETURN:
			fexitb();
			break;

		case EShortcut::MOVE_LEFT:
			fLcornerb();
			break;
		case EShortcut::MOVE_RIGHT:
			fRcornerb();
			break;
		case EShortcut::MOVE_UP:
		case EShortcut::MOVE_DOWN:
		{
			bool down = key == EShortcut::MOVE_DOWN;
			static const std::array legacyOrder = { SpellSchool::AIR, SpellSchool::EARTH, SpellSchool::FIRE, SpellSchool::WATER, SpellSchool::ANY };

			auto order = customSpellSchools;
			order.insert(order.begin(), legacyOrder.begin(), legacyOrder.end());

			int idx = std::distance(order.begin(), std::find(order.begin(), order.end(), selectedTab));
			idx = (idx + (down ? 1 : -1) + static_cast<int>(order.size())) % static_cast<int>(order.size());
			if(selectedTab != order[idx])
				selectSchool(order[idx]);
			break;
		}
		case EShortcut::SPELLBOOK_TAB_COMBAT:
			fbattleSpellsb();
			break;
		case EShortcut::SPELLBOOK_TAB_ADVENTURE:
			fadvSpellsb();
			break;
		case EShortcut::SPELLBOOK_SEARCH_FOCUS:
			toggleSearchBoxFocus();
			break;
	}
}

bool CSpellWindow::captureThisKey(EShortcut key)
{
	if(!usesNativeSpellbookNavigation())
		return false;

	return key == EShortcut::GLOBAL_ACCEPT || key == EShortcut::GLOBAL_CANCEL
		|| key == EShortcut::BATTLE_CONTROLLER_CAST_SPELL
		|| key == EShortcut::BATTLE_DEFEND || key == EShortcut::BATTLE_WAIT
		|| key == EShortcut::MOVE_LEFT || key == EShortcut::MOVE_RIGHT
		|| key == EShortcut::MOVE_UP || key == EShortcut::MOVE_DOWN;
}

bool CSpellWindow::usesNativeSpellbookNavigation() const
{
	return myInt && myInt->battleInt && myInt->battleInt->fieldController
		&& myInt->battleInt->fieldController->isControllerNativeMode();
}

void CSpellWindow::setControllerFocus(std::optional<size_t> index)
{
	if(index && (*index >= static_cast<size_t>(spellsPerPage) || !spellAreas[*index]->hasSpell()))
		index.reset();
	if(controllerFocusIndex == index)
		return;
	if(controllerFocusIndex)
		spellAreas[*controllerFocusIndex]->setControllerFocused(false);
	controllerFocusIndex = index;
	if(controllerFocusIndex)
		spellAreas[*controllerFocusIndex]->setControllerFocused(true);
}

void CSpellWindow::revalidateControllerFocus()
{
	if(!usesNativeSpellbookNavigation())
	{
		setControllerFocus(std::nullopt);
		return;
	}
	if(controllerFocusIndex && spellAreas[*controllerFocusIndex]->hasSpell())
		return;

	std::optional<size_t> next;
	if(controllerFocusIndex)
	{
		const Point previousCenter = spellAreas[*controllerFocusIndex]->pos.center();
		double bestDistance = std::numeric_limits<double>::max();
		for(size_t index = 0; index < static_cast<size_t>(spellsPerPage); ++index)
		{
			if(!spellAreas[index]->hasSpell())
				continue;
			const Point delta = spellAreas[index]->pos.center() - previousCenter;
			const double distance = static_cast<double>(delta.x) * delta.x + static_cast<double>(delta.y) * delta.y;
			if(distance < bestDistance)
			{
				bestDistance = distance;
				next = index;
			}
		}
	}
	else
	{
		for(size_t index = 0; index < static_cast<size_t>(spellsPerPage); ++index)
			if(spellAreas[index]->hasSpell())
			{
				next = index;
				break;
			}
	}
	setControllerFocus(next);
}

void CSpellWindow::moveControllerFocus(double directionX, double directionY)
{
	revalidateControllerFocus();
	if(!controllerFocusIndex)
		return;

	const double directionLength = std::hypot(directionX, directionY);
	if(vstd::isAlmostZero(directionLength))
		return;
	const Point origin = spellAreas[*controllerFocusIndex]->pos.center();
	std::optional<size_t> best;
	double bestAlignment = CONTROLLER_SPELL_FOCUS_MIN_ALIGNMENT;
	double bestDistance = std::numeric_limits<double>::max();
	for(size_t index = 0; index < static_cast<size_t>(spellsPerPage); ++index)
	{
		if(index == *controllerFocusIndex || !spellAreas[index]->hasSpell())
			continue;
		const Point delta = spellAreas[index]->pos.center() - origin;
		const double distance = std::hypot(static_cast<double>(delta.x), static_cast<double>(delta.y));
		if(vstd::isAlmostZero(distance))
			continue;
		const double alignment = (delta.x * directionX + delta.y * directionY) / (distance * directionLength);
		if(alignment > bestAlignment + 0.0001 || (std::abs(alignment - bestAlignment) <= 0.0001 && distance < bestDistance))
		{
			bestAlignment = alignment;
			bestDistance = distance;
			best = index;
		}
	}
	if(best)
		setControllerFocus(best);
}

void CSpellWindow::resetControllerAxis()
{
	controllerAxisX = controllerAxisY = 0.0;
	controllerAxisSettleElapsed = 0;
	controllerAxisPending = false;
	controllerAxisLatched = false;
}

void CSpellWindow::tick(uint32_t msPassed)
{
	if(!controllerAxisPending)
		return;
	controllerAxisSettleElapsed += msPassed;
	if(controllerAxisSettleElapsed < CONTROLLER_SPELL_FOCUS_SETTLE_MS)
		return;
	controllerAxisPending = false;
	moveControllerFocus(controllerAxisX, controllerAxisY);
}

void CSpellWindow::inputModeChanged(InputMode)
{
	resetControllerAxis();
	if(usesNativeSpellbookNavigation())
		revalidateControllerFocus();
	else
		setControllerFocus(std::nullopt);
}

bool CSpellWindow::usesNativeControllerAxis() const
{
	return usesNativeSpellbookNavigation();
}

bool CSpellWindow::controllerAxisMoved(int, const std::vector<EShortcut> & actions, double value)
{
	bool leftStickChanged = false;
	if(vstd::contains(actions, EShortcut::MOUSE_CURSOR_X))
	{
		controllerAxisX = value;
		leftStickChanged = true;
	}
	if(vstd::contains(actions, EShortcut::MOUSE_CURSOR_Y))
	{
		controllerAxisY = value;
		leftStickChanged = true;
	}

	if(leftStickChanged)
	{
		const bool active = !vstd::isAlmostZero(controllerAxisX) || !vstd::isAlmostZero(controllerAxisY);
		if(!active)
			resetControllerAxis();
		else if(!controllerAxisLatched)
		{
			controllerAxisLatched = true;
			controllerAxisPending = true;
			controllerAxisSettleElapsed = 0;
		}
	}
	return true;
}

void CSpellWindow::controllerInputReset()
{
	resetControllerAxis();
	if(usesNativeSpellbookNavigation())
		revalidateControllerFocus();
}

int CSpellWindow::pagesWithinCurrentTab()
{
	return battleSpellsOnly ? sitesPerTabBattle[selectedTab] : sitesPerTabAdv[selectedTab];
}

CSpellWindow::SpellArea::SpellArea(Rect pos, CSpellWindow * owner)
{
	this->pos = pos;
	this->owner = owner;
	addUsedEvents(LCLICK | SHOW_POPUP | HOVER);

	schoolLevel = -1;
	mySpell = nullptr;

	OBJECT_CONSTRUCTION;

	image = std::make_shared<CAnimImage>(AnimationPath::builtin("Spells"), 0, 0, SPELL_SLOT_CONTENT_OFFSET_X, 0);
	image->visible = false;

	name = std::make_shared<CLabel>(48, 70, FONT_TINY, ETextAlignment::CENTER);
	level = std::make_shared<CLabel>(48, 82, FONT_TINY, ETextAlignment::CENTER);
	cost = std::make_shared<CLabel>(48, 94, FONT_TINY, ETextAlignment::CENTER);

	for(auto l : {name, level, cost})
		l->setAutoRedraw(false);
}

CSpellWindow::SpellArea::~SpellArea() = default;

bool CSpellWindow::SpellArea::hasSpell() const
{
	return mySpell != nullptr;
}

void CSpellWindow::SpellArea::setControllerFocused(bool focused)
{
	if(controllerFocused == focused)
		return;
	controllerFocused = focused;
	setRedrawParent(true);
	redraw();
	updateStatus(focused);
}

std::string CSpellWindow::SpellArea::controllerAcceptActionText() const
{
	if(!mySpell)
		return "";
	if(owner->onSpellSelect)
		return LIBRARY->generaltexth->translate("vcmi.battleWindow.controller.select");
	if(mySpell->isCombat())
		return LIBRARY->generaltexth->translate("vcmi.battleWindow.controller.cast");
	return LIBRARY->generaltexth->translate("vcmi.battleWindow.controller.inspect");
}

void CSpellWindow::SpellArea::showAll(Canvas & to)
{
	const bool focused = controllerFocused || (!owner->usesNativeSpellbookNavigation() && isHovered());
	if(focused && mySpell)
		to.drawColorBlended(pos, SPELL_FOCUS_BACKGROUND_COLOR);
	CIntObject::showAll(to);
	if(!focused || !mySpell)
		return;

	const int left = pos.left();
	const int right = pos.right();
	const int top = pos.top();
	const int bottom = pos.bottom();
	for(int offset = 0; offset < SPELL_FOCUS_CORNER_THICKNESS; ++offset)
	{
		to.drawLine(
			Point(left + offset, top + offset),
			Point(left + SPELL_FOCUS_CORNER_LENGTH, top + offset),
			SPELL_FOCUS_COLOR,
			SPELL_FOCUS_COLOR);
		to.drawLine(
			Point(left + offset, top + offset),
			Point(left + offset, top + SPELL_FOCUS_CORNER_LENGTH),
			SPELL_FOCUS_COLOR,
			SPELL_FOCUS_COLOR);
		to.drawLine(
			Point(right - offset, top + offset),
			Point(right - SPELL_FOCUS_CORNER_LENGTH, top + offset),
			SPELL_FOCUS_COLOR,
			SPELL_FOCUS_COLOR);
		to.drawLine(
			Point(right - offset, top + offset),
			Point(right - offset, top + SPELL_FOCUS_CORNER_LENGTH),
			SPELL_FOCUS_COLOR,
			SPELL_FOCUS_COLOR);
		to.drawLine(
			Point(left + offset, bottom - offset),
			Point(left + SPELL_FOCUS_CORNER_LENGTH, bottom - offset),
			SPELL_FOCUS_COLOR,
			SPELL_FOCUS_COLOR);
		to.drawLine(
			Point(left + offset, bottom - offset),
			Point(left + offset, bottom - SPELL_FOCUS_CORNER_LENGTH),
			SPELL_FOCUS_COLOR,
			SPELL_FOCUS_COLOR);
		to.drawLine(
			Point(right - offset, bottom - offset),
			Point(right - SPELL_FOCUS_CORNER_LENGTH, bottom - offset),
			SPELL_FOCUS_COLOR,
			SPELL_FOCUS_COLOR);
		to.drawLine(
			Point(right - offset, bottom - offset),
			Point(right - offset, bottom - SPELL_FOCUS_CORNER_LENGTH),
			SPELL_FOCUS_COLOR,
			SPELL_FOCUS_COLOR);
	}
}

void CSpellWindow::SpellArea::clickPressed(const Point & cursorPosition)
{
	if(mySpell)
	{
		ENGINE->input().hapticFeedback();

		if(owner->onSpellSelect)
		{
			owner->onSpellSelect(mySpell->id);
			owner->close();
			return;
		}

		if(!mySpell->isCombat())
		{
			const auto spellCost = owner->myInt->cb->getSpellCost(mySpell, owner->myHero);
			if(spellCost > owner->myHero->mana) //insufficient mana
			{
				MetaString message = MetaString::createFromTextID("core.genrltxt.206"); // That spell costs %d spell points. Your hero only has %d spell points...
				message.replaceNumber(spellCost);
				message.replaceNumber(owner->myHero->mana);
				CInfoWindow::showInfoDialog(message.toString(&GAME->translator()), {}, owner->myInt->playerID);
				return;
			}
		}

		//anything that is not combat spell is adventure spell
		//this not an error in general to cast even creature ability with hero
		const bool combatSpell = mySpell->isCombat();
		if(combatSpell == mySpell->isAdventure())
		{
			logGlobal->error("Spell have invalid flags");
			return;
		}

		const bool inCombat = owner->myInt->battleInt != nullptr;
		const bool inCastle = owner->myInt->castleInt != nullptr;

		//battle spell on adv map or adventure map spell during combat => display infowindow, not cast
		if((combatSpell != inCombat) || inCastle || (!combatSpell && !GAME->interface()->makingTurn))
		{
			std::vector<std::shared_ptr<CComponent>> hlp(1, std::make_shared<CComponent>(ComponentType::SPELL, mySpell->id));
			CInfoWindow::showInfoDialog(mySpell->getDescriptionTranslated(schoolLevel), hlp, owner->myInt->playerID);
		}
		else if(combatSpell)
		{
			spells::detail::ProblemImpl problem;
			if(mySpell->canBeCast(problem, owner->myInt->battleInt->getBattle().get(), spells::Mode::HERO, owner->myHero))
			{
				owner->myInt->battleInt->castThisSpell(mySpell->id);
				owner->fexitb();
			}
			else
			{
				std::vector<std::string> texts;
				problem.getAll(texts);
				if(!texts.empty())
					CInfoWindow::showInfoDialog(texts.front(), {}, owner->myInt->playerID);
				else
					CInfoWindow::showInfoDialog(LIBRARY->generaltexth->translate("vcmi.adventureMap.spellUnknownProblem"), {}, owner->myInt->playerID);
			}
		}
		else //adventure spell
		{
			const CGHeroInstance * h = owner->myHero;
			ENGINE->windows().popWindows(1);

			auto guard = vstd::makeScopeGuard([this]()
			{
				auto spellBookState = owner->myInt->localState->getSpellbookSettings();
				spellBookState.spellbookLastTabAdvmap = owner->selectedTab;
				spellBookState.spellbookLastPageAdvmap = owner->currentPage;
				owner->myInt->localState->setSpellbookSettings(spellBookState);
			});

			spells::detail::ProblemImpl problem;
			if (mySpell->getAdventureMechanics().canBeCast(problem, GAME->interface()->cb.get(), owner->myHero))
			{
				const auto * rangeEffect = mySpell->getAdventureMechanics().getEffectAs<AdventureSpellRangedEffect>(owner->myHero);

				if(rangeEffect != nullptr)
					adventureInt->enterCastingMode(mySpell);
				else
					owner->myInt->cb->castSpell(h, mySpell->id);
			}
			else
			{
				std::vector<std::string> texts;
				problem.getAll(texts);
				if(!texts.empty())
					CInfoWindow::showInfoDialog(texts.front(), {}, owner->myInt->playerID);
				else
					CInfoWindow::showInfoDialog(LIBRARY->generaltexth->translate("vcmi.adventureMap.spellUnknownProblem"), {}, owner->myInt->playerID);
			}
		}
	}
}

void CSpellWindow::SpellArea::showPopupWindow(const Point & cursorPosition)
{
	if(mySpell)
	{
		std::string dmgInfo;
		auto causedDmg = owner->myInt->cb->estimateSpellDamage(mySpell, owner->myHero);
		if(causedDmg == 0 || mySpell->id == SpellID::TITANS_LIGHTNING_BOLT) //Titan's Lightning Bolt already has damage info included
			dmgInfo.clear();
		else
		{
			MetaString dmgText = MetaString::createFromTextID("core.genrltxt.343");
			dmgText.replaceNumber(causedDmg);
			dmgInfo = dmgText.toString(&GAME->translator());
		}

		CRClickPopup::createAndPush(
			mySpell->getDescriptionTranslated(schoolLevel) + dmgInfo,
			std::make_shared<CComponent>(ComponentType::SPELL, mySpell->id),
			cursorPosition);
	}
}

void CSpellWindow::SpellArea::hover(bool on)
{
	setRedrawParent(true);
	redraw();
	if(!owner->usesNativeSpellbookNavigation())
		updateStatus(on);
}

void CSpellWindow::SpellArea::updateStatus(bool on)
{
	if(!mySpell)
		return;
	if(on)
	{
		MetaString message = MetaString::createFromRawString("%s (%s)");
		message.replaceTextID(mySpell->getNameTextID());
		message.replaceTextID("core.genrltxt", 171 + mySpell->getLevel());
		owner->statusBar->write(message.toString(&GAME->translator()));
	}
	else
		owner->statusBar->clear();
}

void CSpellWindow::SpellArea::setSpell(const CSpell * spell)
{
	const bool presentedAsFocused = controllerFocused || (!owner->usesNativeSpellbookNavigation() && isHovered());
	if(presentedAsFocused && !spell)
		updateStatus(false);
	schoolBorder.reset();
	image->visible = false;
	name->setText("");
	level->setText("");
	cost->setText("");
	mySpell = spell;
	if(mySpell)
	{
		SpellSchool whichSchool;
		schoolLevel = owner->myHero->getSpellSchoolLevel(mySpell, &whichSchool);
		auto spellCost = owner->myInt->cb->getSpellCost(mySpell, owner->myHero);

		image->setFrame(mySpell->id.getNum());
		image->visible = true;

		{
			OBJECT_CONSTRUCTION;

			schoolBorder.reset();
			if (owner->selectedTab == SpellSchool::ANY)
			{
				if (whichSchool.hasValue())
					schoolBorder = std::make_shared<CAnimImage>(
						LIBRARY->spellSchoolHandler->getById(whichSchool)->getSpellBordersPath(),
						schoolLevel,
						0,
						SPELL_SLOT_CONTENT_OFFSET_X,
						0);
			}
			else
				schoolBorder = std::make_shared<CAnimImage>(
					LIBRARY->spellSchoolHandler->getById(owner->selectedTab)->getSpellBordersPath(),
					schoolLevel,
					0,
					SPELL_SLOT_CONTENT_OFFSET_X,
					0);
		}

		ColorRGBA firstLineColor, secondLineColor;
		if(spellCost > owner->myHero->mana && !owner->onSpellSelect) //hero cannot cast this spell
		{
			firstLineColor = Colors::WHITE;
			secondLineColor = Colors::ORANGE;
		}
		else
		{
			firstLineColor = Colors::YELLOW;
			secondLineColor = Colors::WHITE;
		}

		name->color = firstLineColor;
		name->setText(mySpell->getNameTranslated());

		level->color = secondLineColor;
		std::string levelTextID = mySpell->getLevel() > 0 ? TextIdentifier("core.genrltxt", 171 + mySpell->getLevel()).get()
														  : "vcmi.spellBook.zero_level.hint";

		if(schoolLevel > 0)
		{
			MetaString levelText = MetaString::createFromRawString("%s/%s");
			levelText.replaceTextID(levelTextID);
			levelText.replaceTextID("core.skilllev", 3 + (schoolLevel - 1)); //lines 4-6
			level->setText(levelText.toString(&GAME->translator()));
		}
		else
			level->setText(GAME->translator().translate(levelTextID));

		cost->color = secondLineColor;
		MetaString costText = MetaString::createFromRawString("%s: %d");
		costText.replaceTextID("core.genrltxt.387"); // Spell Points
		costText.replaceNumber(spellCost);
		cost->setText(costText.toString(&GAME->translator()));
	}
	if(presentedAsFocused)
		updateStatus(true);
}
