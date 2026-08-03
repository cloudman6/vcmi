/*
 * VCMIDirs.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "VCMIDirs.h"
#include "json/JsonNode.h"

#ifdef VCMI_MAC
#include "VCMIDirsOSXInternal.h"

#include <cerrno>
#include <iterator>
#include <unistd.h>
#endif

#ifdef VCMI_IOS
#include "iOS_utils.h"
#elif defined(VCMI_ANDROID)
#include "CAndroidVMHelper.h"
#endif

namespace bfs = boost::filesystem;

bfs::path IVCMIDirs::userLogsPath() const { return userCachePath(); }

bfs::path IVCMIDirs::userSavePath() const { return userDataPath() / "Saves"; }

bfs::path IVCMIDirs::userExtractedPath() const { return userCachePath() / "extracted"; }

std::string IVCMIDirs::genHelpString() const
{
	std::vector<std::string> tempVec;
	for (const bfs::path & path : dataPaths())
		tempVec.push_back(path.string());
	const auto gdStringA = boost::algorithm::join(tempVec, ":");

	return
		"  game data:		" + gdStringA + "\n"
		"  server:			" + serverPath().string() + "\n"
		"\n"
		"  user data:		" + userDataPath().string() + "\n"
		"  user cache:		" + userCachePath().string() + "\n"
		"  user config:		" + userConfigPath().string() + "\n"
		"  user logs:		" + userLogsPath().string() + "\n"
		"  user saves:		" + userSavePath().string() + "\n"
		"  user extracted:	" + userExtractedPath().string() + "\n";
}

void IVCMIDirs::init()
{
	// TODO: Log errors
	bfs::create_directories(userDataPath());
	bfs::create_directories(userCachePath());
	bfs::create_directories(userConfigPath());
	bfs::create_directories(userLogsPath());
	bfs::create_directories(userSavePath());
}

#ifdef VCMI_WINDOWS

#ifdef __MINGW32__
	#define _WIN32_IE 0x0500

	#ifndef CSIDL_MYDOCUMENTS
	#define CSIDL_MYDOCUMENTS CSIDL_PERSONAL
	#endif
#endif // __MINGW32__

#include <windows.h>
#include <shlobj.h>

class VCMIDirsWIN32 final : public IVCMIDirs
{
	public:
		VCMIDirsWIN32();
		bfs::path userDataPath() const override;
		bfs::path userCachePath() const override;
		bfs::path userConfigPath() const override;
		bfs::path userLogsPath() const override;
		bfs::path userSavePath() const override;

		std::vector<bfs::path> dataPaths() const override;

		bfs::path clientPath() const override;
		bfs::path mapEditorPath() const override;
		bfs::path serverPath() const override;

		bfs::path binaryPath() const override;

	protected:
		std::unique_ptr<JsonNode> dirsConfig;

		bfs::path getPathFromConfigOrDefault(const std::string& key, const std::function<bfs::path()>& fallbackFunc) const;
		bfs::path getDefaultUserDataPath() const;

		std::wstring utf8ToWstring(const std::string& str) const;
		std::string pathToUtf8(const bfs::path& path) const;
};


VCMIDirsWIN32::VCMIDirsWIN32()
{
	wchar_t currentPath[MAX_PATH];
	GetModuleFileNameW(nullptr, currentPath, MAX_PATH);
	auto configPath = bfs::path(currentPath).parent_path() / "config" / "dirs.json";

	if (!bfs::exists(configPath))
		return;

	std::ifstream in(pathToUtf8(configPath), std::ios::binary);
	if (!in)
		return;

	std::string buffer((std::istreambuf_iterator<char>(in)), {});
	dirsConfig = std::make_unique<JsonNode>(reinterpret_cast<const std::byte*>(buffer.data()), buffer.size(), pathToUtf8(configPath));
}

std::string VCMIDirsWIN32::pathToUtf8(const bfs::path& path) const
{
	std::wstring wstr = path.wstring();
	int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string result(size - 1, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, result.data(), size, nullptr, nullptr);
	return result;
}

std::wstring VCMIDirsWIN32::utf8ToWstring(const std::string& str) const
{
	std::wstring result;
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
	if (size_needed > 0)
	{
		result.resize(size_needed - 1);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), size_needed);
	}
	return result;
}

bfs::path VCMIDirsWIN32::getPathFromConfigOrDefault(const std::string& key, const std::function<bfs::path()>& fallbackFunc) const
{
	if (!dirsConfig || !dirsConfig->isStruct())
		return fallbackFunc();

	const JsonNode& node = (*dirsConfig)[key];
	if (!node.isString())
		return fallbackFunc();

	std::wstring raw = utf8ToWstring(node.String());
	wchar_t expanded[MAX_PATH];
	if (ExpandEnvironmentStringsW(raw.c_str(), expanded, MAX_PATH))
		return bfs::path(expanded);
	else
		return bfs::path(raw);
}

bfs::path VCMIDirsWIN32::getDefaultUserDataPath() const
{
	wchar_t profileDir[MAX_PATH];
	if (SHGetSpecialFolderPathW(nullptr, profileDir, CSIDL_MYDOCUMENTS, FALSE) != FALSE)
		return bfs::path(profileDir) / "My Games" / "vcmi";
	return bfs::path(".");
}

bfs::path VCMIDirsWIN32::userDataPath() const
{
	return getPathFromConfigOrDefault("userDataPath", [this] { return getDefaultUserDataPath(); });
}

bfs::path VCMIDirsWIN32::userCachePath() const
{
	return getPathFromConfigOrDefault("userCachePath", [this] { return userDataPath() / "cache"; });
}

bfs::path VCMIDirsWIN32::userConfigPath() const
{
	return getPathFromConfigOrDefault("userConfigPath", [this] { return userDataPath() / "config"; });
}

bfs::path VCMIDirsWIN32::userLogsPath() const
{
	return getPathFromConfigOrDefault("userLogsPath", [this] { return userDataPath() / "logs"; });
}

bfs::path VCMIDirsWIN32::userSavePath() const
{
	return getPathFromConfigOrDefault("userSavePath", [this] { return userDataPath() / "Saves"; });
}

std::vector<bfs::path> VCMIDirsWIN32::dataPaths() const
{
	return std::vector<bfs::path>(1, bfs::path("."));
}

bfs::path VCMIDirsWIN32::clientPath() const { return binaryPath() / "VCMI_client.exe"; }
bfs::path VCMIDirsWIN32::mapEditorPath() const { return binaryPath() / "VCMI_mapeditor.exe"; }
bfs::path VCMIDirsWIN32::serverPath() const { return binaryPath() / "VCMI_server.exe"; }

bfs::path VCMIDirsWIN32::binaryPath() const { return ".";  }
#elif defined(VCMI_UNIX)
class IVCMIDirsUNIX : public IVCMIDirs
{
	public:
		bfs::path clientPath() const override;
		bfs::path mapEditorPath() const override;
		bfs::path serverPath() const override;

		virtual bool developmentMode() const;
};

bool IVCMIDirsUNIX::developmentMode() const
{
	// We want to be able to run VCMI from single directory. E.g to run from build output directory
	const bool hasConfigs = bfs::exists("config") && bfs::exists("Mods");
	const bool hasBinaries = bfs::exists("vcmiclient")
		|| bfs::exists("vcmiserver")
		|| bfs::exists("vcmilobby")
		|| bfs::exists("vcmieditor")
		|| bfs::exists("vcmitest");
	return hasConfigs && hasBinaries;
}

bfs::path IVCMIDirsUNIX::clientPath() const { return binaryPath() / "vcmiclient"; }
bfs::path IVCMIDirsUNIX::mapEditorPath() const { return binaryPath() / "vcmieditor"; }
bfs::path IVCMIDirsUNIX::serverPath() const { return binaryPath() / "vcmiserver"; }

#ifdef VCMI_APPLE
class VCMIDirsApple : public IVCMIDirsUNIX
{
public:
	bfs::path userConfigPath() const override;
};

bfs::path VCMIDirsApple::userConfigPath() const { return userDataPath() / "config"; }

#ifdef VCMI_IOS
class VCMIDirsIOS final : public VCMIDirsApple
{
public:
	bfs::path userDataPath() const override;
	bfs::path userCachePath() const override;
	bfs::path userLogsPath() const override;

	std::vector<bfs::path> dataPaths() const override;

	bfs::path binaryPath() const override;
};

bfs::path VCMIDirsIOS::userDataPath() const { return {iOS_utils::documentsPath()}; }
bfs::path VCMIDirsIOS::userCachePath() const { return {iOS_utils::cachesPath()}; }
bfs::path VCMIDirsIOS::userLogsPath() const { return {iOS_utils::documentsPath()}; }

std::vector<bfs::path> VCMIDirsIOS::dataPaths() const
{
	std::vector<bfs::path> paths;
	paths.reserve(4);
#ifdef VCMI_IOS_SIM
	paths.emplace_back(iOS_utils::hostApplicationSupportPath());
#endif
	paths.emplace_back(userDataPath());
	paths.emplace_back(iOS_utils::documentsPath());
	paths.emplace_back(binaryPath());
	return paths;
}

bfs::path VCMIDirsIOS::binaryPath() const { return {iOS_utils::bundlePath()}; }
#elif defined(VCMI_MAC)
namespace
{
enum class UserRootRuntimeFailure
{
	InvalidValue,
	StorageUnavailable,
	StorageNotWritable,
};

[[noreturn]] void failUserRoot(const UserRootRuntimeFailure failure)
{
	switch(failure)
	{
	case UserRootRuntimeFailure::InvalidValue:
		throw std::runtime_error("VCMI_USER_ROOT: invalid value");
	case UserRootRuntimeFailure::StorageUnavailable:
		throw std::runtime_error("VCMI_USER_ROOT: storage unavailable");
	case UserRootRuntimeFailure::StorageNotWritable:
		throw std::runtime_error("VCMI_USER_ROOT: storage not writable");
	}

	throw std::runtime_error("VCMI_USER_ROOT: storage unavailable");
}

VCMIDirs::detail::UserRootResolution readUserRootResolution()
{
	const char * rawValue = std::getenv("VCMI_USER_ROOT");
	if(rawValue == nullptr)
		return VCMIDirs::detail::resolveUserRoot(std::nullopt);

	const std::string copiedValue(rawValue);
	return VCMIDirs::detail::resolveUserRoot(std::string_view(copiedValue));
}

UserRootRuntimeFailure classifyFilesystemError(const boost::system::error_code & error)
{
	const boost::system::error_condition condition = error.default_error_condition();
	if(condition == boost::system::errc::make_error_condition(boost::system::errc::permission_denied)
		|| condition == boost::system::errc::make_error_condition(boost::system::errc::operation_not_permitted)
		|| condition == boost::system::errc::make_error_condition(boost::system::errc::read_only_file_system))
		return UserRootRuntimeFailure::StorageNotWritable;
	return UserRootRuntimeFailure::StorageUnavailable;
}

UserRootRuntimeFailure classifyErrno(const int error)
{
	if(error == EACCES || error == EPERM || error == EROFS)
		return UserRootRuntimeFailure::StorageNotWritable;
	return UserRootRuntimeFailure::StorageUnavailable;
}

bfs::file_status inspectPath(const bfs::path & path)
{
	boost::system::error_code error;
	const bfs::file_status status = bfs::symlink_status(path, error);
	if(error.default_error_condition() == boost::system::errc::make_error_condition(boost::system::errc::no_such_file_or_directory))
		return bfs::file_status(bfs::file_not_found);
	if(error)
		failUserRoot(classifyFilesystemError(error));
	return status;
}

bool isMissing(const bfs::file_status & status)
{
	return status.type() == bfs::file_not_found;
}

void requireDirectory(const bfs::path & path)
{
	const bfs::file_status status = inspectPath(path);
	if(isMissing(status) || bfs::is_symlink(status) || !bfs::is_directory(status))
		failUserRoot(UserRootRuntimeFailure::StorageUnavailable);
}

void createDirectoryIfMissing(const bfs::path & path)
{
	if(isMissing(inspectPath(path)))
	{
		boost::system::error_code error;
		bfs::create_directory(path, error);
		if(error)
			failUserRoot(classifyFilesystemError(error));
	}
	requireDirectory(path);
}

bfs::path canonicalPath(const bfs::path & path)
{
	boost::system::error_code error;
	const bfs::path result = bfs::canonical(path, error);
	if(error)
		failUserRoot(classifyFilesystemError(error));
	return result;
}

bool isContainedIn(const bfs::path & child, const bfs::path & parent)
{
	auto childIterator = child.begin();
	for(auto parentIterator = parent.begin(); parentIterator != parent.end(); ++parentIterator)
	{
		if(childIterator == child.end() || *childIterator != *parentIterator)
			return false;
		++childIterator;
	}
	return true;
}

void verifyDirectoryContainment(const bfs::path & path, const bfs::path & canonicalRoot)
{
	if(!isContainedIn(canonicalPath(path), canonicalRoot))
		failUserRoot(UserRootRuntimeFailure::StorageUnavailable);
}

void verifyDirectoryWritable(const bfs::path & directory)
{
	std::string pattern = (directory / ".vcmi-userroot-write.XXXXXX").string();
	std::vector<char> patternBuffer(pattern.begin(), pattern.end());
	patternBuffer.push_back('\0');

	const int descriptor = mkstemp(patternBuffer.data());
	if(descriptor < 0)
		failUserRoot(classifyErrno(errno));

	int writeError = 0;
	const char marker = 'x';
	errno = 0;
	if(write(descriptor, &marker, 1) != 1)
		writeError = errno == 0 ? EIO : errno;
	errno = 0;
	if(close(descriptor) != 0 && writeError == 0)
		writeError = errno == 0 ? EIO : errno;

	boost::system::error_code removeError;
	bfs::remove(bfs::path(patternBuffer.data()), removeError);
	if(removeError)
		failUserRoot(UserRootRuntimeFailure::StorageUnavailable);
	if(writeError != 0)
		failUserRoot(classifyErrno(writeError));
}

void initializeOverrideStorage(const VCMIDirs::detail::UserRootLayout & layout)
{
	const bfs::path root(layout.root);
	const bfs::path parent(root.parent_path());
	requireDirectory(parent);
	const bfs::path canonicalParent = canonicalPath(parent);

	createDirectoryIfMissing(root);
	const bfs::path canonicalRoot = canonicalPath(root);
	if(canonicalRoot == bfs::path("/") || canonicalRoot.parent_path() != canonicalParent)
		failUserRoot(UserRootRuntimeFailure::StorageUnavailable);

	const std::vector<bfs::path> directories = {
		root,
		bfs::path(layout.data),
		bfs::path(layout.config),
		bfs::path(layout.cache),
		bfs::path(layout.logs),
		bfs::path(layout.saves),
		bfs::path(layout.extracted),
	};

	for(const bfs::path & directory : directories)
	{
		const bfs::file_status status = inspectPath(directory);
		if(!isMissing(status) && (bfs::is_symlink(status) || !bfs::is_directory(status)))
			failUserRoot(UserRootRuntimeFailure::StorageUnavailable);
	}

	for(auto iterator = std::next(directories.begin()); iterator != directories.end(); ++iterator)
		createDirectoryIfMissing(*iterator);

	for(const bfs::path & directory : directories)
		verifyDirectoryContainment(directory, canonicalRoot);
	for(const bfs::path & directory : directories)
		verifyDirectoryWritable(directory);
}
}

class VCMIDirsOSX final : public VCMIDirsApple
{
public:
	VCMIDirsOSX();

	bfs::path userDataPath() const override;
	bfs::path userCachePath() const override;
	bfs::path userConfigPath() const override;
	bfs::path userLogsPath() const override;
	bfs::path userSavePath() const override;
	bfs::path userExtractedPath() const override;

	std::vector<bfs::path> dataPaths() const override;

	bfs::path binaryPath() const override;

	void init() override;

private:
	const VCMIDirs::detail::UserRootResolution userRootResolution;

	const VCMIDirs::detail::UserRootLayout * overrideLayout() const;
};

VCMIDirsOSX::VCMIDirsOSX()
	: userRootResolution(readUserRootResolution())
{
}

const VCMIDirs::detail::UserRootLayout * VCMIDirsOSX::overrideLayout() const
{
	if(const auto * overrideResolution = std::get_if<VCMIDirs::detail::UseOverride>(&userRootResolution))
		return &overrideResolution->layout;
	if(std::holds_alternative<VCMIDirs::detail::Invalid>(userRootResolution))
		failUserRoot(UserRootRuntimeFailure::InvalidValue);
	return nullptr;
}

void VCMIDirsOSX::init()
{
	if(std::holds_alternative<VCMIDirs::detail::Invalid>(userRootResolution))
		failUserRoot(UserRootRuntimeFailure::InvalidValue);
	if(const auto * layout = overrideLayout())
	{
		initializeOverrideStorage(*layout);
		return;
	}

	// Call base (init dirs)
	IVCMIDirsUNIX::init();

	auto moveDirIfExists = [](const bfs::path& from, const bfs::path& to)
	{
		if (!bfs::is_directory(from))
			return; // Nothing to do here. Flies away.

		if (bfs::is_empty(from))
		{
			bfs::remove(from);
			return; // Nothing to do here. Flies away.
		}

		if (!bfs::is_directory(to))
		{
			// IVCMIDirs::init() should create all destination directories.
			// TODO: Log fact, that we shouldn't be here.
			bfs::create_directories(to);
		}

		for (bfs::directory_iterator file(from); file != bfs::directory_iterator(); ++file)
		{
			const bfs::path& srcFilePath = file->path();
			const bfs::path  dstFilePath = to / srcFilePath.filename();

			// TODO: Application should ask user what to do when file exists:
			// replace/ignore/stop process/replace all/ignore all
			if (!bfs::exists(dstFilePath))
				bfs::rename(srcFilePath, dstFilePath);
		}

		if (!bfs::is_empty(from)); // TODO: Log warn. Some files not moved. User should try to move files.
		else
			bfs::remove(from);
	};

	moveDirIfExists(userDataPath() / "Games", userSavePath());
}

bfs::path VCMIDirsOSX::userDataPath() const
{
	if(const auto * layout = overrideLayout())
		return layout->data;

	// This is Cocoa code that should be normally used to get path to Application Support folder but can't use it here for now...
	// NSArray* urls = [[NSFileManager defaultManager] URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask];
	// UserPath = path([urls[0] path] + "/vcmi").string();

	// ...so here goes a bit of hardcode instead

	const char* homeDir = getenv("HOME"); // Should be std::getenv?
	if (homeDir == nullptr)
		homeDir = ".";
	return bfs::path(homeDir) / "Library" / "Application Support" / "vcmi";
}
bfs::path VCMIDirsOSX::userCachePath() const
{
	if(const auto * layout = overrideLayout())
		return layout->cache;
	return userDataPath();
}

bfs::path VCMIDirsOSX::userConfigPath() const
{
	if(const auto * layout = overrideLayout())
		return layout->config;
	return VCMIDirsApple::userConfigPath();
}

bfs::path VCMIDirsOSX::userLogsPath() const
{
	if(const auto * layout = overrideLayout())
		return layout->logs;

	// TODO: use proper objc code from Foundation framework
	if(const auto homeDir = std::getenv("HOME"))
		return bfs::path{homeDir} / "Library" / "Logs" / "vcmi";
	return IVCMIDirsUNIX::userLogsPath();
}

bfs::path VCMIDirsOSX::userSavePath() const
{
	if(const auto * layout = overrideLayout())
		return layout->saves;
	return IVCMIDirs::userSavePath();
}

bfs::path VCMIDirsOSX::userExtractedPath() const
{
	if(const auto * layout = overrideLayout())
		return layout->extracted;
	return IVCMIDirs::userExtractedPath();
}

std::vector<bfs::path> VCMIDirsOSX::dataPaths() const
{
	std::vector<bfs::path> ret;
	//FIXME: need some proper codepath for detecting running from build output directory
	if(developmentMode())
	{
		ret.push_back(".");
	}
	else
	{
		ret.push_back("../Resources/Data");
	}
	return ret;
}

bfs::path VCMIDirsOSX::binaryPath() const { return "."; }
#endif // VCMI_IOS, VCMI_MAC

#elif defined(VCMI_ANDROID)
class VCMIDirsAndroid : public IVCMIDirsUNIX
{
	std::string basePath;
	std::string internalPath;
public:
	bfs::path binaryPath() const override;
	bfs::path userDataPath() const override;
	bfs::path userCachePath() const override;
	bfs::path userConfigPath() const override;

	std::vector<bfs::path> dataPaths() const override;

	void init() override;
};

bfs::path VCMIDirsAndroid::binaryPath() const { return "."; }
bfs::path VCMIDirsAndroid::userDataPath() const { return basePath; }
bfs::path VCMIDirsAndroid::userCachePath() const { return userDataPath() / "cache"; }
bfs::path VCMIDirsAndroid::userConfigPath() const { return userDataPath() / "config"; }

std::vector<bfs::path> VCMIDirsAndroid::dataPaths() const
{
	return {
		internalPath,
		userDataPath(),
	};
}

void VCMIDirsAndroid::init()
{
	// asks java code to retrieve needed paths from environment
	CAndroidVMHelper envHelper;
	basePath = envHelper.callStaticStringMethod(CAndroidVMHelper::NATIVE_METHODS_DEFAULT_CLASS, "dataRoot");
	internalPath = envHelper.callStaticStringMethod(CAndroidVMHelper::NATIVE_METHODS_DEFAULT_CLASS, "internalDataRoot");
	IVCMIDirsUNIX::init();
}

#elif defined(VCMI_PORTMASTER)
class VCMIDirsPM : public IVCMIDirsUNIX
{
public:
	bfs::path userDataPath() const override;
	bfs::path userCachePath() const override;
	bfs::path userConfigPath() const override;

	std::vector<bfs::path> dataPaths() const override;

	bfs::path binaryPath() const override;
};

bfs::path VCMIDirsPM::userDataPath() const
{
	const char* homeDir;
	if((homeDir = getenv("PORTMASTER_HOME")))
		return bfs::path(homeDir) / "data";
	else
		return bfs::path(".") / "data";
}
bfs::path VCMIDirsPM::userCachePath() const
{
	// $XDG_CACHE_HOME, default: $HOME/.cache
	const char * tempResult;
	if ((tempResult = getenv("PORTMASTER_HOME")))
		return bfs::path(tempResult) / "cache";
	else
		return bfs::path(".") / "cache";
}
bfs::path VCMIDirsPM::userConfigPath() const
{
	// $XDG_CONFIG_HOME, default: $HOME/.config
	const char * tempResult;
	if ((tempResult = getenv("PORTMASTER_HOME")))
		return bfs::path(tempResult) / "save";
	else
		return bfs::path(".") / "save";
}

std::vector<bfs::path> VCMIDirsPM::dataPaths() const
{
	// $XDG_DATA_DIRS, default: /usr/local/share/:/usr/share/

	// construct list in reverse.
	// in specification first directory has highest priority
	// in vcmi fs last directory has highest priority
	std::vector<bfs::path> ret;
	const char * tempResult;
	if ((tempResult = getenv("PORTMASTER_HOME")))
	{
		ret.push_back(bfs::path(tempResult) / "data");
		ret.push_back(bfs::path(tempResult));
	}

	ret.push_back(bfs::path(".") / "data");
	ret.push_back(bfs::path("."));
	return ret;
}

bfs::path VCMIDirsPM::binaryPath() const
{
	const char * tempResult;
	if ((tempResult = getenv("PORTMASTER_HOME")))
		return bfs::path(tempResult) / "bin";
	else
		return M_BIN_DIR;
}

#elif defined(VCMI_XDG)
class VCMIDirsXDG : public IVCMIDirsUNIX
{
public:
	bfs::path userDataPath() const override;
	bfs::path userCachePath() const override;
	bfs::path userConfigPath() const override;

	std::vector<bfs::path> dataPaths() const override;

	bfs::path binaryPath() const override;
};

bfs::path VCMIDirsXDG::userDataPath() const
{
	// $XDG_DATA_HOME, default: $HOME/.local/share
	const char* homeDir;
	if((homeDir = getenv("XDG_DATA_HOME")))
		return bfs::path(homeDir) / "vcmi";
	else if((homeDir = getenv("HOME")))
		return bfs::path(homeDir) / ".local" / "share" / "vcmi";
	else
		return ".";
}
bfs::path VCMIDirsXDG::userCachePath() const
{
	// $XDG_CACHE_HOME, default: $HOME/.cache
	const char * tempResult;
	if ((tempResult = getenv("XDG_CACHE_HOME")))
		return bfs::path(tempResult) / "vcmi";
	else if ((tempResult = getenv("HOME")))
		return bfs::path(tempResult) / ".cache" / "vcmi";
	else
		return ".";
}
bfs::path VCMIDirsXDG::userConfigPath() const
{
	// $XDG_CONFIG_HOME, default: $HOME/.config
	const char * tempResult = getenv("XDG_CONFIG_HOME");
	if (tempResult)
		return bfs::path(tempResult) / "vcmi";

	tempResult = getenv("HOME");
	if (tempResult)
		return bfs::path(tempResult) / ".config" / "vcmi";

	return ".";
}

std::vector<bfs::path> VCMIDirsXDG::dataPaths() const
{
	// $XDG_DATA_DIRS, default: /usr/local/share/:/usr/share/

	// construct list in reverse.
	// in specification first directory has highest priority
	// in vcmi fs last directory has highest priority
	std::vector<bfs::path> ret;

	if(developmentMode())
	{
		//For now we'll disable usage of system directories when VCMI running from bin directory
		ret.emplace_back(".");
	}
	else
	{
		ret.emplace_back(M_DATA_DIR);
		const char * tempResult;
		if((tempResult = getenv("XDG_DATA_DIRS")) != nullptr)
		{
			std::string dataDirsEnv = tempResult;
			std::vector<std::string> dataDirs;
			boost::split(dataDirs, dataDirsEnv, boost::is_any_of(":"));
			for (auto & entry : std::views::reverse(dataDirs))
				ret.push_back(bfs::path(entry) / "vcmi");
		}
		else
		{
			ret.push_back(bfs::path("/usr/share") / "vcmi");
			ret.push_back(bfs::path("/usr/local/share") / "vcmi");
		}

		// Debian and other distributions might want to use it while it's not part of XDG
		ret.push_back(bfs::path("/usr/share/games") / "vcmi");
	}

	return ret;
}

bfs::path VCMIDirsXDG::binaryPath() const
{
	if(developmentMode())
		return ".";
	else
		return M_BIN_DIR;
}

#endif // VCMI_APPLE, VCMI_ANDROID, VCMI_XDG
#endif // VCMI_WINDOWS, VCMI_UNIX

// Getters for interfaces are separated for clarity.
namespace VCMIDirs
{
	const IVCMIDirs& get()
	{
		#ifdef VCMI_WINDOWS
			static VCMIDirsWIN32 singleton;
		#elif defined(VCMI_ANDROID)
			static VCMIDirsAndroid singleton;
		#elif defined(VCMI_PORTMASTER)
			static VCMIDirsPM singleton;
		#elif defined(VCMI_XDG)
			static VCMIDirsXDG singleton;
		#elif defined(VCMI_MAC)
			static VCMIDirsOSX singleton;
		#elif defined(VCMI_IOS)
			static VCMIDirsIOS singleton;
		#endif

		static std::once_flag flag;
		std::call_once(flag, [] { singleton.init(); });
		return singleton;
	}
}
