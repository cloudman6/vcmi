/*
 * UserRootRuntimeProbe.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "../Global.h"

#include "../lib/VCMIDirs.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace
{
bool isExpectedLayout(const IVCMIDirs & directories, const std::string & root)
{
	const boost::filesystem::path expectedRoot(root);
	return directories.userDataPath() == expectedRoot / "data"
		&& directories.userConfigPath() == expectedRoot / "config"
		&& directories.userCachePath() == expectedRoot / "cache"
		&& directories.userLogsPath() == expectedRoot / "logs"
		&& directories.userSavePath() == expectedRoot / "data" / "Saves"
		&& directories.userExtractedPath() == expectedRoot / "cache" / "extracted";
}

bool hasAllDirectories(const IVCMIDirs & directories, const std::string & root)
{
	const boost::filesystem::path expectedRoot(root);
	return boost::filesystem::is_directory(expectedRoot)
		&& boost::filesystem::is_directory(directories.userDataPath())
		&& boost::filesystem::is_directory(directories.userConfigPath())
		&& boost::filesystem::is_directory(directories.userCachePath())
		&& boost::filesystem::is_directory(directories.userLogsPath())
		&& boost::filesystem::is_directory(directories.userSavePath())
		&& boost::filesystem::is_directory(directories.userExtractedPath());
}
}

int main(int argc, char * argv[])
{
	const char * const rawUserRoot = std::getenv("VCMI_USER_ROOT");
	if(rawUserRoot == nullptr)
	{
		std::cerr << "userroot-runtime: configuration error" << std::endl;
		return 64;
	}
	const std::string initialRoot(rawUserRoot);
	if(argc != 1 && argc != 3)
	{
		std::cerr << "userroot-runtime: configuration error" << std::endl;
		return 64;
	}
	if(argc == 3 && std::string_view(argv[1]) != "--change-root")
	{
		std::cerr << "userroot-runtime: configuration error" << std::endl;
		return 64;
	}

	try
	{
		const IVCMIDirs & directories = VCMIDirs::get();
		const boost::filesystem::path firstDataPath = directories.userDataPath();
		bool readOnce = true;
		if(argc == 3)
		{
			if(setenv("VCMI_USER_ROOT", argv[2], 1) != 0)
			{
				std::cerr << "userroot-runtime: configuration error" << std::endl;
				return 64;
			}
			readOnce = VCMIDirs::get().userDataPath() == firstDataPath;
		}

		const bool layoutMatches = isExpectedLayout(directories, initialRoot);
		const bool directoriesExist = hasAllDirectories(directories, initialRoot);
		std::cout << "userroot-runtime: ok"
			<< " layout=" << layoutMatches
			<< " directories=" << directoriesExist
			<< " read-once=" << readOnce
			<< std::endl;
		return layoutMatches && directoriesExist && readOnce ? 0 : 1;
	}
	catch(const std::exception & error)
	{
		std::cerr << "userroot-runtime: " << error.what() << std::endl;
		return 2;
	}
}
