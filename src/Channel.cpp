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
constexpr double kResultFadeOutTime   = 0.6;// before an automatic return to the waiting screen
constexpr double kIdleFadeInTime      = 0.4;
constexpr double kPi                  = 3.14159265358979323846;
constexpr size_t kWheelSlices         = 16;

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

std::string ItemName( const std::string& item )
{
	size_t bar = item.find( '|' );
	return bar == std::string::npos ? item : Trim( item.substr( 0, bar ) );
}

std::string ItemDetail( const std::string& item )
{
	size_t bar = item.find( '|' );
	return bar == std::string::npos ? std::string() : Trim( item.substr( bar + 1 ) );
}

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

bool Channel::PickAvailableNumber( long long& out, const std::vector< std::string >& alsoTaken )
{
	const long long count = (long long)config.numMax - config.numMin + 1;
	std::uniform_int_distribution< long long > any( config.numMin, config.numMax );
	if( !config.noRepeat && alsoTaken.empty() )
	{
		out = any( rng );
		return true;
	}

	// The numbers drawn before (when they may not repeat) and the ones already picked for this same draw.
	std::unordered_set< std::string > drawn( alsoTaken.begin(), alsoTaken.end() );
	if( config.noRepeat )
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

namespace
{
// UTF-8 text split into characters, for the columns of the slot machine.
std::vector< std::string > SplitCharacters( const std::string& s )
{
	std::vector< std::string > out;
	for( size_t i = 0; i < s.size(); )
	{
		const unsigned char c = (unsigned char)s[ i ];
		size_t length         = c < 0x80 ? 1 : ( c >> 5 ) == 0x6 ? 2 : ( c >> 4 ) == 0xE ? 3 : ( c >> 3 ) == 0x1E ? 4 : 1;
		length                = std::min( length, s.size() - i );
		out.push_back( s.substr( i, length ) );
		i += length;
	}
	return out;
}
}//namespace

Segment Channel::BuildSegment( const std::string& winner, const std::vector< std::string >& pool, Animation animation, double start, double spin )
{
	Segment seg;
	seg.winner                   = winner;
	seg.start                    = start;
	seg.spin                     = spin;
	const std::string winnerName = ItemName( winner );
	const bool numbers           = config.mode == DrawMode::Numbers;

	auto randomEntry = [ & ]() -> std::string {
		if( !pool.empty() )
			return ItemName( pool[ std::uniform_int_distribution< size_t >( 0, pool.size() - 1 )( rng ) ] );
		return FormatNumber( std::uniform_int_distribution< long long >( config.numMin, config.numMax )( rng ) );
	};
	const bool canVary = numbers ? config.numMax > config.numMin : pool.size() > 1;
	// The easing 1 - (1 - x)^k starts with a slope of k, so this distance gives the configured initial speed.
	auto distanceFor = [ & ]( double seconds ) {
		return std::min( kMaxDistance, std::max( kMinDistance, std::llround( config.speed * seconds / config.decel ) ) );
	};
	auto makeReel = [ & ]( const std::string& target, long long distance, double stop, bool vary, auto&& next ) {
		Reel reel;
		reel.distance = distance;
		reel.stop     = stop;
		reel.entries.assign( (size_t)( distance + 2 * kReelPad + 1 ), std::string() );
		const size_t targetSlot = (size_t)( kReelPad + distance );
		for( size_t i = 0; i < reel.entries.size(); ++i )
		{
			if( i == targetSlot )
			{
				reel.entries[ i ] = target;
				continue;
			}
			std::string entry = next();
			// Avoid showing the same text twice in a row, and keep the target from appearing right next to itself.
			for( int retry = 0; vary && retry < 8; ++retry )
			{
				bool repeatsPrevious = i > 0 && entry == reel.entries[ i - 1 ];
				bool touchesTarget   = ( i + 1 == targetSlot || i == targetSlot + 1 ) && entry == target;
				if( !repeatsPrevious && !touchesTarget )
					break;
				entry = next();
			}
			reel.entries[ i ] = entry;
		}
		return reel;
	};

	if( animation == Animation::Wheel )
	{
		// Up to kWheelSlices different slices, the winner among them, in random order.
		std::vector< std::string > slices = { winnerName };
		std::unordered_set< std::string > used = { winnerName };
		for( int attempt = 0; canVary && attempt < 400 && slices.size() < kWheelSlices; ++attempt )
		{
			std::string entry = randomEntry();
			if( used.insert( entry ).second )
				slices.push_back( entry );
		}
		std::shuffle( slices.begin(), slices.end(), rng );
		const long long count = (long long)slices.size();
		const long long index = std::find( slices.begin(), slices.end(), winnerName ) - slices.begin();
		Reel wheel;
		wheel.entries  = std::move( slices );
		wheel.distance = index + std::max( 2LL, distanceFor( spin ) / count ) * count;
		wheel.stop     = spin;
		seg.reels.push_back( std::move( wheel ) );
	}
	else if( animation == Animation::Slot )
	{
		// Digits roll through 0 to 9, names through the letters of the list.
		std::vector< std::string > alphabet;
		if( numbers )
			for( char d = '0'; d <= '9'; ++d )
				alphabet.push_back( std::string( 1, d ) );
		else
		{
			std::set< std::string > letters;
			for( const std::string& entry : pool )
				for( const std::string& c : SplitCharacters( ItemName( entry ) ) )
					if( c != " " )
						letters.insert( c );
			alphabet.assign( letters.begin(), letters.end() );
			if( alphabet.size() < 2 )
				for( char c = 'A'; c <= 'Z'; ++c )
					alphabet.push_back( std::string( 1, c ) );
		}
		auto randomCharacter = [ & ]() { return alphabet[ std::uniform_int_distribution< size_t >( 0, alphabet.size() - 1 )( rng ) ]; };

		const std::vector< std::string > characters = SplitCharacters( winnerName );
		// Spaces, and signs in numbers, stand still; the others stop one after another from the left.
		auto still = [ & ]( const std::string& c ) {
			return c == " " || ( numbers && std::find( alphabet.begin(), alphabet.end(), c ) == alphabet.end() );
		};
		const size_t moving = (size_t)std::count_if( characters.begin(), characters.end(), [ & ]( const std::string& c ) { return !still( c ); } );
		size_t order        = 0;
		for( const std::string& c : characters )
		{
			if( still( c ) )
			{
				seg.reels.push_back( makeReel( c, 0, 0.0, false, [ & ]() { return c; } ) );
				continue;
			}
			const double stop = spin * ( 0.45 + 0.55 * (double)( ++order ) / (double)moving );
			seg.reels.push_back( makeReel( c, distanceFor( stop ), stop, true, randomCharacter ) );
		}
	}
	else
		seg.reels.push_back( makeReel( winnerName, distanceFor( spin ), spin, canVary, randomEntry ) );
	return seg;
}

bool Channel::Start( double now, bool test )
{
	Run& r = test ? testRun : run;
	if( r.phase == Phase::Spinning )
		return false;

	const size_t wanted = (size_t)config.drawCount;
	std::vector< std::string > winners, pool;
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
		// Several winners of one draw are always different lines of the list.
		std::vector< size_t > order( available.size() );
		for( size_t i = 0; i < order.size(); ++i )
			order[ i ] = i;
		std::shuffle( order.begin(), order.end(), rng );
		for( size_t i = 0; i < std::min( wanted, order.size() ); ++i )
			winners.push_back( available[ order[ i ] ] );
		// The reel shows what can still come out, unless that is too little to make an animation.
		pool = available.size() >= 2 ? available : items;
	}
	else
	{
		for( size_t i = 0; i < wanted; ++i )
		{
			long long value = 0;
			if( !PickAvailableNumber( value, winners ) )
				break;
			winners.push_back( FormatNumber( value ) );
		}
		if( winners.empty() )
		{
			if( !test )
			{
				message = "Todos os números já foram sorteados.";
				Touch();
				return false;
			}
			winners.push_back( FormatNumber( std::uniform_int_distribution< long long >( config.numMin, config.numMax )( rng ) ) );
		}
	}
	message.clear();
	if( winners.size() < wanted && !test )
		message = "Só restavam " + std::to_string( winners.size() ) + " para sortear.";

	r.segments.clear();
	r.animation = config.animation;
	r.reveal    = config.reveal == 1 ? Reveal::Grid : Reveal::OneByOne;
	r.revealGap = config.revealGap;
	r.decel     = config.decel;
	const double first = now + config.countdown;
	const size_t count = winners.size();
	for( size_t i = 0; i < count; ++i )
	{
		if( count > 1 && r.reveal == Reveal::Grid )
		{
			// All spin together in place, each in its cell, and they stop one after another.
			const double spin = config.duration * ( 0.55 + 0.45 * (double)( i + 1 ) / (double)count );
			r.segments.push_back( BuildSegment( winners[ i ], pool, Animation::Flip, first, spin ) );
		}
		else
			r.segments.push_back( BuildSegment( winners[ i ], pool, r.animation, first + i * ( config.duration + r.revealGap ), config.duration ) );
	}
	r.serial   = ++serial;
	r.seed     = (uint32_t)rng();
	r.recorded = 0;
	r.phase    = Phase::Spinning;
	Touch();
	return true;
}

