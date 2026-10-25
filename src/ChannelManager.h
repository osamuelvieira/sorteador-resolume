#pragma once
#include "Channel.h"
#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace sorteador
{
class WebServer;

// Process wide state shared by every plugin instance and the web server.
// Instances on the same channel show the same draw, perfectly in sync because they share one clock.
class ChannelManager
{
public:
	static constexpr int kNumChannels = 4;
	static constexpr int kDefaultPort = 8090;

	static ChannelManager& Get();
	// Seconds on a monotonic clock, shared by all instances.
	static double Now();

	// Called once when Resolume loads / unloads the plugin library.
	void Startup();
	void Shutdown();

	// Runs f with exclusive access to a channel (0 based). The channel is ticked first.
	template< typename F >
	auto With( int channel, F&& f ) -> decltype( f( std::declval< Channel& >() ) )
	{
		std::lock_guard< std::mutex > lock( mutex );
		Channel& c = channels[ ClampChannel( channel ) ];
		c.Tick( Now() );
		return f( c );
	}

	static int ClampChannel( int channel );
	// e.g. "http://192.168.0.10:8090", empty if the server is not running.
	std::string WebAddress() const;

private:
	ChannelManager();
	~ChannelManager();

	void Load();
	void Save();
	void SaverLoop();
	uint64_t RevisionSum();

	std::mutex mutex;
	std::array< Channel, kNumChannels > channels;
	int port = kDefaultPort;

	std::unique_ptr< WebServer > server;
	std::thread saver;
	std::mutex saverMutex;
	std::condition_variable saverWake;
	bool stopSaver   = false;
	bool started     = false;
	uint64_t savedRevision = 0;
};

}//namespace sorteador
