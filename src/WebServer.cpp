#define CPPHTTPLIB_THREAD_POOL_COUNT 4
#include <httplib/httplib.h>

#include "WebServer.h"
#include "ChannelManager.h"
#include "Platform.h"
#include "QrCode.h"
#include "web_assets.h"
#include <cmath>

using nlohmann::json;

namespace sorteador
{
namespace
{
constexpr int kPortAttempts = 10;

// Finds the address other devices on the network use to reach this computer. Connecting a UDP socket sends
// nothing, it only makes the OS pick the outgoing interface.
std::string LocalIPv4()
{
	std::string result = "127.0.0.1";
	auto sock          = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
#if defined( _WIN32 )
	if( sock == INVALID_SOCKET )
		return result;
#else
	if( sock < 0 )
		return result;
#endif
	sockaddr_in target{};
	target.sin_family = AF_INET;
	target.sin_port   = htons( 53 );
	inet_pton( AF_INET, "8.8.8.8", &target.sin_addr );
	if( connect( sock, (sockaddr*)&target, sizeof( target ) ) == 0 )
	{
		sockaddr_in local{};
		socklen_t length = sizeof( local );
		char text[ INET_ADDRSTRLEN ] = {};
		if( getsockname( sock, (sockaddr*)&local, &length ) == 0 && inet_ntop( AF_INET, &local.sin_addr, text, sizeof( text ) ) )
			result = text;
	}
#if defined( _WIN32 )
	closesocket( sock );
#else
	close( sock );
#endif
	return result;
}

const char* PhaseName( Phase phase )
{
	switch( phase )
	{
	case Phase::Spinning:
		return "spinning";
	case Phase::Result:
		return "result";
	case Phase::Board:
		return "board";
	default:
		return "idle";
	}
}

// Channels are 1 based in the API, matching what the Resolume parameter shows.
int ChannelFromRequest( const httplib::Request& req )
{
	if( !req.has_param( "canal" ) )
		return 0;
	try
	{
		return ChannelManager::ClampChannel( std::stoi( req.get_param_value( "canal" ) ) - 1 );
	}
	catch( const std::exception& )
	{
		return 0;
	}
}

// The text closest to the center slot is what the audience is reading right now.
std::string CenterText( const FrameView& frame )
{
	std::string display;
	float best = 1e9f;
	for( const VisibleItem& item : frame.items )
		if( std::fabs( item.offset ) < best )
		{
			best    = std::fabs( item.offset );
			display = item.text;
		}
	return display;
}

// The state of a draw that changes as it runs. Its reels and timing are fetched separately (/api/reel), only when
// the serial changes, and the web panel replays the draw on its own, in step with the output.
json RunJson( const Run& r )
{
	return json{
		{ "serial", r.serial },       { "phase", PhaseName( r.phase ) }, { "startTime", r.StartTime() }, { "endTime", r.EndTime() },
		{ "clearTime", r.clearTime }, { "boardTime", r.boardTime },      { "seed", r.seed },
	};
}

json ReelsJson( const Run& r )
{
	json segments = json::array();
	for( const Segment& seg : r.segments )
	{
		json reels = json::array();
		for( const Reel& reel : seg.reels )
			reels.push_back( { { "entries", reel.entries }, { "distance", reel.distance }, { "stop", reel.stop } } );
		segments.push_back( { { "winner", seg.winner }, { "start", seg.start }, { "spin", seg.spin }, { "reels", reels } } );
	}
	return json{
		{ "serial", r.serial }, { "pad", kReelPad }, { "animation", (int)r.animation }, { "grid", r.IsGrid() },
		{ "revealGap", r.revealGap }, { "decel", r.decel }, { "segments", segments },
	};
}

bool TestFromRequest( const httplib::Request& req )
{
	return req.has_param( "teste" ) && req.get_param_value( "teste" ) == "1";
}

void SendJson( httplib::Response& res, const json& body, int status = 200 )
{
	res.status = status;
	res.set_content( body.dump( -1, ' ', false, json::error_handler_t::replace ), "application/json; charset=utf-8" );
}
}//namespace

WebServer::WebServer( ChannelManager& manager ) :
	manager( manager ),
	http( std::make_unique< httplib::Server >() )
{
	RegisterRoutes();
}

WebServer::~WebServer()
{
	Stop();
}

bool WebServer::Start( int preferredPort )
{
	for( int port = preferredPort; port < preferredPort + kPortAttempts; ++port )
	{
		if( !http->bind_to_port( "0.0.0.0", port ) )
			continue;
		address = "http://" + LocalIPv4() + ":" + std::to_string( port );
		thread  = std::thread( [ this ]() { http->listen_after_bind(); } );
		return true;
	}
	address.clear();
	return false;
}

void WebServer::Stop()
{
	if( http )
		http->stop();
	if( thread.joinable() )
		thread.join();
	address.clear();
}

void WebServer::RegisterRoutes()
{
	auto stateJson = [ this ]( int channel ) {
		double now = ChannelManager::Now();
		json state = manager.With( channel, [ now ]( Channel& c ) {
			return json{
				{ "phase", PhaseName( c.GetPhase() ) },
				{ "progress", c.Progress( now ) },
				{ "winner", c.GetPhase() == Phase::Result ? c.GetWinner() : std::string() },
				{ "display", CenterText( c.BuildFrame( now ) ) },
				{ "test",
				  {
					  { "phase", PhaseName( c.GetPhase( true ) ) },
					  { "progress", c.Progress( now, true ) },
					  { "winner", c.GetPhase( true ) == Phase::Result ? c.GetWinner( true ) : std::string() },
					  { "display", CenterText( c.BuildFrame( now, true ) ) },
				  } },
				{ "message", c.GetMessage() },
				{ "history", HistoryToJson( c.GetHistory() ) },
				{ "now", now },
				{ "run", RunJson( c.GetRun() ) },
				{ "testRun", RunJson( c.GetRun( true ) ) },
				{ "available", c.AvailableCount() },
				{ "total", c.config.mode == DrawMode::List ? (long long)c.Items().size() : (long long)c.config.numMax - c.config.numMin + 1 },
				{ "revision", c.revision },
				{ "config", ToJson( c.config ) },
				{ "locked", c.locked },
				{ "canUndoReset", c.CanUndoReset() },
			};
		} );
		const OutputCount outputs = manager.Outputs( channel );
		state[ "outputs" ]     = { { "live", outputs.live }, { "preview", outputs.preview } };
		state[ "channel" ]     = channel + 1;
		state[ "channelNames" ] = manager.Names();
		state[ "numChannels" ] = ChannelManager::kNumChannels;
		state[ "version" ]     = SORTEADOR_VERSION;
		state[ "address" ]     = manager.WebAddress();
		return state;
	};

	// No CORS headers on purpose: the panel is served from here, and Companion or Stream Deck do not need them. With
	// them, any website open on this computer could read the answers and send settings.
	http->set_default_headers( { { "Cache-Control", "no-store" } } );

	http->Get( "/", []( const httplib::Request&, httplib::Response& res ) {
		res.set_content( reinterpret_cast< const char* >( kIndexHtml ), kIndexHtmlSize, "text/html; charset=utf-8" );
	} );

	http->Get( "/api/state", [ stateJson ]( const httplib::Request& req, httplib::Response& res ) {
		SendJson( res, stateJson( ChannelFromRequest( req ) ) );
	} );

	http->Post( "/api/config", [ this, stateJson ]( const httplib::Request& req, httplib::Response& res ) {
		json body = json::parse( req.body, nullptr, false );
		if( !body.is_object() )
			return SendJson( res, { { "error", "JSON inválido" } }, 400 );
		// /api/font serves this file, so it has to be a real font, not any file on the computer.
		for( const char* key : { "fontPath", "titleFontPath" } )
		{
			auto fontPath = body.find( key );
			if( fontPath != body.end() && fontPath->is_string() && !fontPath->get< std::string >().empty() &&
			    !IsSystemFont( fontPath->get< std::string >() ) )
				return SendJson( res, { { "error", "Fonte inválida: escolha uma fonte da lista." } }, 400 );
		}
		int channel = ChannelFromRequest( req );
		if( manager.With( channel, []( Channel& c ) { return c.locked; } ) )
			return SendJson( res, { { "error", "Configurações travadas. Destrave no painel para mudar." } }, 423 );
		try
		{
			manager.With( channel, [ &body ]( Channel& c ) {
				// Merged into a copy so a bad value halfway through leaves the settings untouched.
				DrawConfig updated = c.config;
				MergeJson( updated, body );
				c.config = updated;
				c.TouchConfig();
			} );
		}
		catch( const std::exception& e )
		{
			return SendJson( res, { { "error", std::string( "Valor inválido: " ) + e.what() } }, 400 );
		}
		SendJson( res, stateJson( channel ) );
	} );

	// Actions answer to GET too, so a Stream Deck or Companion button can trigger them with a plain URL.
	auto action = [ this, stateJson ]( const char* path, std::function< void( Channel&, const httplib::Request& ) > run ) {
		auto handler = [ this, stateJson, run ]( const httplib::Request& req, httplib::Response& res ) {
			int channel = ChannelFromRequest( req );
			manager.With( channel, [ & ]( Channel& c ) { run( c, req ); } );
			SendJson( res, stateJson( channel ) );
		};
		http->Get( path, handler );
		http->Post( path, handler );
	};

	action( "/api/start", []( Channel& c, const httplib::Request& ) { c.Start( ChannelManager::Now() ); } );
	action( "/api/clear", []( Channel& c, const httplib::Request& ) { c.Clear( ChannelManager::Now() ); } );
	// Test draws only show on clips in preview mode and never touch the history.
	action( "/api/test", []( Channel& c, const httplib::Request& ) { c.Start( ChannelManager::Now(), true ); } );
	action( "/api/test/clear", []( Channel& c, const httplib::Request& ) { c.Clear( ChannelManager::Now(), true ); } );
	action( "/api/board", []( Channel& c, const httplib::Request& ) { c.ShowBoard( ChannelManager::Now() ); } );
	action( "/api/test/board", []( Channel& c, const httplib::Request& ) { c.ShowBoard( ChannelManager::Now(), true ); } );
	// /api/rename?nome=Brindes names the channel; an empty name brings back "Canal N".
	action( "/api/rename", []( Channel& c, const httplib::Request& req ) {
		if( c.locked )
			return;
		std::string name = req.get_param_value( "nome" );
		if( name.size() > 40 )
			name.resize( 40 );
		if( name != c.name )
		{
			c.name = name;
			c.Touch();
		}
	} );
	action( "/api/redraw", []( Channel& c, const httplib::Request& ) { c.Redraw( ChannelManager::Now() ); } );
	action( "/api/reset-history", []( Channel& c, const httplib::Request& ) {
		if( !c.locked )
			c.ResetHistory();
	} );
	action( "/api/history/undo-reset", []( Channel& c, const httplib::Request& ) { c.UndoReset(); } );
	// /api/lock?on=1 locks the settings of a channel, /api/lock?on=0 unlocks them.
	action( "/api/lock", []( Channel& c, const httplib::Request& req ) {
		const bool on = req.get_param_value( "on" ) != "0";
		if( on != c.locked )
		{
			c.locked = on;
			c.Touch();
		}
	} );
	action( "/api/history/remove", []( Channel& c, const httplib::Request& req ) {
		if( req.has_param( "index" ) )
			c.RemoveHistory( (size_t)std::strtoul( req.get_param_value( "index" ).c_str(), nullptr, 10 ) );
	} );

	http->Get( "/api/reel", [ this ]( const httplib::Request& req, httplib::Response& res ) {
		const bool test = TestFromRequest( req );
		json reel       = manager.With( ChannelFromRequest( req ), [ test ]( Channel& c ) {
			const Run& r = c.GetRun( test );
			return ReelsJson( r );
		} );
		SendJson( res, reel );
	} );

	// The font the output uses, so the panel draws with the same letters. Only the channel's own font is served, and
	// only from the system font folders, whatever ended up in the saved settings.
	http->Get( "/api/font", [ this ]( const httplib::Request& req, httplib::Response& res ) {
		// ?titulo=1 asks for the title font, which falls back to the names font.
		const bool title = req.get_param_value( "titulo" ) == "1";
		std::string path = manager.With( ChannelFromRequest( req ), [ title ]( Channel& c ) {
			return title && !c.config.titleFontPath.empty() ? c.config.titleFontPath : c.config.fontPath;
		} );
		if( path.empty() )
			path = DefaultFontPath();
		std::vector< unsigned char > bytes;
		if( path.empty() || !IsSystemFont( path ) || !ReadFile( PathFromUtf8( path ), bytes ) )
		{
			res.status = 404;
			return;
		}
		res.set_content( reinterpret_cast< const char* >( bytes.data() ), bytes.size(), "application/octet-stream" );
	} );

	// The QR code the waiting screen shows, so the panel can draw the same one.
	http->Get( "/api/qr", [ this ]( const httplib::Request& req, httplib::Response& res ) {
		const std::string address = manager.WebAddress();
		const std::string url     = address.empty() ? std::string() : address + "/?canal=" + std::to_string( ChannelFromRequest( req ) + 1 );
		SendJson( res, { { "url", url }, { "modules", url.empty() ? std::vector< std::string >() : QrModules( url ) } } );
	} );

	// Default settings, for "Restaurar padrão" in the panel.
	http->Get( "/api/defaults", []( const httplib::Request&, httplib::Response& res ) { SendJson( res, ToJson( DrawConfig{} ) ); } );

	http->Get( "/api/fonts", []( const httplib::Request&, httplib::Response& res ) {
		json fonts = json::array();
		for( const FontFile& font : ListSystemFonts() )
			fonts.push_back( { { "name", font.name }, { "path", font.path } } );
		SendJson( res, { { "fonts", fonts }, { "default", DefaultFontPath() } } );
	} );
}

}//namespace sorteador
