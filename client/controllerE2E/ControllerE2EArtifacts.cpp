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
#include <sstream>

#include <array>
#include <cmath>

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

/// JSONL requires exactly one JSON document per line. JsonWriter is not
/// exported from the shared library, so evidence serialization stays local.
void writeCompact(std::ostream & out, const JsonNode & node)
{
	switch(node.getType())
	{
	case JsonNode::JsonType::DATA_NULL:
		out << "null";
		return;
	case JsonNode::JsonType::DATA_BOOL:
		out << (node.Bool() ? "true" : "false");
		return;
	case JsonNode::JsonType::DATA_INTEGER:
		out << node.Integer();
		return;
	case JsonNode::JsonType::DATA_FLOAT:
		out << node.Float();
		return;
	case JsonNode::JsonType::DATA_STRING:
	{
		out << '"';
		for(const char character : node.String())
		{
			switch(character)
			{
			case '"': out << "\\\""; break;
			case '\\': out << "\\\\"; break;
			case '\n': out << "\\n"; break;
			case '\r': out << "\\r"; break;
			case '\t': out << "\\t"; break;
			default:
				if(static_cast<unsigned char>(character) < 0x20)
				{
					char buffer[8];
					std::snprintf(buffer, sizeof(buffer), "\\u%04x", character);
					out << buffer;
				}
				else
					out << character;
			}
		}
		out << '"';
		return;
	}
	case JsonNode::JsonType::DATA_VECTOR:
	{
		out << '[';
		bool first = true;
		for(const auto & entry : node.Vector())
		{
			if(!first)
				out << ',';
			first = false;
			writeCompact(out, entry);
		}
		out << ']';
		return;
	}
	case JsonNode::JsonType::DATA_STRUCT:
	{
		out << '{';
		bool first = true;
		for(const auto & [key, value] : node.Struct())
		{
			if(!first)
				out << ',';
			first = false;
			writeCompact(out, JsonNode(key));
			out << ':';
			writeCompact(out, value);
		}
		out << '}';
		return;
	}
	}
}

std::string compactJson(const JsonNode & document)
{
	std::ostringstream stream;
	writeCompact(stream, document);
	return stream.str();
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
		stream << compactJson(document) << "\n";
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

bool ArtifactWriter::savePng(
	SDL_Surface * source,
	const std::string & label,
	bool hasRegion,
	int x,
	int y,
	int w,
	int h,
	std::string & savedName
)
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

namespace
{

/// Minimal self-contained SHA-256 (FIPS 180-4). K holds the fractional parts
/// of the cube roots of the first 64 primes; H holds the fractional parts of
/// the square roots of the first 8 primes.
struct Sha256
{
	std::array<uint32_t, 8> state =
		{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	std::array<uint32_t, 64> table = []()
	{
		std::array<uint32_t, 64> result{};
		uint32_t candidate = 2;
		for(size_t index = 0; index < 64; ++candidate)
		{
			bool prime = true;
			for(uint32_t divisor = 2; divisor * divisor <= candidate; ++divisor)
				if(candidate % divisor == 0)
					prime = false;
			if(!prime)
				continue;
			const double root = std::cbrt(static_cast<double>(candidate));
			result[index++] = static_cast<uint32_t>((root - std::floor(root)) * 4294967296.0);
		}
		return result;
	}();
	uint64_t totalLength = 0;
	std::vector<uint8_t> pending;

	static uint32_t rotr(uint32_t value, int bits) { return (value >> bits) | (value << (32 - bits)); }

	void processBlock(const uint8_t * block)
	{
		std::array<uint32_t, 64> words{};
		for(int index = 0; index < 16; ++index)
			words[index] = (uint32_t(block[index * 4]) << 24) | (uint32_t(block[index * 4 + 1]) << 16)
						 | (uint32_t(block[index * 4 + 2]) << 8) | uint32_t(block[index * 4 + 3]);
		for(int index = 16; index < 64; ++index)
		{
			const uint32_t s0 = rotr(words[index - 15], 7) ^ rotr(words[index - 15], 18) ^ (words[index - 15] >> 3);
			const uint32_t s1 = rotr(words[index - 2], 17) ^ rotr(words[index - 2], 19) ^ (words[index - 2] >> 10);
			words[index] = words[index - 16] + s0 + words[index - 7] + s1;
		}
		uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4], f = state[5], g = state[6],
				 h = state[7];
		for(int index = 0; index < 64; ++index)
		{
			const uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
			const uint32_t ch = (e & f) ^ (~e & g);
			const uint32_t temp1 = h + s1 + ch + table[index] + words[index];
			const uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
			const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
			const uint32_t temp2 = s0 + maj;
			h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
		}
		state[0] += a; state[1] += b; state[2] += c; state[3] += d;
		state[4] += e; state[5] += f; state[6] += g; state[7] += h;
	}

	void update(const std::string & data)
	{
		totalLength += data.size();
		pending.insert(pending.end(), data.begin(), data.end());
		while(pending.size() >= 64)
		{
			processBlock(pending.data());
			pending.erase(pending.begin(), pending.begin() + 64);
		}
	}

	std::string digest()
	{
		const uint64_t bitLength = totalLength * 8;
		pending.push_back(0x80);
		while(pending.size() % 64 != 56)
			pending.push_back(0);
		for(int index = 7; index >= 0; --index)
			pending.push_back(static_cast<uint8_t>(bitLength >> (index * 8)));
		for(size_t offset = 0; offset < pending.size(); offset += 64)
			processBlock(pending.data() + offset);
		std::string result;
		char buffer[3];
		for(const uint32_t word : state)
			for(int byte = 3; byte >= 0; --byte)
			{
				std::snprintf(buffer, sizeof(buffer), "%02x", (word >> (byte * 8)) & 0xff);
				result += buffer;
			}
		return result;
	}
};

}

std::string sha256Hex(const std::string & data)
{
	Sha256 hasher;
	hasher.update(data);
	return hasher.digest();
}

}
