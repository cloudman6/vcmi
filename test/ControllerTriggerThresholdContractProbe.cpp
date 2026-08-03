/*
 * ControllerTriggerThresholdContractProbe.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "../Global.h"

#include "../lib/CConfigHandler.h"
#include "../lib/VCMIDirs.h"
#include "../lib/filesystem/Filesystem.h"
#include "../lib/json/JsonUtils.h"
#include "../lib/logging/CLogger.h"

#include <cmath>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>

namespace
{
constexpr std::string_view canonicalKey = "controllerTriggerThreshold";
constexpr std::string_view legacyKey = "controllerTriggerTreshold";
constexpr std::string_view invalidCanonicalDiagnostic = "controller-trigger-threshold.invalid-canonical";
constexpr std::string_view invalidLegacyDiagnostic = "controller-trigger-threshold.invalid-legacy-defaulted";

class RecordingLogTarget final : public ILogTarget
{
public:
	void write(const LogRecord & record) override
	{
		if(record.level == ELogLevel::WARN && record.message == "Data in settings is invalid!")
			settingsValidationWarning = true;
		if(record.message == invalidCanonicalDiagnostic)
			invalidCanonicalWarning = true;
		if(record.message == invalidLegacyDiagnostic)
			invalidLegacyWarning = true;
	}

	bool settingsValidationWarning = false;
	bool invalidCanonicalWarning = false;
	bool invalidLegacyWarning = false;
};

bool isKnownScenario(const std::string_view scenario)
{
	return scenario == "legacy-only"
		|| scenario == "canonical"
		|| scenario == "conflict"
		|| scenario == "invalid-canonical"
		|| scenario == "invalid-legacy-type"
		|| scenario == "invalid-legacy-range"
		|| scenario == "missing"
		|| scenario == "writeback"
		|| scenario == "reload";
}

std::string fixtureFor(const std::string_view scenario)
{
	if(scenario == "legacy-only")
		return R"({"input":{"controllerTriggerTreshold":0.6}})";
	if(scenario == "canonical")
		return R"({"input":{"controllerTriggerThreshold":0.7}})";
	if(scenario == "conflict")
		return R"({"input":{"controllerTriggerThreshold":0.8,"controllerTriggerTreshold":0.2}})";
	if(scenario == "invalid-canonical")
		return R"({"input":{"controllerTriggerThreshold":"invalid","controllerTriggerTreshold":0.9}})";
	if(scenario == "invalid-legacy-type")
		return R"({"input":{"controllerTriggerTreshold":"invalid"}})";
	if(scenario == "invalid-legacy-range")
		return R"({"input":{"controllerTriggerTreshold":1.1}})";
	if(scenario == "missing")
		return R"({"input":{}})";
	if(scenario == "writeback")
		return R"({"input":{"controllerTriggerTreshold":0.4}})";
	return {};
}

double expectedThreshold(const std::string_view scenario)
{
	if(scenario == "legacy-only")
		return 0.6;
	if(scenario == "canonical")
		return 0.7;
	if(scenario == "conflict")
		return 0.8;
	if(scenario == "writeback" || scenario == "reload")
		return 0.73;
	return 0.3;
}

bool containsRequiredKey(const JsonNode & schema, const std::string_view key)
{
	for(const JsonNode & entry : schema["required"].Vector())
	{
		if(entry.String() == key)
			return true;
	}
	return false;
}

bool hasCanonicalInputSchema()
{
	const JsonNode & inputSchema = JsonUtils::getSchema("vcmi:settings")["properties"]["input"];
	const JsonNode & properties = inputSchema["properties"];
	const JsonNode & canonicalSchema = properties[std::string(canonicalKey)];
	return !canonicalSchema.isNull()
		&& canonicalSchema["default"].Float() == 0.3
		&& containsRequiredKey(inputSchema, canonicalKey)
		&& !containsRequiredKey(inputSchema, legacyKey)
		&& properties[std::string(legacyKey)].isNull();
}

bool hasExpectedLayout(const IVCMIDirs & directories, const std::string & root)
{
	const boost::filesystem::path expectedRoot(root);
	return directories.userDataPath() == expectedRoot / "data"
		&& directories.userConfigPath() == expectedRoot / "config"
		&& directories.userCachePath() == expectedRoot / "cache"
		&& directories.userLogsPath() == expectedRoot / "logs"
		&& directories.userSavePath() == expectedRoot / "data" / "Saves"
		&& directories.userExtractedPath() == expectedRoot / "cache" / "extracted";
}

bool writeFixture(const IVCMIDirs & directories, const std::string_view scenario)
{
	std::ofstream file((directories.userConfigPath() / "settings.json").string(), std::ofstream::out | std::ofstream::trunc);
	if(!file)
		return false;
	file << fixtureFor(scenario);
	return static_cast<bool>(file);
}

bool hasCanonicalPersistedSettings(const IVCMIDirs & directories)
{
	std::ifstream file((directories.userConfigPath() / "settings.json").string());
	if(!file)
		return false;
	const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	if(content.empty())
		return false;
	const JsonNode persisted(content.data(), content.size(), "settings.json");
	return !persisted["input"][std::string(canonicalKey)].isNull()
		&& persisted["input"][std::string(legacyKey)].isNull();
}

void printResult(
	const bool layout,
	const bool schema,
	const bool loader,
	const bool value,
	const bool canonical,
	const bool legacy,
	const bool conflict,
	const bool invalidDiagnostic,
	const bool invalidLegacyDefault,
	const bool invalidLegacyDiagnostic,
	const bool validationWarning,
	const bool writeback,
	const bool reload)
{
	std::cout
		<< "threshold-contract:"
		<< " layout=" << layout
		<< " schema=" << schema
		<< " loader=" << loader
		<< " value=" << value
		<< " canonical=" << canonical
		<< " legacy=" << legacy
		<< " conflict=" << conflict
		<< " invalid-canonical-diagnostic=" << invalidDiagnostic
		<< " invalid-legacy-default=" << invalidLegacyDefault
		<< " invalid-legacy-diagnostic=" << invalidLegacyDiagnostic
		<< " validation-warning=" << validationWarning
		<< " writeback=" << writeback
		<< " reload=" << reload
		<< std::endl;
}
}

int main(int argc, char * argv[])
{
	if(argc != 2)
		return 64;
	const std::string_view scenario(argv[1]);
	if(!isKnownScenario(scenario))
		return 64;

	const char * const rawUserRoot = std::getenv("VCMI_USER_ROOT");
	if(rawUserRoot == nullptr || *rawUserRoot == '\0')
		return 64;
	const std::string root(rawUserRoot);

	try
	{
		const IVCMIDirs & directories = VCMIDirs::get();
		directories.init();
		if(scenario != "reload" && !writeFixture(directories, scenario))
			return 2;

		auto recordingTarget = std::make_unique<RecordingLogTarget>();
		RecordingLogTarget * const recording = recordingTarget.get();
		CLogger::getGlobalLogger()->addTarget(std::move(recordingTarget));

		CResourceHandler::initialize();
		settings.init("config/settings.json", "vcmi:settings");

		bool writeback = false;
		if(scenario == "writeback")
		{
			{
				Settings threshold = settings.write["input"][std::string(canonicalKey)];
				threshold->Float() = 0.73;
			}
			writeback = hasCanonicalPersistedSettings(directories);
		}

		const JsonNode & input = settings["input"];
		const JsonNode & canonicalValue = input[std::string(canonicalKey)];
		const bool canonical = !canonicalValue.isNull();
		const bool legacy = !input[std::string(legacyKey)].isNull();
		const bool value = canonicalValue.isNumber() && std::abs(canonicalValue.Float() - expectedThreshold(scenario)) < 0.000001;
		const bool conflict = scenario != "conflict" || value;
		const bool invalidDiagnostic = scenario != "invalid-canonical" || recording->invalidCanonicalWarning;
		const bool invalidLegacyScenario = scenario == "invalid-legacy-type" || scenario == "invalid-legacy-range";
		const bool invalidLegacyDefault = !invalidLegacyScenario || (canonicalValue.isNumber() && std::abs(canonicalValue.Float() - 0.3) < 0.000001);
		const bool invalidLegacyDiagnostic = !invalidLegacyScenario || recording->invalidLegacyWarning;
		const bool reload = scenario != "reload" || value;
		const bool schema = hasCanonicalInputSchema();
		const bool layout = hasExpectedLayout(directories, root);
		const bool loader = true;
		const bool validationWarning = recording->settingsValidationWarning;

		printResult(
			layout,
			schema,
			loader,
			value,
			canonical,
			legacy,
			conflict,
			invalidDiagnostic,
			invalidLegacyDefault,
			invalidLegacyDiagnostic,
			validationWarning,
			writeback,
			reload);

		return layout && schema && loader && value && canonical && !legacy && conflict && invalidDiagnostic && invalidLegacyDefault && invalidLegacyDiagnostic && !validationWarning && writeback == (scenario == "writeback") && reload == (scenario == "reload") ? 0 : 1;
	}
	catch(const std::exception &)
	{
		std::cerr << "threshold-contract: loader-error" << std::endl;
		return 2;
	}
}
