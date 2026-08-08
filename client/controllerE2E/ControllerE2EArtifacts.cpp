/*
 * ControllerE2EArtifacts.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "ControllerE2EArtifacts.h"

#include "../../lib/json/JsonNode.h"

#include "../renderSDL/SDLImage.h"

#include <SDL_surface.h>

#include <boost/filesystem.hpp>
#include <fstream>

namespace ControllerE2E
{

namespace
{

std::string sanitizeLabel(const std::string & label)
{
	std::string result;
	for(const char character : label)
	{
		if(std::isalnum(static_cast<unsigned char>(character)) || character == '-' || character == '_')
			result += character;
		else
			result += '_';
	}
	return result.empty() ? "capture" : result;
}

}

ArtifactWriter::ArtifactWriter(boost::filesystem::path outputDir)
	: outputDir(std::move(outputDir))
{
}

bool ArtifactWriter::prepareDirectory()
{
	try
	{
		boost::filesystem::create_directories(outputDir);
		boost::filesystem::create_directories(outputDir / "screenshots");
		directoryReady = true;
	}
	catch(const std::exception & e)
	{
		writeErrors.push_back(std::string("failed to prepare output directory: ") + e.what());
		directoryReady = false;
	}
	return directoryReady;
}

bool ArtifactWriter::writeJson(const std::string & fileName, const JsonNode & document)
{
	if(!directoryReady)
	{
		writeErrors.push_back("output directory not ready for " + fileName);
		return false;
	}
	try
	{
		std::ofstream stream((outputDir / fileName).string(), std::ios::trunc);
		if(!stream.is_open())
			throw std::runtime_error("cannot open file for writing");
		stream << document.toString() << "\n";
		stream.flush();
		if(!stream.good())
			throw std::runtime_error("write failed");
		return true;
	}
	catch(const std::exception & e)
	{
		writeErrors.push_back("failed to write " + fileName + ": " + e.what());
		return false;
	}
}

bool ArtifactWriter::appendJsonLine(const std::string & fileName, const JsonNode & document)
{
	if(!directoryReady)
	{
		writeErrors.push_back("output directory not ready for " + fileName);
		return false;
	}
	try
	{
		std::ofstream stream((outputDir / fileName).string(), std::ios::app);
		if(!stream.is_open())
			throw std::runtime_error("cannot open file for append");
		stream << document.toString() << "\n";
		if(!stream.good())
			throw std::runtime_error("append failed");
		return true;
	}
	catch(const std::exception & e)
	{
		writeErrors.push_back("failed to append " + fileName + ": " + e.what());
		return false;
	}
}

std::string ArtifactWriter::nextScreenshotName(const std::string & label)
{
	char index[8] = {};
	std::snprintf(index, sizeof(index), "%04d", screenshotCounter++);
	return std::string(index) + "-" + sanitizeLabel(label) + ".png";
}

bool ArtifactWriter::savePng(SDL_Surface * source, const std::string & label, bool hasRegion, int x, int y, int w, int h, std::string & savedName)
{
	if(!directoryReady)
	{
		writeErrors.push_back("output directory not ready for screenshot " + label);
		return false;
	}
	if(!source)
	{
		writeErrors.push_back("screenshot " + label + ": no screen surface available");
		return false;
	}

	SDL_Surface * target = source;
	SDL_Surface * clipped = nullptr;
	if(hasRegion)
	{
		const SDL_Rect area{x, y, w, h};
		clipped = SDL_CreateRGBSurfaceWithFormat(0, w, h, source->format->BitsPerPixel, source->format->format);
		if(!clipped)
		{
			writeErrors.push_back("screenshot " + label + ": failed to allocate region surface: " + SDL_GetError());
			return false;
		}
		if(SDL_BlitSurface(source, &area, clipped, nullptr) != 0)
		{
			writeErrors.push_back("screenshot " + label + ": region blit failed: " + SDL_GetError());
			SDL_FreeSurface(clipped);
			return false;
		}
		target = clipped;
	}

	savedName = nextScreenshotName(label);
	bool success = true;
	try
	{
		auto image = std::make_shared<SDLImageShared>(target);
		image->exportBitmap(outputDir / "screenshots" / savedName, nullptr);
	}
	catch(const std::exception & e)
	{
		writeErrors.push_back("screenshot " + label + ": export failed: " + e.what());
		success = false;
	}

	if(clipped)
		SDL_FreeSurface(clipped);
	return success;
}

bool ArtifactWriter::copyText(const std::string & fileName, const std::string & content)
{
	if(!directoryReady)
	{
		writeErrors.push_back("output directory not ready for " + fileName);
		return false;
	}
	try
	{
		std::ofstream stream((outputDir / fileName).string(), std::ios::trunc);
		if(!stream.is_open())
			throw std::runtime_error("cannot open file for writing");
		stream << content;
		stream.flush();
		if(!stream.good())
			throw std::runtime_error("write failed");
		return true;
	}
	catch(const std::exception & e)
	{
		writeErrors.push_back("failed to write " + fileName + ": " + e.what());
		return false;
	}
}

}
