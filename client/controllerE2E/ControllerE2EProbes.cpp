/*
 * ControllerE2EProbes.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "ControllerE2EProbes.h"

#include "../../lib/json/JsonNode.h"

namespace ControllerE2E
{

ProbeRegistry & ProbeRegistry::instance()
{
	static ProbeRegistry registry;
	return registry;
}

void ProbeRegistry::registerProbe(const std::string & name, Provider provider)
{
	providers[name] = std::move(provider);
}

void ProbeRegistry::unregisterProbe(const std::string & name)
{
	providers.erase(name);
}

bool ProbeRegistry::has(const std::string & name) const
{
	return providers.count(name) > 0;
}

std::vector<std::string> ProbeRegistry::names() const
{
	std::vector<std::string> result;
	result.reserve(providers.size());
	for(const auto & [name, provider] : providers)
		result.push_back(name);
	return result;
}

JsonNode ProbeRegistry::read(const std::string & name) const
{
	const auto found = providers.find(name);
	if(found == providers.end())
	{
		JsonNode error;
		error["error"].String() = "unknown probe '" + name + "'";
		return error;
	}
	try
	{
		return found->second();
	}
	catch(const std::exception & e)
	{
		JsonNode error;
		error["error"].String() = "probe '" + name + "' failed: " + e.what();
		return error;
	}
}

const JsonNode * findProbeField(const JsonNode & snapshot, const std::string & dottedPath)
{
	const JsonNode * current = &snapshot;
	size_t begin = 0;
	while(begin <= dottedPath.size())
	{
		const size_t end = dottedPath.find('.', begin);
		const std::string segment = dottedPath.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
		if(segment.empty() || !current->isStruct())
			return nullptr;
		const auto & fields = current->Struct();
		const auto found = fields.find(segment);
		if(found == fields.end())
			return nullptr;
		current = &found->second;
		if(end == std::string::npos)
			return current;
		begin = end + 1;
	}
	return nullptr;
}

}
