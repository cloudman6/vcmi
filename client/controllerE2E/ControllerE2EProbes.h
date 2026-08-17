/*
 * ControllerE2EProbes.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

class JsonNode;

namespace ControllerE2E
{

/// Read-only semantic probe registry. Probes expose stable product
/// observations (focus, input mode, presentation, consumer-owned business
/// state). They never expose object pointers and never mutate state; scenarios
/// cannot invoke actions through probes.
class ProbeRegistry
{
public:
	using Provider = std::function<JsonNode()>;

	static ProbeRegistry & instance();

	/// Registers or replaces a probe. Domain probes must be registered by the
	/// owning consumer, not by a central window-type dispatcher.
	void registerProbe(const std::string & name, Provider provider);
	void unregisterProbe(const std::string & name);
	bool has(const std::string & name) const;
	std::vector<std::string> names() const;

	/// Returns the probe snapshot, or {"error": "..."} when unknown/failing
	JsonNode read(const std::string & name) const;

private:
	ProbeRegistry() = default;
	mutable std::map<std::string, Provider> providers;
};

/// Navigates a dotted field path inside a snapshot, e.g. "focus.index".
/// Returns nullptr when any segment is missing.
const JsonNode * findProbeField(const JsonNode & snapshot, const std::string & dottedPath);

}
