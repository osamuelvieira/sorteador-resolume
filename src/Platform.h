#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace sorteador
{
// All strings crossing the plugin are UTF-8; these convert to and from native paths.
std::filesystem::path PathFromUtf8( const std::string& utf8 );
std::string PathToUtf8( const std::filesystem::path& path );

// Folder where settings and history are stored (created if missing).
std::filesystem::path DataDirectory();

// First bold system font that exists.
std::string DefaultFontPath();

struct FontFile
{
	std::string name;
	std::string path;
};
std::vector< FontFile > ListSystemFonts();

bool ReadFile( const std::filesystem::path& path, std::vector< unsigned char >& out );
// Writes to a temporary file first, so a crash never leaves a half written file behind.
bool WriteFileAtomic( const std::filesystem::path& path, const std::string& contents );

}//namespace sorteador
