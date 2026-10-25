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
	Flip       = 2
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

	// How it moves
	Animation animation = Animation::Vertical;
	float duration      = 6.0f; // seconds
	float speed         = 25.0f;// items per second at the start of the draw
	float decel         = 3.0f; // easing exponent, higher = longer slow down
	int neighbors       = 2;    // items visible above/below (or left/right) the center one

	// How it looks
	std::string fontPath;// empty = system default
	float textSize     = 0.16f;// fraction of output height
	float posY         = 0.5f; // 0 = bottom, 1 = top
	Color textColor    = { 1.0f, 1.0f, 1.0f };
	Color winnerColor  = { 1.0f, 0.82f, 0.2f };
	Color outlineColor = { 0.0f, 0.0f, 0.0f };
	float outline      = 0.3f;
	Color bgColor      = { 0.0f, 0.0f, 0.0f };
	float bgOpacity    = 0.0f;
	std::string title;
	std::string idleText = "SORTEIO";
	bool showAddress     = true;
};

constexpr int kMaxNeighbors = 4;

// Clamps all values into their valid ranges.
void Sanitize( DrawConfig& config );

nlohmann::json ToJson( const DrawConfig& config );
// Only keys present in the json are applied, so the web panel can send partial updates.
void MergeJson( DrawConfig& config, const nlohmann::json& json );

}//namespace sorteador