bool Channel::ShowBoard( double now, bool test )
{
	Run& r = test ? testRun : run;
	if( r.phase == Phase::Spinning )
		return false;
	r.phase     = Phase::Board;
	r.boardTime = now;
	message.clear();
	Touch();
	return true;
}

double Run::EndTime() const
{
	double end = 0.0;
	for( const Segment& seg : segments )
		end = std::max( end, seg.Landing() );
	return end;
}

const std::string& Channel::GetWinner( bool test ) const
{
	static const std::string none;
	const Run& r = GetRun( test );
	return r.segments.size() == 1 ? r.segments.front().winner : none;
}

void Channel::Tick( double now )
{
	for( bool test : { false, true } )
	{
		Run& r = test ? testRun : run;
		if( r.phase == Phase::Spinning )
		{
			// Every winner goes to the history the moment it lands.
			while( r.recorded < r.segments.size() && now >= r.segments[ r.recorded ].Landing() )
			{
				if( !test )
				{
					history.push_back( { r.segments[ r.recorded ].winner, (int64_t)std::time( nullptr ) } );
					resetHistory.clear();
				}
				++r.recorded;
				Touch();
			}
			if( now >= r.EndTime() )
			{
				r.phase = Phase::Result;
				Touch();
			}
		}
		const double end = r.FinalTime() + config.autoClear;
		if( r.phase == Phase::Result && config.autoClear > 0.0f && now >= end )
		{
			r.phase     = Phase::Idle;
			r.clearTime = end;
			Touch();
		}
	}
}

