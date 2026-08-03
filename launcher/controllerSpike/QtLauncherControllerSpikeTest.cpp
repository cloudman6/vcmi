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
#include <QSet>
#include <QTimer>
#include <QVector>

#include <cmath>
#include <iostream>
#include <limits>
#include <optional>

namespace
{
struct FixtureVariant
{
	int qtMajor;
	std::uint32_t controllerInstance;
	LauncherControllerAction navigationAction;
	LauncherControllerAction startAction;
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

std::optional<LauncherControllerAction> decodeAction(const QJsonObject & object, const QString & field, QString & failure)
{
	const auto value = object.value(field);
	if(!value.isString())
	{
		failure = QStringLiteral("fixture %1 is missing or not a string").arg(field);
		return std::nullopt;
	}

	const auto action = value.toString();
	if(action == QStringLiteral("navigate-next"))
		return LauncherControllerAction::NavigateNext;
	if(action == QStringLiteral("start"))
		return LauncherControllerAction::Start;

	failure = QStringLiteral("fixture %1 has unsupported action '%2'").arg(field, action);
	return std::nullopt;
}

std::optional<QVector<FixtureVariant>> decodeFixture(const QByteArray & fixtureBytes, QString & failure)
{
	QJsonParseError parseError;
	const auto document = QJsonDocument::fromJson(fixtureBytes, &parseError);
	if(parseError.error != QJsonParseError::NoError || !document.isObject())
	{
		failure = QStringLiteral("fixture must be a JSON object");
		return std::nullopt;
	}

	const auto root = document.object();
	const auto fixtureVersion = root.value(QStringLiteral("fixtureVersion"));
	if(!fixtureVersion.isDouble() || fixtureVersion.toDouble() != 1.0)
	{
		failure = QStringLiteral("fixtureVersion must equal 1");
		return std::nullopt;
	}

	const auto variantsValue = root.value(QStringLiteral("qtVariants"));
	if(!variantsValue.isArray())
	{
		failure = QStringLiteral("fixture qtVariants must be an array");
		return std::nullopt;
	}

	const auto variants = variantsValue.toArray();
	if(variants.size() != 2)
	{
		failure = QStringLiteral("fixture must contain exactly two Qt variants");
		return std::nullopt;
	}

	QSet<int> majors;
	QVector<FixtureVariant> decodedVariants;
	decodedVariants.reserve(variants.size());
	for(int index = 0; index < variants.size(); ++index)
	{
		const auto & value = variants.at(index);
		if(!value.isObject())
		{
			failure = QStringLiteral("fixture variant %1 must be an object").arg(index);
			return std::nullopt;
		}

		const auto object = value.toObject();
		const auto qtMajor = object.value(QStringLiteral("qtMajor"));
		if(!qtMajor.isDouble() || qtMajor.toDouble() != std::floor(qtMajor.toDouble()))
		{
			failure = QStringLiteral("fixture variant %1 qtMajor must be an integer").arg(index);
			return std::nullopt;
		}

		const auto qtMajorValue = qtMajor.toInt();
		if(majors.contains(qtMajorValue))
		{
			failure = QStringLiteral("fixture contains duplicate qtMajor %1").arg(qtMajorValue);
			return std::nullopt;
		}
		majors.insert(qtMajorValue);

		const auto controllerInstance = object.value(QStringLiteral("controllerInstance"));
		if(!controllerInstance.isDouble() || controllerInstance.toDouble() <= 0.0 ||
			controllerInstance.toDouble() != std::floor(controllerInstance.toDouble()) ||
			controllerInstance.toDouble() > std::numeric_limits<std::uint32_t>::max())
		{
			failure = QStringLiteral("fixture variant %1 controllerInstance must be a positive integer").arg(index);
			return std::nullopt;
		}

		const auto navigationAction = decodeAction(object, QStringLiteral("navigationAction"), failure);
		if(!navigationAction)
			return std::nullopt;
		const auto startAction = decodeAction(object, QStringLiteral("startAction"), failure);
		if(!startAction)
			return std::nullopt;

		decodedVariants.append(FixtureVariant{
			qtMajorValue,
			static_cast<std::uint32_t>(controllerInstance.toDouble()),
			*navigationAction,
			*startAction});
	}

	const QSet<int> expectedMajors{5, 6};
	if(majors != expectedMajors)
	{
		failure = QStringLiteral("fixture variants must be exactly Qt5 and Qt6");
		return std::nullopt;
	}

	return decodedVariants;
}

std::optional<QVector<FixtureVariant>> loadFixture(QString & failure)
{
	QFile fixture(QStringLiteral(VCMI_QT_LAUNCHER_SPIKE_FIXTURE_PATH));
	if(!fixture.open(QIODevice::ReadOnly))
	{
		failure = QStringLiteral("fixture file could not be opened");
		return std::nullopt;
	}

	return decodeFixture(fixture.readAll(), failure);
}

const FixtureVariant * activeFixture(const QVector<FixtureVariant> & variants)
{
	const auto currentQtMajor = QLibraryInfo::version().majorVersion();
	for(const auto & variant : variants)
	{
		if(variant.qtMajor == currentQtMajor)
			return &variant;
	}

	return nullptr;
}

QByteArray fixtureWithQt5Variant(const QString & qt5Fields)
{
	return QStringLiteral(R"({"fixtureVersion":1,"qtVariants":[{%1},{"qtMajor":6,"controllerInstance":1,"navigationAction":"navigate-next","startAction":"start"}]})")
		.arg(qt5Fields)
		.toUtf8();
}

bool requireFixtureRejection(const QByteArray & fixtureBytes, const QString & expectedFailure)
{
	QString failure;
	const auto decoded = decodeFixture(fixtureBytes, failure);
	return require(!decoded.has_value() && failure.contains(expectedFailure),
		QStringLiteral("invalid fixture must fail explicitly for %1 (got: %2)").arg(expectedFailure).arg(failure));
}
}

int main(int argc, char ** argv)
{
	QApplication app(argc, argv);
	QString fixtureFailure;
	const auto variants = loadFixture(fixtureFailure);
	if(!require(variants.has_value(), QStringLiteral("fixture must load and validate: %1").arg(fixtureFailure)))
		return 1;

	if(!requireFixtureRejection(QByteArrayLiteral(R"({"fixtureVersion":2,"qtVariants":[{"qtMajor":5,"controllerInstance":1,"navigationAction":"navigate-next","startAction":"start"},{"qtMajor":6,"controllerInstance":1,"navigationAction":"navigate-next","startAction":"start"}]})"), QStringLiteral("fixtureVersion")))
		return 1;
	if(!requireFixtureRejection(QByteArrayLiteral(R"({"fixtureVersion":1,"qtVariants":[{"qtMajor":5,"controllerInstance":1,"navigationAction":"navigate-next","startAction":"start"},{"qtMajor":5,"controllerInstance":1,"navigationAction":"navigate-next","startAction":"start"}]})"), QStringLiteral("duplicate qtMajor")))
		return 1;
	if(!requireFixtureRejection(fixtureWithQt5Variant(QStringLiteral("\"qtMajor\":5,\"controllerInstance\":1,\"navigationAction\":\"unknown\",\"startAction\":\"start\"")), QStringLiteral("navigationAction")))
		return 1;
	if(!requireFixtureRejection(fixtureWithQt5Variant(QStringLiteral("\"qtMajor\":5,\"controllerInstance\":1,\"navigationAction\":\"navigate-next\"")), QStringLiteral("startAction")))
		return 1;
	if(!requireFixtureRejection(fixtureWithQt5Variant(QStringLiteral("\"qtMajor\":5,\"navigationAction\":\"navigate-next\",\"startAction\":\"start\"")), QStringLiteral("controllerInstance")))
		return 1;
	if(!requireFixtureRejection(fixtureWithQt5Variant(QStringLiteral("\"qtMajor\":5,\"controllerInstance\":0,\"navigationAction\":\"navigate-next\",\"startAction\":\"start\"")), QStringLiteral("controllerInstance")))
		return 1;

	const auto fixture = activeFixture(*variants);
	if(!require(fixture != nullptr, QStringLiteral("fixture must contain the active Qt major variant")))
		return 1;

	if(!require(fixture->qtMajor == QLibraryInfo::version().majorVersion(), QStringLiteral("fixture Qt major must match the runtime")))
		return 1;

	LauncherControllerSpike spike;
	spike.show();
	spike.navigationButton()->setFocus(Qt::OtherFocusReason);
	drainQtEventLoop();

	if(!require(QApplication::focusWidget() == spike.navigationButton(), QStringLiteral("native Qt focus must begin on the navigation button")))
		return 1;
	if(!require(!spike.lastControllerInstance().has_value(), QStringLiteral("controller instance must not be observed before a Qt controller event")))
		return 1;

	spike.postControllerInput({fixture->controllerInstance, fixture->navigationAction});
	drainQtEventLoop();
	if(!require(QApplication::focusWidget() == spike.startButton(), QStringLiteral("queued fixture navigation must use native Qt focus traversal")))
		return 1;
	if(!require(spike.lastControllerInstance() == fixture->controllerInstance, QStringLiteral("navigation event must expose its controller instance")))
		return 1;

	spike.postControllerInput({fixture->controllerInstance, fixture->startAction});
	drainQtEventLoop();
	if(!require(spike.acceptedLaunches() == 1, QStringLiteral("first fixture Start action must create one logical launch")))
		return 1;
	if(!require(spike.lastControllerInstance() == fixture->controllerInstance, QStringLiteral("Start event must expose its controller instance")))
		return 1;

	spike.postControllerInput({fixture->controllerInstance, fixture->startAction});
	drainQtEventLoop();
	if(!require(spike.acceptedLaunches() == 1 && spike.ignoredStarts() == 1, QStringLiteral("duplicate fixture Start action must not create a second logical launch")))
		return 1;

	std::cout << "PASS: Qt" << fixture->qtMajor << " validated fixture controller event, native focus, and Start-once fixture\n";
	return 0;
}
