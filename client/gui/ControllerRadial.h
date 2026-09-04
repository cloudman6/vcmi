/*
 * ControllerRadial.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "ControllerRadialState.h"

#include "../gui/CIntObject.h"

class IImage;

struct ControllerRadialItem
{
	ControllerRadialItemId id;
	std::string label;
	bool enabled;
	size_t slot;
	size_t page = 0;
	std::function<void()> callback;
	std::string iconAnimation;
	int32_t iconFrame = 0;
	std::string unavailableReason = {};
	std::string detailSummary = {};
	std::string detailDescription = {};
	std::string iconImage = {};
};

struct ControllerRadialPageShortcuts
{
	EShortcut previous;
	EShortcut next;
};

/// Shared controller radial presentation. Its consumer remains the action and
/// availability owner through the live item provider.
class ControllerRadial final : public WindowBase
{
public:
	using ItemProvider = std::function<std::vector<ControllerRadialItem>()>;
	using BoundsProvider = std::function<Rect()>;

private:
	ItemProvider itemProvider;
	BoundsProvider boundsProvider;
	EShortcut openingShortcut;
	size_t slotCount;
	std::optional<ControllerRadialPageShortcuts> pageShortcuts;
	std::vector<EShortcut> capturedShortcuts;
	std::string selectLabel;
	std::string closeLabel;
	ControllerRadialState state;
	std::map<std::string, std::shared_ptr<IImage>> promptSpriteCache;
	std::map<std::string, std::shared_ptr<IImage>> itemImageCache;
	std::map<std::pair<std::string, int32_t>, std::shared_ptr<IImage>> itemSpriteCache;
	double axisX = 0.0;
	double axisY = 0.0;
	bool confirmPressed = false;
	std::optional<size_t> pageTransitionFrom;
	uint32_t pageTransitionElapsed = 0;

	std::vector<ControllerRadialItem> currentItems() const;
	std::vector<ControllerRadialEntry> currentEntries(const std::vector<ControllerRadialItem> & items) const;
	void closeWithoutCommit();
	void releaseConfirm();
	const std::shared_ptr<IImage> & promptSprite(const std::string & path);
	const std::shared_ptr<IImage> & itemImage(const std::string & path);
	const std::shared_ptr<IImage> & itemSprite(const std::string & animation, int32_t frame);
	void drawKeyPrompt(Canvas & to, Point position, EShortcut shortcut, const std::string & actionText, bool pressed, bool disabled = false);
	void changePage(int offset);
	void drawWheel(Canvas & to, const std::vector<ControllerRadialItem> & items, size_t page, Point center, double scale, bool active);
	void drawPageSwitchPrompt(Canvas & to, Point promptCenter, EShortcut shortcut, const std::string & arrow);

public:
	ControllerRadial(
		ItemProvider itemProvider,
		BoundsProvider boundsProvider,
		EShortcut openingShortcut,
		size_t slotCount,
		std::optional<ControllerRadialPageShortcuts> pageShortcuts,
		std::vector<EShortcut> capturedShortcuts,
		std::string selectLabel,
		std::string closeLabel);

	bool captureThisKey(EShortcut key) override;
	void keyPressed(EShortcut key) override;
	void keyReleased(EShortcut key) override;
	void inputModeChanged(InputMode mode) override;
	void tick(uint32_t msPassed) override;
	void showAll(Canvas & to) override;
	void onScreenResize() override;

	bool usesNativeControllerAxis() const override;
	bool controllerAxisMoved(int instanceId, const std::vector<EShortcut> & actions, double value) override;
	void controllerInputReset() override;
};
