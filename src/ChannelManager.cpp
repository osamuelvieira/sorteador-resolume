#include "ChannelManager.h"
#include "Platform.h"
#include "WebServer.h"
#include <chrono>

using nlohmann::json;

namespace sorteador
{
namespace
{
constexpr int kStateVersion = 1;

std::filesystem::path StateFile()
{
	return DataDirectory() / "estado.json";
}
}//namespace

ChannelManager& ChannelManager::Get()
{
	static ChannelManager instance;
	return instance;
}

double ChannelManager::Now()
{
	using namespace std::chrono;
	static const steady_clock::time_point epoch = steady_clock::now();
	return duration< double >( steady_clock::now() - epoch ).count();
}

int ChannelManager::ClampChannel( int channel )
{
	return channel < 0 ? 0 : ( channel >= kNumChannels ? kNumChannels - 1 : channel );
}

ChannelManager::ChannelManager() = default;

ChannelManager::~ChannelManager()
{
	// Only reached when the host exits without unloading the plugin. The OS has already stopped our threads by now
	// and joining them could deadlock, so they are let go.
	if( saver.joinable() )
		saver.detach();
	if( server )
		(void)server.release();
}

void ChannelManager::Startup()
{
	if( started )
		return;
	started = true;

	Load();
	savedRevision = RevisionSum();

	stopSaver = false;
	saver     = std::thread( &ChannelManager::SaverLoop, this );

	server = std::make_unique< WebServer >( *this );
	server->Start( port );
}

void ChannelManager::Shutdown()
{
	if( !started )
		return;
	started = false;

	if( server )
	{
		server->Stop();
		server.reset();
	}

	{
		std::lock_guard< std::mutex > lock( saverMutex );
		stopSaver = true;
	}
	saverWake.notify_all();
	if( saver.joinable() )
		saver.join();
	Save();
}

std::string ChannelManager::WebAddress() const
{
	return server ? server->Address() : std::string();
}

uint64_t ChannelManager::RevisionSum()
{
	std::lock_guard< std::mutex > lock( mutex );
	uint64_t sum = 0;
	for( const Channel& c : channels )
		sum += c.revision;
	return sum;
}

void ChannelManager::SaverLoop()
{
	std::unique_lock< std::mutex > lock( saverMutex );
	while( !stopSaver )
	{
		saverWake.wait_for( lock, std::chrono::seconds( 1 ) );
		if( stopSaver )
			break;
		uint64_t revision = RevisionSum();
		if( revision != savedRevision )
		{
			savedRevision = revision;
			Save();
		}
	}
}

void ChannelManager::Load()
{
	std::vector< unsigned char > bytes;
	if( !ReadFile( StateFile(), bytes ) )
		return;

	json state = json::parse( bytes.begin(), bytes.end(), nullptr, false );
	if( !state.is_object() )
		return;

	try
	{
		if( state.contains( "port" ) )
			port = state[ "port" ].get< int >();

		std::lock_guard< std::mutex > lock( mutex );
		const json& saved = state.value( "channels", json::array() );
		for( size_t i = 0; i < saved.size() && i < channels.size(); ++i )
		{
			MergeJson( channels[ i ].config, saved[ i ].value( "config", json::object() ) );
			channels[ i ].SetHistory( saved[ i ].value( "history", std::vector< std::string >() ) );
		}
	}
	catch( const std::exception& )
	{
		// A damaged file only costs the saved settings, the plugin still starts with defaults.
	}
}

void ChannelManager::Save()
{
	json state = { { "version", kStateVersion }, { "port", port }, { "channels", json::array() } };
	{
		std::lock_guard< std::mutex > lock( mutex );
		for( const Channel& c : channels )
			state[ "channels" ].push_back( { { "config", ToJson( c.config ) }, { "history", c.GetHistory() } } );
	}
	WriteFileAtomic( StateFile(), state.dump( 2, ' ', false, json::error_handler_t::replace ) );
}

}//namespace sorteador
