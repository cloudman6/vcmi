/*
 * ControllerE2EArtifacts.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include <boost/filesystem/path.hpp>

#include <string>
#include <vector>

class JsonNode;
struct SDL_Surface;

namespace ControllerE2E
{

/// Deterministic evidence writer for one scenario run. The output directory is
/// exclusive to the run; artifact write failures must never be reported as PASS.
class ArtifactWriter
{
	boost::filesystem::path outputDir;
	bool directoryReady = false;
	int screenshotCounter = 0;
	std::vector<std::string> writeErrors;

public:
	explicit ArtifactWriter(boost::filesystem::path outputDir);

	bool prepareDirectory();
	bool isReady() const { return directoryReady; }
	const boost::filesystem::path & getOutputDir() const { return outputDir; }
	const std::vector<std::string> & getWriteErrors() const { return writeErrors; }
	bool hasWriteErrors() const { return !writeErrors.empty(); }

	/// Writes a pretty-printed JSON document
	bool writeJson(const std::string & fileName, const JsonNode & document);
	/// Appends one JSON object as a JSONL line
	bool appendJsonLine(const std::string & fileName, const JsonNode & document);
	/// Saves the surface (or a region of it) as a deterministic screenshot name
	bool savePng(
		SDL_Surface * source,
		const std::string & label,
		bool hasRegion,
		int x,
		int y,
		int w,
		int h,
		std::string & savedName
	);
	/// Copies a plain-text file into the output directory
	bool copyText(const std::string & fileName, const std::string & content);

	std::string nextScreenshotName(const std::string & label);
};

/// SHA-256 of the input bytes as lowercase hex. Used for scenario/profile
/// identity digests in the run manifest; covered by known-answer unit tests.
std::string sha256Hex(const std::string & data);

}