void Channel::Clear( double now, bool test )
{
	Run& r = test ? testRun : run;
	if( r.phase != Phase::Idle )
		r.clearTime = now;
	r.phase = Phase::Idle;
	message.clear();
	Touch();
}

void Channel::ResetHistory()
{
	// Kept for UndoReset, unless there is nothing to keep (a second reset must not lose the first one).
	if( !history.empty() )
		resetHistory = std::move( history );
	history.clear();
	message.clear();
	Touch();
}

bool Channel::UndoReset()
{
	if( !CanUndoReset() )
		return false;
	history = std::move( resetHistory );
	resetHistory.clear();
	Touch();
	return true;
}

bool Channel::Redraw( double now )
{
	if( run.phase != Phase::Result || run.segments.size() != 1 || history.empty() || history.back().text != run.segments[ 0 ].winner ||
	    history.back().absent )
	{
		message = "Só dá para sortear de novo com um único ganhador na tela.";
		Touch();
		return false;
	}
	history.back().absent = true;
	Touch();
	return Start( now );
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
	{
		const double length = r.EndTime() - r.StartTime();
		return length > 0.0 ? Clamp01( ( now - r.StartTime() ) / length ) : 0.0;
	}
	case Phase::Result:
		return 1.0;
	default:
		return 0.0;
	}
}

