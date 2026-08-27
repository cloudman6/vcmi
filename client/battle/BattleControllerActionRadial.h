/*
 * BattleControllerActionRadial.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "BattleControllerActionRadialState.h"

#include "../gui/CIntObject.h"
#include "../gui/ControllerAxisReceiver.h"

class IImage;

struct BattleControllerActionRadialItem
{
	BattleControllerActionRadialAction action;
	std::string label;
	bool enabled;
	bool active;
	std::function<void()> callback;
};

/// Battle-local fixed action radial. BattleWindow remains the action and availability owner.
class BattleControllerActionRadial final : public WindowBase, public IControllerAxisReceiver
{
public:
	using ItemProvider = std::function<std::vector<BattleControllerActionRadialItem>()>;

private:
	ItemProvider itemProvider;
	BattleControllerActionRadialState state;
	std::map<std::string, std::shared_ptr<IImage>> promptSpriteCache;
	double axisX = 0.0;
	double axisY = 0.0;
	bool confirmPressed = false;

	std::vector<BattleControllerActionRadialItem> currentItems() const;
	std::vector<BattleControllerActionRadialEntry> currentEntries(
		const std::vector<BattleControllerActionRadialItem> & items) const;
	void closeWithoutCommit();
	void releaseConfirm();
	const std::shared_ptr<IImage> & promptSprite(const std::string & path);
	void drawKeyPrompt(Canvas & to, Point position, EShortcut shortcut,
		const std::string & actionText, bool pressed);

public:
	explicit BattleControllerActionRadial(ItemProvider itemProvider);

#ifdef VCMI_CONTROLLER_E2E
	JsonNode controllerE2ESnapshot() const;
#endif

	bool captureThisKey(EShortcut key) override;
	void keyPressed(EShortcut key) override;
	void keyReleased(EShortcut key) override;
	void keyCancelled(EShortcut key) override;
	void inputModeChanged(InputMode mode) override;
	void showAll(Canvas & to) override;
	void onScreenResize() override;

	ControllerAxisRoute controllerAxisMoved(const ControllerAxisEvent & event) override;
	void controllerAxisUpdate(uint32_t msPassed) override;
	void controllerAxisReset() override;
	bool controllerCursorAllowed() const override;
};
