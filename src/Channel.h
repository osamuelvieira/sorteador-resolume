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
};

// A drawing channel: its settings, the running draw and the history of winners.
// Not thread safe on its own, ChannelManager guards every access with its mutex.
class Channel
{
public:
	Channel();

	DrawConfig config;

	// Starts a draw. Returns false (and sets the message) if there is nothing left to draw.
	bool Start( double now );
	// Advances the state machine, moving a finished spin into the result phase.
	void Tick( double now );
	void Clear();
	void ResetHistory();
	void RemoveHistory( size_t index );

	FrameView BuildFrame( double now ) const;

	std::vector< std::string > Items() const;
	// How many entries can still be drawn, honoring the no-repeat setting.
	size_t AvailableCount() const;

	Phase GetPhase() const { return phase; }
	const std::string& GetWinner() const { return winner; }
	const std::string& GetMessage() const { return message; }
	const std::vector< std::string >& GetHistory() const { return history; }
	void SetHistory( std::vector< std::string > newHistory );
	double Progress( double now ) const;

	// Bumped on every change, so pollers and Resolume parameters know when to refresh.
	uint64_t revision       = 1;
	uint64_t configRevision = 1;
	void Touch() { ++revision; }
	void TouchConfig() { ++revision; ++configRevision; }

private:
	std::string FormatNumber( long long value ) const;
	std::vector< std::string > AvailableItems() const;
	bool PickAvailableNumber( long long& out );
	const std::string& ReelAt( long long index ) const;
	double ReelPosition( double now ) const;

	std::mt19937_64 rng;
	Phase phase = Phase::Idle;
	std::string winner;
	std::string message;
	std::vector< std::string > history;

	// The reel is the sequence of texts the animation scrolls through; reel[ reelPad + distance ] is the winner.
	std::vector< std::string > reel;
	long long distance = 0;
	double startTime   = 0.0;
	double spinTime    = 0.0;
	double decel       = 3.0;
};

}//namespace sorteador
