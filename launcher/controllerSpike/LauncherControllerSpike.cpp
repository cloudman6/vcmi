/*
 * LauncherControllerSpike.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "LauncherControllerSpike.h"

#include <QCoreApplication>
#include <QEvent>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
const auto controllerEventType = static_cast<QEvent::Type>(QEvent::registerEventType());

class LauncherControllerQtEvent final : public QEvent
{
public:
	explicit LauncherControllerQtEvent(const LauncherControllerInput & input)
		: QEvent(controllerEventType)
		, input(input)
	{
	}

	LauncherControllerInput input;
};
}

LauncherControllerSpike::LauncherControllerSpike()
	: navigation(new QPushButton(QStringLiteral("Navigate"), this))
	, start(new QPushButton(QStringLiteral("Start"), this))
{
	auto * layout = new QVBoxLayout(this);
	layout->addWidget(navigation);
	layout->addWidget(start);

	navigation->setFocusPolicy(Qt::StrongFocus);
	start->setFocusPolicy(Qt::StrongFocus);
	QWidget::setTabOrder(navigation, start);

	connect(start, &QPushButton::clicked, this, [this]
	{
		++acceptedLaunchCount;
	});
}

QPushButton * LauncherControllerSpike::navigationButton() const
{
	return navigation;
}

QPushButton * LauncherControllerSpike::startButton() const
{
	return start;
}

void LauncherControllerSpike::postControllerInput(const LauncherControllerInput & input)
{
	QCoreApplication::postEvent(this, new LauncherControllerQtEvent(input));
}

int LauncherControllerSpike::acceptedLaunches() const
{
	return acceptedLaunchCount;
}

int LauncherControllerSpike::ignoredStarts() const
{
	return ignoredStartCount;
}

bool LauncherControllerSpike::event(QEvent * event)
{
	if(event->type() != controllerEventType)
		return QWidget::event(event);

	const auto & controllerEvent = static_cast<const LauncherControllerQtEvent &>(*event);
	switch(controllerEvent.input.action)
	{
	case LauncherControllerAction::NavigateNext:
		focusNextChild();
		return true;
	case LauncherControllerAction::Start:
		if(acceptedLaunchCount == 0)
			start->click();
		else
			++ignoredStartCount;
		return true;
	}

	return false;
}
