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
	Spinning = 1,// also the countdown before it, and the pauses between winners revealed one by one
	Result   = 2,
	Board    = 3 // the winners so far, on screen
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

// One character column of the slot machine while it spins.
struct ColumnView
{
	std::string finalText;// the character it stops on, which sets the column width
	std::vector< VisibleItem > items;
};

// One name in a grid: several winners at once, the summary after them, or the winners board.
struct CellView
{
	std::string text;
	std::string layoutText;// what it stops on, so the grid keeps its size while the names roll
	float winnerMix   = 0.0f;
	float sinceLanded = -1.0f;// negative while it still spins
};

struct WheelView
{
	std::vector< std::string > entries;// the slices, clockwise from the top
	float rotation  = 0.0f;            // radians turned so far
	float alpha     = 0.0f;            // 0 = no wheel this frame
	int winnerIndex = -1;
	float highlight = 0.0f;// 0..1 once it stopped: the other slices dim
};

struct FrameView
{
	DrawConfig config;
	Phase phase         = Phase::Idle;
	Animation animation = Animation::Vertical;// of the draw on screen, which may differ from the settings changed since
	std::vector< VisibleItem > items;// reel, winner, waiting text or countdown number, in the center
	bool landed       = false;// a winner just stopped (in items, or the last one of a grid): effects play
	float sinceResult = -1.0f;// seconds since it stopped
	bool countdown    = false;// items holds a countdown number
	std::vector< ColumnView > columns;// slot machine
	std::vector< CellView > cells;    // grid of names
	bool board = false;               // cells are the winners board
	std::vector< std::string > tray;  // winners already revealed, when they come one by one
	WheelView wheel;
	std::string detail;// second line under the winner, from "name | detail" list entries
	float fade    = 1.0f;// everything but the background and the title fades with this (result leaving, waiting text arriving)
	uint32_t seed = 0;   // changes on every draw, so the effects never repeat exactly
};

// The reel has this many extra entries before and after the visible part, so neighbors are always available.
constexpr long long kReelPad = kMaxNeighbors + 2;

// One strip that spins and stops. reels: entries[ kReelPad + distance ] is where it stops. Wheel: entries are the
// slices and distance counts slices turned.
struct Reel
{
	std::vector< std::string > entries;
	long long distance = 0;
	double stop        = 0.0;// seconds after the segment start when it lands
};

// One winner being revealed: a single reel, a wheel, or one reel per character for the slot machine.
struct Segment
{
	std::string winner;// whole list line
	double start = 0.0;
	double spin  = 0.0;// seconds until the last of its reels lands
	std::vector< Reel > reels;
	double Landing() const { return start + spin; }
};

enum class Reveal : int
{
	OneByOne = 0,
	Grid     = 1
};

// One draw: what it shows and when every winner lands. The parts of the settings that shape it are kept here, so
// changing them halfway through never breaks a running draw.
struct Run
{
	uint64_t serial = 0;// changes on every start, so the web panel knows when to fetch the reels
	Phase phase     = Phase::Idle;
	std::vector< Segment > segments;
	Animation animation = Animation::Vertical;
	Reveal reveal       = Reveal::OneByOne;
	double revealGap    = 3.0;
	double decel        = 3.0;
	uint32_t seed       = 0;
	size_t recorded     = 0;    // winners already written to the history
	double clearTime    = -1e9;// when it last went back to the waiting screen, for the fade in
	double boardTime    = 0.0; // when the winners board came up

	double StartTime() const { return segments.empty() ? 0.0 : segments.front().start; }
	double EndTime() const;
	bool IsGrid() const { return segments.size() > 1 && reveal == Reveal::Grid; }
	// When the last thing a draw reveals is on screen: the last winner, or the summary after them one by one.
	double FinalTime() const { return EndTime() + ( segments.size() > 1 && !IsGrid() ? revealGap : 0.0 ); }
};

// A list line is "name" or "name | detail". The reel shows names, the detail shows under the winner.
std::string ItemName( const std::string& item );
std::string ItemDetail( const std::string& item );

struct HistoryEntry
{
	std::string text;
	int64_t time = 0;// when it was drawn, in seconds since 1970 (UTC); 0 for entries saved before 1.2
	bool absent  = false;// drawn but not present: stays in the record and keeps counting as drawn
};

// A drawing channel: its settings, the running draw and the history of winners. Next to the real draw it keeps a
// test draw, which only previews show and which never touches the history.
// Not thread safe on its own, ChannelManager guards every access with its mutex.
class Channel
{
public:
	Channel();

	DrawConfig config;

	// Starts a draw of config.drawCount winners. Returns false (and sets the message) if there is nothing left to draw.
	bool Start( double now, bool test = false );
	// Shows the winners so far on screen, until the next Clear or Start.
	bool ShowBoard( double now, bool test = false );
	// Advances the state machine, moving a finished spin into the result phase.
	void Tick( double now );
	void Clear( double now, bool test = false );
	void ResetHistory();
	// Brings back the history cleared by the last ResetHistory, if nothing was drawn since.
	bool UndoReset();
	bool CanUndoReset() const { return !resetHistory.empty() && history.empty(); }
	void RemoveHistory( size_t index );
	// The winner on screen is not present: marks them absent and starts a new draw right away.
	bool Redraw( double now );

	FrameView BuildFrame( double now, bool test = false ) const;

	std::vector< std::string > Items() const;
	// How many entries can still be drawn, honoring the no-repeat setting.
	size_t AvailableCount() const;

	const Run& GetRun( bool test = false ) const { return test ? testRun : run; }
	Phase GetPhase( bool test = false ) const { return GetRun( test ).phase; }
	// The winner of a single draw, empty when several were drawn at once.
	const std::string& GetWinner( bool test = false ) const;
	const std::string& GetMessage() const { return message; }
	const std::vector< HistoryEntry >& GetHistory() const { return history; }
	void SetHistory( std::vector< HistoryEntry > newHistory );
	double Progress( double now, bool test = false ) const;

	// Bumped on every change, so pollers and Resolume parameters know when to refresh.
	uint64_t revision       = 1;
	uint64_t configRevision = 1;
	// Web panels may only run draws, not change settings or clear the history. Resolume itself is not affected.
	bool locked = false;
	std::string name;// shown instead of "Canal N" when set
	void Touch() { ++revision; }
	void TouchConfig() { ++revision; ++configRevision; }

private:
	std::string FormatNumber( long long value ) const;
	std::vector< std::string > AvailableItems() const;
	bool PickAvailableNumber( long long& out, const std::vector< std::string >& alsoTaken );
	Segment BuildSegment( const std::string& winner, const std::vector< std::string >& pool, Animation animation, double start, double spin );
	std::mt19937_64 rng;
	std::string message;
	std::vector< HistoryEntry > history;
	std::vector< HistoryEntry > resetHistory;
	uint64_t serial = 0;
	Run run;
	Run testRun;
};

// [ { "name": ..., "time": ..., "absent": ... } ], the format of the saved state and of the web API.
nlohmann::json HistoryToJson( const std::vector< HistoryEntry >& history );

}//namespace sorteador