namespace
{
double ReelPosition( const Reel& reel, double sinceStart, double decel )
{
	if( reel.stop <= 0.0 )
		return (double)reel.distance;
	return (double)reel.distance * ( 1.0 - std::pow( 1.0 - Clamp01( sinceStart / reel.stop ), decel ) );
}

const std::string& ReelAt( const Reel& reel, long long index )
{
	long long i = std::min( (long long)reel.entries.size() - 1, std::max( 0LL, index + kReelPad ) );
	return reel.entries[ (size_t)i ];
}
}//namespace

nlohmann::json HistoryToJson( const std::vector< HistoryEntry >& history )
{
	nlohmann::json out = nlohmann::json::array();
	for( const HistoryEntry& h : history )
		out.push_back( { { "name", h.text }, { "time", h.time }, { "absent", h.absent } } );
	return out;
}

FrameView Channel::BuildFrame( double now, bool test ) const
{
	const Run& r = GetRun( test );
	FrameView frame;
	frame.config    = config;
	frame.phase     = r.phase;
	frame.seed      = r.seed;
	frame.animation = r.phase == Phase::Spinning || r.phase == Phase::Result ? r.animation : config.animation;
	const int k     = config.neighbors;

	if( r.phase == Phase::Idle )
	{
		frame.fade = (float)Clamp01( ( now - r.clearTime ) / kIdleFadeInTime );
		if( !config.idleText.empty() )
			frame.items.push_back( { config.idleText, 0.0f, frame.fade } );
		return frame;
	}
	if( r.phase == Phase::Board )
	{
		frame.board = true;
		frame.fade  = (float)Clamp01( ( now - r.boardTime ) / kIdleFadeInTime );
		for( const HistoryEntry& h : history )
			if( !h.absent )
				frame.cells.push_back( { ItemName( h.text ), ItemName( h.text ), 0.0f, 1e3f } );
		return frame;
	}
	if( r.segments.empty() )
		return frame;

	if( r.phase == Phase::Result && config.autoClear > 0.0f )
		frame.fade = 1.0f - (float)Clamp01( ( now - r.FinalTime() - ( config.autoClear - kResultFadeOutTime ) ) / kResultFadeOutTime );

	// "3, 2, 1": each number pops in big and shrinks, fading out at the end of its second.
	const double first = r.StartTime();
	if( now < first )
	{
		const double left   = first - now;
		const double number = std::ceil( left - 1e-9 );
		const double part   = left - ( number - 1.0 );// 1 down to 0 within each second
		VisibleItem item;
		item.text      = std::to_string( (int)number );
		item.winnerMix = 1.0f;
		item.scale     = 0.85f + 0.45f * (float)std::pow( part, 4.0 );
		item.alpha     = (float)Clamp01( part / 0.2 );
		frame.countdown = true;
		frame.items.push_back( item );
		return frame;
	}

	if( r.IsGrid() )
	{
		for( const Segment& seg : r.segments )
		{
			CellView cell;
			cell.layoutText = ItemName( seg.winner );
			if( now >= seg.Landing() )
			{
				cell.text        = ItemName( seg.winner );
				cell.sinceLanded = (float)( now - seg.Landing() );
				cell.winnerMix   = (float)Clamp01( cell.sinceLanded / kHighlightTime );
			}
			else
				cell.text = ReelAt( seg.reels.front(), std::llround( ReelPosition( seg.reels.front(), now - seg.start, r.decel ) ) );
			frame.cells.push_back( cell );
		}
		if( now >= r.EndTime() )
		{
			frame.landed      = true;
			frame.sinceResult = (float)( now - r.EndTime() );
		}
		return frame;
	}

	// One winner at a time: the one on screen is the last that started.
	size_t current = 0;
	while( current + 1 < r.segments.size() && now >= r.segments[ current + 1 ].start )
		++current;
	if( r.segments.size() > 1 && now >= r.FinalTime() )
	{
		// After the last one had its moment alone, everybody together.
		for( const Segment& seg : r.segments )
			frame.cells.push_back( { ItemName( seg.winner ), ItemName( seg.winner ), 1.0f, (float)( now - seg.Landing() ) } );
		frame.fade = std::min( frame.fade, (float)Clamp01( ( now - r.FinalTime() ) / kIdleFadeInTime ) );
		return frame;
	}
	for( size_t i = 0; i < current; ++i )
		frame.tray.push_back( ItemName( r.segments[ i ].winner ) );

	const Segment& seg        = r.segments[ current ];
	const Reel& reel          = seg.reels.front();
	const double since        = now - seg.start;
	const double sinceLanding = now - seg.Landing();
	if( r.animation == Animation::Wheel )
	{
		const double slice      = 2.0 * kPi / (double)reel.entries.size();
		frame.wheel.entries     = reel.entries;
		frame.wheel.winnerIndex = (int)( reel.distance % (long long)reel.entries.size() );
		frame.wheel.rotation    = (float)( ReelPosition( reel, since, r.decel ) * slice );
		frame.wheel.highlight   = sinceLanding < 0.0 ? 0.0f : (float)Clamp01( sinceLanding / kHighlightTime );
		// Once it stops, the wheel steps back so the name in front of it reads well.
		frame.wheel.alpha = ( sinceLanding < 0.0 ? 1.0f : 1.0f - 0.7f * (float)Clamp01( sinceLanding / kNeighborFadeOutTime ) ) * frame.fade;
	}

	if( sinceLanding < 0.0 )
	{
		if( r.animation == Animation::Slot )
		{
			for( const Reel& column : seg.reels )
			{
				ColumnView view;
				view.finalText = column.entries[ (size_t)( kReelPad + column.distance ) ];
				if( since >= column.stop )
					view.items.push_back( { view.finalText, 0.0f, 1.0f, 1.0f, (float)Clamp01( ( since - column.stop ) / kHighlightTime ) } );
				else
				{
					const double position = ReelPosition( column, since, r.decel );
					const long long base  = (long long)std::floor( position );
					for( long long i = base - 1; i <= base + 2; ++i )
					{
						const float offset = (float)( i - position );
						const float alpha  = 1.0f - std::fabs( offset ) / 1.15f;
						if( alpha > 0.0f )
							view.items.push_back( { ReelAt( column, i ), offset, alpha } );
					}
				}
				frame.columns.push_back( std::move( view ) );
			}
		}
		else if( r.animation == Animation::Flip )
			frame.items.push_back( { ReelAt( reel, std::llround( ReelPosition( reel, since, r.decel ) ) ) } );
		else if( r.animation != Animation::Wheel )
		{
			const double position = ReelPosition( reel, since, r.decel );
			const long long base  = (long long)std::floor( position );
			for( long long i = base - k - 1; i <= base + k + 1; ++i )
			{
				const float offset = (float)( i - position );
				const float alpha  = 1.0f - std::fabs( offset ) / ( k + 1 );
				if( alpha > 0.0f )
					frame.items.push_back( { ReelAt( reel, i ), offset, alpha } );
			}
		}
		return frame;
	}

	// Landed: the winner alone in the center, with its effects.
	frame.landed      = true;
	frame.sinceResult = (float)sinceLanding;
	frame.detail      = ItemDetail( seg.winner );
	VisibleItem item;
	item.text      = ItemName( seg.winner );
	item.alpha     = frame.fade;
	item.winnerMix = (float)Clamp01( sinceLanding / kHighlightTime );
	item.scale     = 1.0f + 0.18f * (float)std::sin( kPi * Clamp01( sinceLanding / kPulseTime ) );
	if( r.animation == Animation::Vertical || r.animation == Animation::Horizontal )
	{
		const float fade = 1.0f - (float)Clamp01( sinceLanding / kNeighborFadeOutTime );
		for( long long d = -k - 1; fade > 0.0f && d <= k + 1; ++d )
		{
			const float alpha = ( 1.0f - std::fabs( (float)d ) / ( k + 1 ) ) * fade;
			if( d != 0 && alpha > 0.0f )
				frame.items.push_back( { ReelAt( reel, reel.distance + d ), (float)d, alpha } );
		}
	}
	frame.items.push_back( item );
	return frame;
}

}//namespace sorteador
