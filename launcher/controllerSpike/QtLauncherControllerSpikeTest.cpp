/*
 * QtLauncherControllerSpikeTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "LauncherControllerSpike.h"

#include <QApplication>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QPushButton>
#include <QTimer>

#include <iostream>
#include <optional>

namespace
{
struct FixtureVariant
{
	int qtMajor;
	int controllerInstance;
	QString navigationAction;
	QString startAction;
};

bool require(bool condition, const QString & message)
{
	if(condition)
		return true;

	std::cerr << "FAIL: " << message.toStdString() << '\n';
	return false;
}

void drainQtEventLoop()
{
	QEventLoop loop;
	QTimer::singleShot(0, &loop, &QEventLoop::quit);
	loop.exec();
}

std::optional<FixtureVariant> activeFixture()
{
	QFile fixture(QStringLiteral(VCMI_QT_LAUNCHER_SPIKE_FIXTURE_PATH));
	if(!fixture.open(QIODevice::ReadOnly))
		return std::nullopt;

	QJsonParseError parseError;
	const auto document = QJsonDocument::fromJson(fixture.readAll(), &parseError);
	if(parseError.error != QJsonParseError::NoError || !document.isObject())
		return std::nullopt;

	const auto variants = document.object().value(QStringLiteral("qtVariants")).toArray();
	const auto currentQtMajor = QLibraryInfo::version().majorVersion();
	for(const auto & value : variants)
	{
		const auto object = value.toObject();
		if(object.value(QStringLiteral("qtMajor")).toInt() != currentQtMajor)
			continue;

		return FixtureVariant{
			currentQtMajor,
			object.value(QStringLiteral("controllerInstance")).toInt(),
			object.value(QStringLiteral("navigationAction")).toString(),
			object.value(QStringLiteral("startAction")).toString()
		};
	}

	return std::nullopt;
}
}

int main(int argc, char ** argv)
{
	QApplication app(argc, argv);
	const auto fixture = activeFixture();
	if(!require(fixture.has_value(), QStringLiteral("fixture must contain the active Qt major variant")))
		return 1;

	if(!require(fixture->qtMajor == QLibraryInfo::version().majorVersion(), QStringLiteral("fixture Qt major must match the runtime")))
		return 1;

	LauncherControllerSpike spike;
	spike.show();
	spike.navigationButton()->setFocus(Qt::OtherFocusReason);
	drainQtEventLoop();

	if(!require(QApplication::focusWidget() == spike.navigationButton(), QStringLiteral("native Qt focus must begin on the navigation button")))
		return 1;

	spike.postControllerInput({static_cast<std::uint32_t>(fixture->controllerInstance), LauncherControllerAction::NavigateNext});
	drainQtEventLoop();
	if(!require(QApplication::focusWidget() == spike.startButton(), QStringLiteral("queued controller navigation must use native Qt focus traversal")))
		return 1;

	spike.postControllerInput({static_cast<std::uint32_t>(fixture->controllerInstance), LauncherControllerAction::Start});
	drainQtEventLoop();
	if(!require(spike.acceptedLaunches() == 1, QStringLiteral("first queued Start action must create one logical launch")))
		return 1;

	spike.postControllerInput({static_cast<std::uint32_t>(fixture->controllerInstance), LauncherControllerAction::Start});
	drainQtEventLoop();
	if(!require(spike.acceptedLaunches() == 1 && spike.ignoredStarts() == 1, QStringLiteral("duplicate Start action must not create a second logical launch")))
		return 1;

	std::cout << "PASS: Qt" << fixture->qtMajor << " queued controller event, native focus, and Start-once fixture\n";
	return 0;
}
