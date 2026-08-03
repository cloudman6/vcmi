/*
 * VCMIDirsOSXInternal.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 * Private macOS-only user-root parsing detail. This header is intentionally
 * not part of VCMIDirs' public ABI and has no side effects.
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace VCMIDirs::detail
{
enum class UserRootInvalidReason
{
	Empty,
	NotAbsolute,
	PathSyntax,
};

struct UserRootLayout
{
	std::string root;
	std::string data;
	std::string config;
	std::string cache;
	std::string logs;
	std::string saves;
	std::string extracted;
};

struct UseDefault
{
};

struct UseOverride
{
	UserRootLayout layout;
};

struct Invalid
{
	UserRootInvalidReason reason;
};

using UserRootResolution = std::variant<UseDefault, UseOverride, Invalid>;

inline std::string joinUserRootPath(const std::string & root, const std::string_view component)
{
	if(root == "/")
		return "/" + std::string(component);
	return root + "/" + std::string(component);
}

inline std::string lexicallyNormalizeAbsoluteUserRoot(const std::string_view rawValue)
{
	std::vector<std::string_view> components;
	for(size_t begin = 1; begin <= rawValue.size();)
	{
		const size_t end = rawValue.find('/', begin);
		const auto component = rawValue.substr(begin, end == std::string_view::npos ? rawValue.size() - begin : end - begin);
		if(!component.empty() && component != ".")
		{
			if(component == "..")
			{
				if(!components.empty())
					components.pop_back();
			}
			else
			{
				components.push_back(component);
			}
		}
		if(end == std::string_view::npos)
			break;
		begin = end + 1;
	}

	std::string result = "/";
	for(const auto component : components)
	{
		if(result.size() > 1)
			result += '/';
		result.append(component);
	}
	return result;
}

inline UserRootResolution resolveUserRoot(const std::optional<std::string_view> & rawValue)
{
	if(!rawValue)
		return UseDefault{};
	if(rawValue->empty())
		return Invalid{UserRootInvalidReason::Empty};
	if(rawValue->find('\0') != std::string_view::npos)
		return Invalid{UserRootInvalidReason::PathSyntax};
	if(rawValue->front() != '/')
		return Invalid{UserRootInvalidReason::NotAbsolute};

	UserRootLayout layout;
	layout.root = lexicallyNormalizeAbsoluteUserRoot(*rawValue);
	layout.data = joinUserRootPath(layout.root, "data");
	layout.config = joinUserRootPath(layout.root, "config");
	layout.cache = joinUserRootPath(layout.root, "cache");
	layout.logs = joinUserRootPath(layout.root, "logs");
	layout.saves = joinUserRootPath(layout.data, "Saves");
	layout.extracted = joinUserRootPath(layout.cache, "extracted");
	return UseOverride{std::move(layout)};
}
}
