#include "Channel.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace sorteador
{
namespace
{
constexpr long long kMinDistance      = 8;
constexpr long long kMaxDistance      = 20000;
constexpr long long kMaxEnumerated    = 200000;// number ranges up to this size are enumerated exactly
constexpr double kHighlightTime       = 0.3;
constexpr double kPulseTime           = 0.45;
constexpr double kNeighborFadeOutTime = 0.5;
constexpr double kPi                  = 3.14159265358979323846;

std::string Trim( const std::string& s )
{
	const char* ws = " \t\r\n";
	size_t begin   = s.find_first_not_of( ws );
	if( begin == std::string::npos )
		return {};
	size_t end = s.find_last_not_of( ws );
	return s.substr( begin, end - begin + 1 );
}

double Clamp01( double v )
{
	return std::min( 1.0, std::max( 0.0, v ) );
}
}//namespace

Channel::Channel() :
	rng( std::random_device{}() )
{
}

std::vector< std::string > Channel::Items() const
{
	// One entry per line. A single line separated by ';' is also accepted, which is how the Resolume text parameter edits the list.
	const char separator = config.listText.find( '\n' ) == std::string::npos ? ';' : '\n';
	std::vector< std::string > items;
	size_t start = 0;
	while( start <= config.listText.size() )
	{
		size_t end = config.listText.find( separator, start );
		if( end == std::string::npos )
			end = config.listText.size();
		std::string item = Trim( config.listText.substr( start, end - start ) );
		if( !item.empty() )
			items.push_back( item );
		start = end + 1;
	}
	return items;
}

std::string Channel::FormatNumber( long long value ) const
{
	std::string digits = std::to_string( std::llabs( value ) );
	if( config.zeroPad )
	{
		size_t width = std::max( std::to_string( std::llabs( (long long)config.numMin ) ).size(),
		                         std::to_string( std::llabs( (long long)config.numMax ) ).size() );
		if( digits.size() < width )
			digits.insert( 0, width - digits.size(), '0' );
	}
	return value < 0 ? "-" + digits : digits;
}

std::vector< std::string > Channel::AvailableItems() const
{
	std::vector< std::string > items = Items();
	if( !config.noRepeat )
		return items;

	// Each history entry consumes one occurrence, so duplicated names in the list can each be drawn once.
	std::unordered_map< std::string, int > drawn;
	for( const HistoryEntry& h : history )
		++drawn[ h.text ];
	std::vector< std::string > available;
	for( const std::string& item : items )
	{
		auto it = drawn.find( item );
		if( it != drawn.end() && it->second > 0 )
			--it->second;
		else
			available.push_back( item );
	}
	return available;
}

size_t Channel::AvailableCount() const
{
	if( config.mode == DrawMode::List )
		return AvailableItems().size();

	long long count = (long long)config.numMax - config.numMin + 1;
	if( !config.noRepeat )
		return (size_t)count;
	std::set< std::string > drawn;
	for( const HistoryEntry& h : history )
		drawn.insert( h.text );
	long long used = 0;
	for( long long v = config.numMin; v <= config.numMax && count <= kMaxEnumerated; ++v )
		used += drawn.count( FormatNumber( v ) );
	if( count > kMaxEnumerated )
		used = (long long)drawn.size();
	return (size_t)std::max( 0LL, count - used );
}

bool Channel::PickAvailableNumber( long long& out )
{
	const long long count = (long long)config.numMax - config.numMin + 1;
	std::uniform_int_distribution< long long > any( config.numMin, config.numMax );
	if( !config.noRepeat )
	{
		out = any( rng );
		return true;
	}

	std::unordered_set< std::string > drawn;
	for( const HistoryEntry& h : history )
		drawn.insert( h.text );
	if( count <= kMaxEnumerated )
	{
		std::vector< long long > available;
		for( long long v = config.numMin; v <= config.numMax; ++v )
			if( !drawn.count( FormatNumber( v ) ) )
				available.push_back( v );
		if( available.empty() )
			return false;
		out = available[ std::uniform_int_distribution< size_t >( 0, available.size() - 1 )( rng ) ];
		return true;
	}

	// Huge ranges: the history can only cover a tiny part of it, so rejection sampling finishes almost immediately.
	for( int attempt = 0; attempt < 10000; ++attempt )
	{
		out = any( rng );
		if( !drawn.count( FormatNumber( out ) ) )
			return true;
	}
	return false;
}

bool Channel::Start( double now, bool test )
{
	Run& r = test ? testRun : run;
	if( r.phase == Phase::Spinning )
		return false;

	std::vector< std::string > pool;
	std::string winner;
	if( config.mode == DrawMode::List )
	{
		std::vector< std::string > items = Items();
		if( items.empty() )
		{
			message = "A lista está vazia.";
			Touch();
			return false;
		}
		std::vector< std::string > available = AvailableItems();
		// A test still works when everyone was drawn, it just picks from the whole list.
		if( available.empty() && test )
			available = items;
		if( available.empty() )
		{
			message = "Todos os itens já foram sorteados.";
			Touch();
			return false;
		}
		winner = available[ std::uniform_int_distribution< size_t >( 0, available.size() - 1 )( rng ) ];
		// The reel shows what can still come out, unless that is too little to make an animation.
		pool = available.size() >= 2 ? available : items;
	}
	else
	{
		long long value = 0;
		if( !PickAvailableNumber( value ) )
		{
			if( !test )
			{
				message = "Todos os números já foram sorteados.";
				Touch();
				return false;
			}
			value = std::uniform_int_distribution< long long >( config.numMin, config.numMax )( rng );
		}
		winner = FormatNumber( value );
	}

	auto randomEntry = [ & ]() -> std::string {
		if( !pool.empty() )
			return pool[ std::uniform_int_distribution< size_t >( 0, pool.size() - 1 )( rng ) ];
		return FormatNumber( std::uniform_int_distribution< long long >( config.numMin, config.numMax )( rng ) );
	};
	const bool canVary = config.mode == DrawMode::Numbers ? config.numMax > config.numMin : pool.size() > 1;

	// The easing 1 - (1 - x)^k starts with a slope of k, so this distance gives the configured initial speed.
	long long distance = std::llround( config.speed * config.duration / config.decel );
	distance           = std::min( kMaxDistance, std::max( kMinDistance, distance ) );

	std::vector< std::string > reel( (size_t)( distance + 2 * kReelPad + 1 ), std::string() );
	const size_t winnerSlot = (size_t)( kReelPad + distance );
	for( size_t i = 0; i < reel.size(); ++i )
	{
		if( i == winnerSlot )
		{
			reel[ i ] = winner;
			continue;
		}
		std::string entry = randomEntry();
		// Avoid showing the same text twice in a row, and keep the winner from appearing right next to itself.
		for( int retry = 0; canVary && retry < 8; ++retry )
		{
			bool repeatsPrevious = i > 0 && entry == reel[ i - 1 ];
			bool touchesWinner   = ( i + 1 == winnerSlot || i == winnerSlot + 1 ) && entry == winner;
			if( !repeatsPrevious && !touchesWinner )
				break;
			entry = randomEntry();
		}
		reel[ i ] = entry;
	}

	r.serial    = ++serial;
	r.winner    = winner;
	r.reel      = std::move( reel );
	r.distance  = distance;
	r.startTime = now;
	r.spinTime  = config.duration;
	r.decel     = config.decel;
	r.seed      = (uint32_t)rng();
	r.phase     = Phase::Spinning;
	message.clear();
	Touch();
	return true;
}

void Channel::Tick( double now )
{
	if( run.phase == Phase::Spinning && now >= run.startTime + run.spinTime )
	{
		run.phase = Phase::Result;
		history.push_back( { run.winner, (int64_t)std::time( nullptr ) } );
		Touch();
	}
	if( testRun.phase == Phase::Spinning && now >= testRun.startTime + testRun.spinTime )
	{
		testRun.phase = Phase::Result;
		Touch();
	}
}

void Channel::Clear( bool test )
{
	( test ? testRun : run ).phase = Phase::Idle;
	message.clear();
	Touch();
}

void Channel::ResetHistory()
{
	history.clear();
	message.clear();
	Touch();
}

void Channel::RemoveHistory( size_t index )
{
	if( index < history.size() )
	{
		history.erase( history.begin() + (long)index );
		Touch();
	}
}

void Channel::SetHistory( std::vector< HistoryEntry > newHistory )
{
	history = std::move( newHistory );
	Touch();
}

double Channel::Progress( double now, bool test ) const
{
	const Run& r = GetRun( test );
	switch( r.phase )
	{
	case Phase::Spinning:
		return Clamp01( ( now - r.startTime ) / r.spinTime );
	case Phase::Result:
		return 1.0;
	default:
		return 0.0;
	}
}

namespace
{
double ReelPosition( const Run& r, double now )
{
	double x = Clamp01( ( now - r.startTime ) / r.spinTime );
	return (double)r.distance * ( 1.0 - std::pow( 1.0 - x, r.decel ) );
}

const std::string& ReelAt( const Run& r, long long index )
{
	long long i = std::min( (long long)r.reel.size() - 1, std::max( 0LL, index + kReelPad ) );
	return r.reel[ (size_t)i ];
}
}//namespace

nlohmann::json HistoryToJson( const std::vector< HistoryEntry >& history )
{
	nlohmann::json out = nlohmann::json::array();
	for( const HistoryEntry& h : history )
		out.push_back( { { "name", h.text }, { "time", h.time } } );
	return out;
}

FrameView Channel::BuildFrame( double now, bool test ) const
{
	const Run& r = GetRun( test );
	FrameView frame;
	frame.config = config;
	frame.phase  = r.phase;
	frame.seed   = r.seed;

	const int k          = config.neighbors;
	const bool showsReel = config.animation != Animation::Flip;

	if( r.phase == Phase::Idle )
	{
		if( !config.idleText.empty() )
			frame.items.push_back( { config.idleText } );
	}
	else if( r.phase == Phase::Spinning )
	{
		double position = ReelPosition( r, now );
		if( !showsReel )
		{
			frame.items.push_back( { ReelAt( r, std::llround( position ) ) } );
		}
		else
		{
			long long base = (long long)std::floor( position );
			for( long long i = base - k - 1; i <= base + k + 1; ++i )
			{
				float offset = (float)( i - position );
				float alpha  = 1.0f - std::fabs( offset ) / ( k + 1 );
				if( alpha > 0.0f )
					frame.items.push_back( { ReelAt( r, i ), offset, alpha } );
			}
		}
	}
	else
	{
		double sinceResult = now - ( r.startTime + r.spinTime );
		frame.sinceResult  = (float)sinceResult;
		VisibleItem item;
		item.text      = r.winner;
		item.winnerMix = (float)Clamp01( sinceResult / kHighlightTime );
		item.scale     = 1.0f + 0.18f * (float)std::sin( kPi * Clamp01( sinceResult / kPulseTime ) );

		if( showsReel )
		{
			float fade = 1.0f - (float)Clamp01( sinceResult / kNeighborFadeOutTime );
			for( long long d = -k - 1; fade > 0.0f && d <= k + 1; ++d )
			{
				float alpha = ( 1.0f - std::fabs( (float)d ) / ( k + 1 ) ) * fade;
				if( d != 0 && alpha > 0.0f )
					frame.items.push_back( { ReelAt( r, r.distance + d ), (float)d, alpha } );
			}
		}
		frame.items.push_back( item );
	}
	return frame;
}

}//namespace sorteador
