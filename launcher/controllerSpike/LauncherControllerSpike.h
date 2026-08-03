/*
 * LauncherControllerSpike.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include <QWidget>

#include <cstdint>

class QPushButton;
class QEvent;

// This data is intentionally Qt-free. It is spike-local evidence for a
// possible pure-data boundary; it does not define a shared controller API.
enum class LauncherControllerAction
{
	NavigateNext,
	Start,
};

struct LauncherControllerInput
{
	std::uint32_t controllerInstance;
	LauncherControllerAction action;
};

class LauncherControllerSpike final : public QWidget
{
public:
	LauncherControllerSpike();

	QPushButton * navigationButton() const;
	QPushButton * startButton() const;

	void postControllerInput(const LauncherControllerInput & input);

	int acceptedLaunches() const;
	int ignoredStarts() const;

protected:
	bool event(QEvent * event) override;

private:
	QPushButton * navigation;
	QPushButton * start;
	int acceptedLaunchCount = 0;
	int ignoredStartCount = 0;
};
