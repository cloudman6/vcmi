/*
 * ControllerTriggerThresholdContractTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include <gtest/gtest.h>

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <fcntl.h>
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
std::string probePath;

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

		std::string pattern = joinPath(tempDirectory, "codex-trigger-threshold.XXXXXX");
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

struct RunResult
{
	int exitCode;
	std::string output;
};

RunResult runProbe(const std::string & root, const std::string_view scenario)
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

	std::vector<std::string> arguments = {probePath, std::string(scenario)};
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
	const int spawnError = posix_spawn(&child, probePath.c_str(), &actions, nullptr, argumentPointers.data(), environmentPointers.data());
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
			waited = waitpid(child, &ignoredStatus, 0);
		}
		while(waited == -1 && errno == EINTR);
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

void expectSuccess(const RunResult & result)
{
	ASSERT_EQ(result.exitCode, 0) << result.output;
	EXPECT_NE(result.output.find("layout=1"), std::string::npos);
	EXPECT_NE(result.output.find("schema=1"), std::string::npos);
	EXPECT_NE(result.output.find("loader=1"), std::string::npos);
}

void expectToken(const RunResult & result, const std::string_view token)
{
	EXPECT_NE(result.output.find(token), std::string::npos) << result.output;
}

bool pathExists(const std::string & path)
{
	struct stat information;
	return lstat(path.c_str(), &information) == 0;
}

void createMarker(const std::string & path)
{
	const int descriptor = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
	ASSERT_NE(descriptor, -1);
	const char marker = 'x';
	ASSERT_EQ(write(descriptor, &marker, 1), 1);
	ASSERT_EQ(close(descriptor), 0);
}

TEST(ControllerTriggerThresholdContractTest, legacyOnlyInputMigratesBeforeSchemaValidation)
{
	ScopedTemporaryDirectory fixture;
	const RunResult result = runProbe(fixture.path("legacy-only"), "legacy-only");
	expectSuccess(result);
	expectToken(result, "value=1");
	expectToken(result, "legacy=0");
	expectToken(result, "canonical=1");
	expectToken(result, "validation-warning=0");
}

TEST(ControllerTriggerThresholdContractTest, canonicalInputAndConflictPreferCanonicalValue)
{
	ScopedTemporaryDirectory fixture;
	const RunResult canonical = runProbe(fixture.path("canonical"), "canonical");
	expectSuccess(canonical);
	expectToken(canonical, "value=1");
	expectToken(canonical, "legacy=0");
	expectToken(canonical, "validation-warning=0");

	const RunResult conflict = runProbe(fixture.path("conflict"), "conflict");
	expectSuccess(conflict);
	expectToken(conflict, "value=1");
	expectToken(conflict, "conflict=1");
	expectToken(conflict, "legacy=0");
	expectToken(conflict, "validation-warning=0");
}

TEST(ControllerTriggerThresholdContractTest, invalidCanonicalNeverFallsBackToLegacyValue)
{
	ScopedTemporaryDirectory fixture;
	const RunResult result = runProbe(fixture.path("invalid-canonical"), "invalid-canonical");
	expectSuccess(result);
	expectToken(result, "value=1");
	expectToken(result, "legacy=0");
	expectToken(result, "invalid-canonical-diagnostic=1");
	expectToken(result, "validation-warning=0");
}

TEST(ControllerTriggerThresholdContractTest, missingKeyUsesCanonicalDefaultInFreshState)
{
	ScopedTemporaryDirectory fixture;
	const RunResult result = runProbe(fixture.path("missing"), "missing");
	expectSuccess(result);
	expectToken(result, "value=1");
	expectToken(result, "canonical=1");
	expectToken(result, "legacy=0");
	expectToken(result, "validation-warning=0");
}

TEST(ControllerTriggerThresholdContractTest, writebackRemovesLegacyKeyAndReloadsCanonicalValue)
{
	ScopedTemporaryDirectory fixture;
	const std::string root = fixture.path("writeback");
	const RunResult writeback = runProbe(root, "writeback");
	expectSuccess(writeback);
	expectToken(writeback, "value=1");
	expectToken(writeback, "writeback=1");
	expectToken(writeback, "legacy=0");

	const RunResult reload = runProbe(root, "reload");
	expectSuccess(reload);
	expectToken(reload, "value=1");
	expectToken(reload, "reload=1");
	expectToken(reload, "legacy=0");
}

TEST(ControllerTriggerThresholdContractTest, separateProcessesUseSeparateStateRoots)
{
	ScopedTemporaryDirectory fixture;
	const std::string firstRoot = fixture.path("state-a");
	const std::string secondRoot = fixture.path("state-b");

	const RunResult first = runProbe(firstRoot, "missing");
	expectSuccess(first);
	const std::string marker = joinPath(joinPath(firstRoot, "data"), "state-a-marker");
	createMarker(marker);

	const RunResult second = runProbe(secondRoot, "missing");
	expectSuccess(second);
	EXPECT_TRUE(pathExists(marker));
	EXPECT_FALSE(pathExists(joinPath(joinPath(secondRoot, "data"), "state-a-marker")));
}
}

int main(int argc, char * argv[])
{
	if(argc != 2)
		return 64;
	probePath = argv[1];
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
