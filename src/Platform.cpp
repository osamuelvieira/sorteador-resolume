#include "Platform.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <system_error>
#include <thread>

#if defined( _WIN32 )
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#else
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
#endif

namespace fs = std::filesystem;

namespace sorteador
{
namespace
{
std::vector< fs::path > FontDirectories()
{
#if defined( _WIN32 )
	std::vector< fs::path > dirs;
	if( const wchar_t* windir = _wgetenv( L"WINDIR" ) )
		dirs.push_back( fs::path( windir ) / L"Fonts" );
	else
		dirs.push_back( L"C:\\Windows\\Fonts" );
	// Fonts installed "for this user only" live here.
	if( const wchar_t* local = _wgetenv( L"LOCALAPPDATA" ) )
		dirs.push_back( fs::path( local ) / L"Microsoft" / L"Windows" / L"Fonts" );
	return dirs;
#else
	std::vector< fs::path > dirs = { "/System/Library/Fonts", "/System/Library/Fonts/Supplemental", "/Library/Fonts" };
	if( const char* home = std::getenv( "HOME" ) )
		dirs.push_back( fs::path( home ) / "Library" / "Fonts" );
	return dirs;
#endif
}

std::string Lower( std::string s )
{
	std::transform( s.begin(), s.end(), s.begin(), []( unsigned char c ) { return (char)std::tolower( c ); } );
	return s;
}

bool IsFontExtension( const fs::path& path )
{
	std::string ext = Lower( PathToUtf8( path.extension() ) );
	return ext == ".ttf" || ext == ".otf" || ext == ".ttc";
}
}//namespace

fs::path PathFromUtf8( const std::string& utf8 )
{
	return fs::u8path( utf8 );
}

std::string PathToUtf8( const fs::path& path )
{
	return path.u8string();
}

fs::path DataDirectory()
{
	// Lets tests and portable setups keep their state somewhere else.
	if( const char* custom = std::getenv( "SORTEADOR_DATA_DIR" ) )
	{
		std::error_code ec;
		fs::create_directories( PathFromUtf8( custom ), ec );
		return PathFromUtf8( custom );
	}
#if defined( _WIN32 )
	const wchar_t* appdata = _wgetenv( L"APPDATA" );
	fs::path dir           = appdata ? fs::path( appdata ) : fs::temp_directory_path();
#else
	const char* home = std::getenv( "HOME" );
	fs::path dir     = home ? fs::path( home ) / "Library" / "Application Support" : fs::temp_directory_path();
#endif
	dir /= "SorteadorResolume";
	std::error_code ec;
	fs::create_directories( dir, ec );
	return dir;
}

std::string DefaultFontPath()
{
#if defined( _WIN32 )
	const char* candidates[] = { "segoeuib.ttf", "arialbd.ttf", "arial.ttf", "tahomabd.ttf" };
#else
	const char* candidates[] = { "Arial Bold.ttf", "Helvetica.ttc", "Arial.ttf" };
#endif
	std::error_code ec;
	for( const char* name : candidates )
		for( const fs::path& dir : FontDirectories() )
			if( fs::exists( dir / name, ec ) )
				return PathToUtf8( dir / name );
	return {};
}

std::vector< FontFile > ListSystemFonts()
{
	std::vector< FontFile > fonts;
	for( const fs::path& dir : FontDirectories() )
	{
		std::error_code ec;
		for( fs::directory_iterator it( dir, ec ), end; !ec && it != end; it.increment( ec ) )
			if( IsFontExtension( it->path() ) )
				fonts.push_back( { PathToUtf8( it->path().stem() ), PathToUtf8( it->path() ) } );
	}
	std::sort( fonts.begin(), fonts.end(), []( const FontFile& a, const FontFile& b ) { return Lower( a.name ) < Lower( b.name ); } );
	return fonts;
}

bool IsSystemFont( const std::string& utf8 )
{
	// Resolved first, so "..", symbolic links or a different spelling cannot step out of the font folders.
	std::error_code ec;
	const fs::path path = fs::weakly_canonical( PathFromUtf8( utf8 ), ec );
	if( ec || !IsFontExtension( path ) || !fs::is_regular_file( path, ec ) )
		return false;
	for( const fs::path& dir : FontDirectories() )
	{
		const fs::path folder = fs::weakly_canonical( dir, ec );
		if( !ec && path.parent_path() == folder )
			return true;
	}
	return false;
}

bool ReadFile( const fs::path& path, std::vector< unsigned char >& out )
{
	std::ifstream file( path, std::ios::binary );
	if( !file )
		return false;
	out.assign( std::istreambuf_iterator< char >( file ), std::istreambuf_iterator< char >() );
	return true;
}

bool WriteFileAtomic( const fs::path& path, const std::string& contents )
{
	fs::path temp = path;
	temp += ".tmp";
	{
		std::ofstream file( temp, std::ios::binary | std::ios::trunc );
		if( !file )
			return false;
		file.write( contents.data(), (std::streamsize)contents.size() );
		if( !file )
			return false;
	}
	std::error_code ec;
	fs::rename( temp, path, ec );
	return !ec;
}

void OpenInBrowser( const std::string& url )
{
	// Runs on its own thread: starting the browser can take a moment and Resolume's UI must not freeze.
	std::thread( [ url ]() {
#if defined( _WIN32 )
		CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );
		std::wstring wide( url.begin(), url.end() );// the URL is plain ASCII
		ShellExecuteW( nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL );
		CoUninitialize();
#else
		pid_t pid;
		const char* args[] = { "open", url.c_str(), nullptr };
		if( posix_spawnp( &pid, "open", nullptr, nullptr, const_cast< char** >( args ), environ ) == 0 )
			waitpid( pid, nullptr, 0 );
#endif
	} ).detach();
}

}//namespace sorteador
