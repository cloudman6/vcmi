/*
 * UserRootRuntimeTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdlib>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <optional>
#include <spawn.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char ** environ;

namespace
{
std::string runtimeProbePath;

std::string joinPath(const std::string & parent, const std::string_view child)
{
	return parent + "/" + std::string(child);
}

class ScopedTemporaryDirectory
{
public:
	ScopedTemporaryDirectory()
	{
		const char * const tempDirectory = std::getenv("TMPDIR");
		if(tempDirectory == nullptr)
			throw std::runtime_error("TMPDIR is unavailable");

		std::string pattern = joinPath(tempDirectory, "codex-userroot-runtime.XXXXXX");
		std::vector<char> patternBuffer(pattern.begin(), pattern.end());
		patternBuffer.push_back('\0');
		char * const result = mkdtemp(patternBuffer.data());
		if(result == nullptr)
			throw std::runtime_error("unable to create test fixture");
		root = result;
	}

	std::string path(const std::string_view child) const
	{
		return joinPath(root, child);
	}

private:
	std::string root;
};

void createDirectory(const std::string & path, const mode_t mode = 0700)
{
	ASSERT_EQ(mkdir(path.c_str(), mode), 0);
}

void createFile(const std::string & path)
{
	const int descriptor = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
	ASSERT_NE(descriptor, -1);
	const char marker = 'x';
	ASSERT_EQ(write(descriptor, &marker, 1), 1);
	ASSERT_EQ(close(descriptor), 0);
}

bool pathExists(const std::string & path)
{
	struct stat info;
	return lstat(path.c_str(), &info) == 0;
}

bool isDirectory(const std::string & path)
{
	struct stat info;
	return lstat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode) && !S_ISLNK(info.st_mode);
}

bool isDirectoryEmpty(const std::string & path)
{
	DIR * directory = opendir(path.c_str());
	if(directory == nullptr)
		return false;
	bool empty = true;
	while(dirent * entry = readdir(directory))
	{
		if(std::string_view(entry->d_name) != "." && std::string_view(entry->d_name) != "..")
		{
			empty = false;
			break;
		}
	}
	closedir(directory);
	return empty;
}

bool hasWriteProbeResidue(const std::string & path)
{
	DIR * directory = opendir(path.c_str());
	if(directory == nullptr)
		return true;
	bool hasResidue = false;
	while(dirent * entry = readdir(directory))
	{
		if(std::string_view(entry->d_name).starts_with(".vcmi-userroot-write."))
		{
			hasResidue = true;
			break;
		}
	}
	closedir(directory);
	return hasResidue;
}

std::vector<std::string> expectedDirectories(const std::string & root)
{
	const std::string data = joinPath(root, "data");
	const std::string cache = joinPath(root, "cache");
	return {
		root,
		data,
		joinPath(root, "config"),
		cache,
		joinPath(root, "logs"),
		joinPath(data, "Saves"),
		joinPath(cache, "extracted"),
	};
}

struct RunResult
{
	int exitCode;
	std::string output;
};

RunResult runProbe(const std::string & root, const std::optional<std::string> changedRoot = std::nullopt)
{
	std::vector<std::string> environment;
	for(char ** current = environ; *current != nullptr; ++current)
	{
		if(!std::string_view(*current).starts_with("VCMI_USER_ROOT="))
			environment.emplace_back(*current);
	}
	environment.emplace_back("VCMI_USER_ROOT=" + root);

	std::vector<char *> environmentPointers;
	for(std::string & entry : environment)
		environmentPointers.push_back(entry.data());
	environmentPointers.push_back(nullptr);

	std::vector<std::string> arguments;
	arguments.push_back(runtimeProbePath);
	if(changedRoot)
	{
		arguments.emplace_back("--change-root");
		arguments.push_back(*changedRoot);
	}
	std::vector<char *> argumentPointers;
	for(std::string & argument : arguments)
		argumentPointers.push_back(argument.data());
	argumentPointers.push_back(nullptr);

	int outputPipe[2];
	if(pipe(outputPipe) != 0)
		throw std::runtime_error("unable to create child output pipe");

	posix_spawn_file_actions_t actions;
	if(posix_spawn_file_actions_init(&actions) != 0)
	{
		close(outputPipe[0]);
		close(outputPipe[1]);
		throw std::runtime_error("unable to initialize child process actions");
	}
	if(posix_spawn_file_actions_adddup2(&actions, outputPipe[1], STDOUT_FILENO) != 0
		|| posix_spawn_file_actions_adddup2(&actions, outputPipe[1], STDERR_FILENO) != 0
		|| posix_spawn_file_actions_addclose(&actions, outputPipe[0]) != 0
		|| posix_spawn_file_actions_addclose(&actions, outputPipe[1]) != 0)
	{
		posix_spawn_file_actions_destroy(&actions);
		close(outputPipe[0]);
		close(outputPipe[1]);
		throw std::runtime_error("unable to configure child process actions");
	}

	pid_t child = 0;
	const int spawnError = posix_spawn(&child, runtimeProbePath.c_str(), &actions, nullptr, argumentPointers.data(), environmentPointers.data());
	posix_spawn_file_actions_destroy(&actions);
	close(outputPipe[1]);
	if(spawnError != 0)
	{
		close(outputPipe[0]);
		throw std::runtime_error("unable to start child probe");
	}

	std::string output;
	char buffer[256];
	while(true)
	{
		const ssize_t readCount = read(outputPipe[0], buffer, sizeof(buffer));
		if(readCount > 0)
		{
			output.append(buffer, static_cast<size_t>(readCount));
			continue;
		}
		if(readCount == 0)
			break;
		if(errno == EINTR)
			continue;
		close(outputPipe[0]);
		int ignoredStatus = 0;
		pid_t waited = 0;
		do
		{
		}
		while((waited = waitpid(child, &ignoredStatus, 0)) == -1 && errno == EINTR);
		if(waited != child)
			throw std::runtime_error("unable to wait for child probe");
		throw std::runtime_error("unable to read child output");
	}
	close(outputPipe[0]);

	int childStatus = 0;
	pid_t waited = 0;
	do
	{
		waited = waitpid(child, &childStatus, 0);
	}
	while(waited == -1 && errno == EINTR);
	if(waited != child || !WIFEXITED(childStatus))
		return {-1, output};
	return {WEXITSTATUS(childStatus), output};
}

void expectRuntimeFailure(const RunResult & result, const std::string_view category)
{
	EXPECT_EQ(result.exitCode, 2);
	EXPECT_NE(result.output.find(category), std::string::npos);
}

void expectRuntimeSuccess(const RunResult & result)
{
	ASSERT_EQ(result.exitCode, 0) << result.output;
}

void expectExactDirectoryLayout(const std::string & root)
{
	for(const std::string & directory : expectedDirectories(root))
		EXPECT_TRUE(isDirectory(directory));
}

void expectNoWriteProbeResidue(const std::string & root)
{
	for(const std::string & directory : expectedDirectories(root))
		EXPECT_FALSE(hasWriteProbeResidue(directory));
}

TEST(UserRootRuntimeTest, validRootCreatesAllDirectoriesAndKeepsFirstValue)
{
	ScopedTemporaryDirectory fixture;
	const std::string root = fixture.path("valid-root");
	const std::string replacement = fixture.path("replacement-root");

	const RunResult result = runProbe(root, replacement);
	expectRuntimeSuccess(result);
	EXPECT_NE(result.output.find("layout=1"), std::string::npos);
	EXPECT_NE(result.output.find("directories=1"), std::string::npos);
	EXPECT_NE(result.output.find("read-once=1"), std::string::npos);
	expectExactDirectoryLayout(root);
	expectNoWriteProbeResidue(root);
	EXPECT_FALSE(pathExists(replacement));
}

TEST(UserRootRuntimeTest, overrideSkipsLegacyGamesMigration)
{
	ScopedTemporaryDirectory fixture;
	const std::string root = fixture.path("migration-root");
	const std::string data = joinPath(root, "data");
	const std::string games = joinPath(data, "Games");
	const std::string marker = joinPath(games, "marker");
	const std::string migratedMarker = joinPath(joinPath(data, "Saves"), "marker");
	createDirectory(root);
	createDirectory(data);
	createDirectory(games);
	createFile(marker);

	const RunResult result = runProbe(root);
	expectRuntimeSuccess(result);
	EXPECT_TRUE(pathExists(marker));
	EXPECT_FALSE(pathExists(migratedMarker));
}

TEST(UserRootRuntimeTest, invalidValuesFailBeforeStorageInitialization)
{
	for(const auto & root : {"", "relative", "/", "///", "/./", "/state/..", "/../state", "/../../state", "/state/../../other"})
		expectRuntimeFailure(runProbe(root), "VCMI_USER_ROOT: invalid value");
}

TEST(UserRootRuntimeTest, missingParentDoesNotCreateAncestors)
{
	ScopedTemporaryDirectory fixture;
	const std::string missingParent = fixture.path("missing-parent");

	expectRuntimeFailure(runProbe(joinPath(missingParent, "root")), "VCMI_USER_ROOT: storage unavailable");
	EXPECT_FALSE(pathExists(missingParent));
}

TEST(UserRootRuntimeTest, regularFileRootFailsClosed)
{
	ScopedTemporaryDirectory fixture;
	const std::string root = fixture.path("file-root");
	createFile(root);

	expectRuntimeFailure(runProbe(root), "VCMI_USER_ROOT: storage unavailable");
}

TEST(UserRootRuntimeTest, rootSymlinkFailsClosed)
{
	ScopedTemporaryDirectory fixture;
	const std::string target = fixture.path("symlink-target");
	const std::string root = fixture.path("symlink-root");
	createDirectory(target);
	ASSERT_EQ(symlink(target.c_str(), root.c_str()), 0);

	expectRuntimeFailure(runProbe(root), "VCMI_USER_ROOT: storage unavailable");
}

TEST(UserRootRuntimeTest, childSymlinkCannotEscapeOverrideRoot)
{
	ScopedTemporaryDirectory fixture;
	const std::string root = fixture.path("root");
	const std::string sibling = fixture.path("sibling");
	createDirectory(root);
	createDirectory(sibling);
	ASSERT_EQ(symlink(sibling.c_str(), joinPath(root, "data").c_str()), 0);

	expectRuntimeFailure(runProbe(root), "VCMI_USER_ROOT: storage unavailable");
	EXPECT_TRUE(isDirectoryEmpty(sibling));
}

TEST(UserRootRuntimeTest, readOnlyRootFailsAsNotWritable)
{
	ScopedTemporaryDirectory fixture;
	const std::string root = fixture.path("readonly-root");
	createDirectory(root);
	ASSERT_EQ(chmod(root.c_str(), 0500), 0);

	const RunResult result = runProbe(root);
	ASSERT_EQ(chmod(root.c_str(), 0700), 0);
	expectRuntimeFailure(result, "VCMI_USER_ROOT: storage not writable");
}

TEST(UserRootRuntimeTest, separateProcessesKeepStateRootsIndependent)
{
	ScopedTemporaryDirectory fixture;
	const std::string firstRoot = fixture.path("state-a");
	const std::string secondRoot = fixture.path("state-b");
	const RunResult firstResult = runProbe(firstRoot);
	expectRuntimeSuccess(firstResult);
	createFile(joinPath(joinPath(firstRoot, "data"), "state-a-marker"));
	const RunResult secondResult = runProbe(secondRoot);
	expectRuntimeSuccess(secondResult);

	EXPECT_TRUE(pathExists(joinPath(joinPath(firstRoot, "data"), "state-a-marker")));
	EXPECT_FALSE(pathExists(joinPath(joinPath(secondRoot, "data"), "state-a-marker")));
}
}

int main(int argc, char * argv[])
{
	if(argc != 2)
		return 64;
	runtimeProbePath = argv[1];
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
