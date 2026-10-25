#pragma once
#include <memory>
#include <string>
#include <thread>

namespace httplib
{
class Server;
}

namespace sorteador
{
class ChannelManager;

// Small HTTP server running inside Resolume that serves the web control panel and its JSON API.
// Kept free of httplib/winsock includes so this header can live next to the OpenGL/windows.h ones.
class WebServer
{
public:
	explicit WebServer( ChannelManager& manager );
	~WebServer();

	// Tries the preferred port and the next few ones, in case something else is using it.
	bool Start( int preferredPort );
	void Stop();
	std::string Address() const { return address; }

private:
	void RegisterRoutes();

	ChannelManager& manager;
	std::unique_ptr< httplib::Server > http;
	std::thread thread;
	std::string address;
};

}//namespace sorteador
