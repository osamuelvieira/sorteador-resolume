#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace sorteador
{
enum class DrawMode : int
{
	List    = 0,
	Numbers = 1
};

enum class Animation : int
{
	Vertical   = 0,
	Horizontal = 1,
	Flip       = 2,
	Wheel      = 3,// wheel of fortune
	Slot       = 4 // slot machine: every character spins on its own and they stop left to right
};

struct Color
{
	float r = 1.0f;
	float g = 1.0f;
	float b = 1.0f;
};

// Every setting of a drawing channel. The web panel and the Resolume parameters are two views of this struct.
struct DrawConfig
{
	// What is being drawn
	DrawMode mode        = DrawMode::List;
	std::string listText = "Ana\nBruno\nCarla\nDaniel\nEduarda\nFelipe\nGabriela\nHenrique";
	int numMin           = 1;
	int numMax           = 100;
	bool zeroPad         = false;
	bool noRepeat        = true;
	int drawCount        = 1;    // winners per draw
	int reveal           = 0;    // with several winners: 0 = one by one, 1 = all at once in a grid
	float revealGap      = 3.0f; // seconds each winner stays alone before the next one, one by one
	std::string boardTitle = "Ganhadores";// heading of the winners board

	// How it moves
	Animation animation = Animation::Vertical;
	float duration      = 6.0f; // whole seconds
	float speed         = 25.0f;// items per second at the start of the draw
	float decel         = 3.0f; // easing exponent (whole number), higher = longer slow down
	int neighbors       = 2;    // items visible above/below (or left/right) the center one
	bool drum           = true; // vertical reel curves away like a drum instead of a flat list
	float autoClear     = 0.0f; // seconds the result stays before going back to the waiting screen, 0 = never
	float countdown     = 0.0f; // "3, 2, 1" before the spin, in whole seconds, 0 = none

	// How it looks
	std::string fontPath;// empty = system default
	float textSize     = 0.16f;// fraction of output height
	float posY         = 0.5f; // 0 = bottom, 1 = top
	Color textColor    = { 1.0f, 1.0f, 1.0f };
	Color winnerColor  = { 1.0f, 1.0f, 1.0f };
	Color outlineColor = { 0.0f, 0.0f, 0.0f };
	float outline      = 0.3f;
	Color bgColor      = { 0.0f, 0.0f, 0.0f };
	float bgOpacity    = 0.0f;
	bool wrapLongNames = true;// long names use two lines before they are shrunk
	std::string title;
	std::string titleFontPath;// empty = same font as the names
	float titleSize      = 0.07f;// fraction of output height
	float titlePosY      = 0.88f;// 0 = bottom, 1 = top
	Color titleColor     = { 1.0f, 1.0f, 1.0f };
	std::string idleText = "SORTEIO";
	float idleSize       = 0.16f;
	Color idleColor      = { 1.0f, 1.0f, 1.0f };
	bool uppercase       = false;// shows every text on screen in capitals
	bool showAddress     = false;

	// Effects when the winner lands
	bool fxImpact      = true; // zoom punch with a short shake
	bool fxFlash       = true; // full screen flash
	bool fxShockwave   = false;// ring expanding from the name
	bool fxSparks      = true; // sparks bursting from the name
	bool fxConfetti    = false;// confetti raining from the top
	bool fxGlow        = false;// pulsing glow around the name
	float fxIntensity  = 1.0f;
};

constexpr int kMaxNeighbors = 4;
constexpr int kMaxDrawCount = 20;
// Winner color used before 1.2, replaced by white when an old saved state is loaded.
constexpr Color kOldWinnerColor = { 1.0f, 0.82f, 0.2f };

// Before 1.3 the title and the waiting text followed the names; keeps that look for settings saved back then.
void MigrateTo13( DrawConfig& config );

// Clamps all values into their valid ranges.
void Sanitize( DrawConfig& config );

nlohmann::json ToJson( const DrawConfig& config );
// Only keys present in the json are applied, so the web panel can send partial updates.
void MergeJson( DrawConfig& config, const nlohmann::json& json );

}//namespace sorteador
