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
// True if the path is a font file directly inside one of the folders ListSystemFonts() looks in.
bool IsSystemFont( const std::string& utf8 );

bool ReadFile( const std::filesystem::path& path, std::vector< unsigned char >& out );
// Writes to a temporary file first, so a crash never leaves a half written file behind.
bool WriteFileAtomic( const std::filesystem::path& path, const std::string& contents );

// Opens the URL in the default browser without blocking the caller.
void OpenInBrowser( const std::string& url );

}//namespace sorteador
