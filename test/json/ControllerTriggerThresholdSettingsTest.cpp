/*
 * ControllerTriggerThresholdSettingsTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "../../lib/json/JsonUtils.h"

namespace
{
// InputSourceGameController reads settings["input"]["controllerTriggerThreshold"].
// The settings schema must declare exactly that key, otherwise the schema default
// lands under a different name, the input source reads a missing node and every
// trigger axis value engages the bound shortcuts immediately.
constexpr char SETTINGS_SCHEMA[] = "vcmi:settings";
constexpr char KEY_CANONICAL[] = "controllerTriggerThreshold";
constexpr double SCHEMA_TRIGGER_THRESHOLD_DEFAULT = 0.3;

JsonNode maximizedSettings()
{
	JsonNode config;
	JsonUtils::maximize(config, SETTINGS_SCHEMA);
	return config;
}
}

TEST(SettingsSchemaTest, triggerThresholdDefaultUsesKeyReadByInputSource)
{
	JsonNode config = maximizedSettings();

	EXPECT_TRUE(JsonUtils::validate(config, SETTINGS_SCHEMA, "settings"));
	EXPECT_NEAR(config["input"][KEY_CANONICAL].Float(), SCHEMA_TRIGGER_THRESHOLD_DEFAULT, 1e-9);
}

TEST(SettingsSchemaTest, userProvidedTriggerThresholdPassesValidation)
{
	JsonNode config;
	config["input"][KEY_CANONICAL].Float() = 0.5;

	JsonUtils::maximize(config, SETTINGS_SCHEMA);

	EXPECT_TRUE(JsonUtils::validate(config, SETTINGS_SCHEMA, "settings"));
	EXPECT_NEAR(config["input"][KEY_CANONICAL].Float(), 0.5, 1e-9);
}
