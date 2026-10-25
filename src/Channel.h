#pragma once
#include "Config.h"
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace sorteador
{
enum class Phase : int
{
	Idle     = 0,
	Spinning = 1,
	Result   = 2
};

// One piece of text the renderer has to draw this frame.
struct VisibleItem
{
	std::string text;
	float offset    = 0.0f;// distance from the center slot, in items
	float alpha     = 1.0f;
	float scale     = 1.0f;
	float winnerMix = 0.0f;// 0 = text color, 1 = winner color
};

struct FrameView
{
	DrawConfig config;
	Phase phase = Phase::Idle;
	std::vector< VisibleItem > items;
	float sinceResult = -1.0f;// seconds since the winner landed, negative before that
	uint32_t seed     = 0;    // changes on every draw, so the effects never repeat exactly
};

// The reel has this many extra entries before and after the visible part, so neighbors are always available.
constexpr long long kReelPad = kMaxNeighbors + 2;

// One draw: the reel the animation scrolls through and where it stops.
struct Run
{
	uint64_t serial = 0;// changes on every start, so the web panel knows when to fetch the new reel
	Phase phase     = Phase::Idle;
	std::string winner;
	// reel[ reelPad + distance ] is the winner.
	std::vector< std::string > reel;
	long long distance = 0;
	double startTime   = 0.0;
	double spinTime    = 0.0;
	double decel       = 3.0;
	uint32_t seed      = 0;
};

struct HistoryEntry
{
	std::string text;
	int64_t time = 0;// when it was drawn, in seconds since 1970 (UTC); 0 for entries saved before 1.2
};

// A drawing channel: its settings, the running draw and the history of winners. Next to the real draw it keeps a
// test draw, which only previews show and which never touches the history.
// Not thread safe on its own, ChannelManager guards every access with its mutex.
class Channel
{
public:
	Channel();

	DrawConfig config;

	// Starts a draw. Returns false (and sets the message) if there is nothing left to draw.
	bool Start( double now, bool test = false );
	// Advances the state machine, moving a finished spin into the result phase.
	void Tick( double now );
	void Clear( bool test = false );
	void ResetHistory();
	void RemoveHistory( size_t index );

	FrameView BuildFrame( double now, bool test = false ) const;

	std::vector< std::string > Items() const;
	// How many entries can still be drawn, honoring the no-repeat setting.
	size_t AvailableCount() const;

	const Run& GetRun( bool test = false ) const { return test ? testRun : run; }
	Phase GetPhase( bool test = false ) const { return GetRun( test ).phase; }
	const std::string& GetWinner( bool test = false ) const { return GetRun( test ).winner; }
	const std::string& GetMessage() const { return message; }
	const std::vector< HistoryEntry >& GetHistory() const { return history; }
	void SetHistory( std::vector< HistoryEntry > newHistory );
	double Progress( double now, bool test = false ) const;

	// Bumped on every change, so pollers and Resolume parameters know when to refresh.
	uint64_t revision       = 1;
	uint64_t configRevision = 1;
	void Touch() { ++revision; }
	void TouchConfig() { ++revision; ++configRevision; }

private:
	std::string FormatNumber( long long value ) const;
	std::vector< std::string > AvailableItems() const;
	bool PickAvailableNumber( long long& out );
	std::mt19937_64 rng;
	std::string message;
	std::vector< HistoryEntry > history;
	uint64_t serial = 0;
	Run run;
	Run testRun;
};

// [ { "name": ..., "time": ... } ], the format of the saved state and of the web API.
nlohmann::json HistoryToJson( const std::vector< HistoryEntry >& history );

}//namespace sorteador
