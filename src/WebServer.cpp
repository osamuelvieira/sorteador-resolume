#define CPPHTTPLIB_THREAD_POOL_COUNT 4
#include <httplib/httplib.h>

#include "WebServer.h"
#include "ChannelManager.h"
#include "Platform.h"
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
			FrameView frame = c.BuildFrame( now );
			// The text closest to the center slot is what the audience is reading right now.
			std::string display;
			float best = 1e9f;
			for( const VisibleItem& item : frame.items )
				if( std::fabs( item.offset ) < best )
				{
					best    = std::fabs( item.offset );
					display = item.text;
				}
			return json{
				{ "phase", PhaseName( c.GetPhase() ) },
				{ "progress", c.Progress( now ) },
				{ "winner", c.GetPhase() == Phase::Result ? c.GetWinner() : std::string() },
				{ "display", display },
				{ "message", c.GetMessage() },
				{ "history", c.GetHistory() },
				{ "available", c.AvailableCount() },
				{ "total", c.config.mode == DrawMode::List ? (long long)c.Items().size() : (long long)c.config.numMax - c.config.numMin + 1 },
				{ "revision", c.revision },
				{ "config", ToJson( c.config ) },
			};
		} );
		state[ "channel" ]     = channel + 1;
		state[ "numChannels" ] = ChannelManager::kNumChannels;
		state[ "address" ]     = manager.WebAddress();
		return state;
	};

	http->set_default_headers( { { "Access-Control-Allow-Origin", "*" }, { "Cache-Control", "no-store" } } );
	http->Options( R"(/api/.*)", []( const httplib::Request&, httplib::Response& res ) {
		res.set_header( "Access-Control-Allow-Methods", "GET, POST, OPTIONS" );
		res.set_header( "Access-Control-Allow-Headers", "Content-Type" );
		res.status = 204;
	} );

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
		int channel = ChannelFromRequest( req );
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
	action( "/api/clear", []( Channel& c, const httplib::Request& ) { c.Clear(); } );
	action( "/api/reset-history", []( Channel& c, const httplib::Request& ) { c.ResetHistory(); } );
	action( "/api/history/remove", []( Channel& c, const httplib::Request& req ) {
		if( req.has_param( "index" ) )
			c.RemoveHistory( (size_t)std::strtoul( req.get_param_value( "index" ).c_str(), nullptr, 10 ) );
	} );

	http->Get( "/api/fonts", []( const httplib::Request&, httplib::Response& res ) {
		json fonts = json::array();
		for( const FontFile& font : ListSystemFonts() )
			fonts.push_back( { { "name", font.name }, { "path", font.path } } );
		SendJson( res, { { "fonts", fonts }, { "default", DefaultFontPath() } } );
	} );
}

}//namespace sorteador
